/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Redfish server for the Ecclesia Management Agent on Indus
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/memory/memory.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "ecclesia/lib/apifs/apifs.h"
#include "ecclesia/lib/io/ioctl.h"
#include "ecclesia/lib/io/pci/discovery.h"
#include "ecclesia/lib/io/pci/location.h"
#include "ecclesia/lib/io/smbus/kernel_dev.h"
#include "ecclesia/lib/io/smbus/smbus.h"
#include "ecclesia/lib/logging/file_globals.h"
#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/lib/smbios/indus/platform_translator.h"
#include "ecclesia/lib/types/fixed_range_int.h"
#include "ecclesia/magent/daemons/common.h"
#include "ecclesia/magent/lib/eeprom/eeprom.h"
#include "ecclesia/magent/lib/eeprom/smbus_eeprom.h"
#include "ecclesia/magent/lib/ipmi/interface_options.h"
#include "ecclesia/magent/lib/ipmi/ipmi.h"
#include "ecclesia/magent/lib/ipmi/ipmitool.h"
#include "ecclesia/magent/redfish/indus/redfish_service.h"
#include "ecclesia/magent/sysmodel/indus/nvme.h"
#include "ecclesia/magent/sysmodel/x86/fru.h"
#include "ecclesia/magent/sysmodel/x86/sysmodel.h"
#include "ecclesia/magent/sysmodel/x86/thermal.h"
#include "tensorflow_serving/util/net_http/server/public/httpserver_interface.h"
#include "tensorflow_serving/util/net_http/server/public/response_code_enum.h"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"

ABSL_FLAG(std::string, mced_socket_path, "/var/run/mced2.socket",
          "Path to the mced unix domain socket");
ABSL_FLAG(
    std::string, mobo_raw_fru_path, "",
    "Path to a file containing the raw EEPROM dump of the motherbord FRU. If "
    "left empty, magent will read the EEPROM directly via SMBUS.");

ABSL_FLAG(std::string, odata_metadata_file_path,
          "/etc/google/magent/metadata/index.xml",
          "Path to a file containing the OData metadata document.");

ABSL_FLAG(std::string, system_event_clear_script_path,
          "",
          "Path to an script that clears the system events.");

namespace {

using ::tensorflow::serving::net_http::HTTPServerInterface;

constexpr char kSmbiosEntryPointPath[] =
    "/sys/firmware/dmi/tables/smbios_entry_point";
constexpr char kSmbiosTablesPath[] = "/sys/firmware/dmi/tables/DMI";
constexpr char kSysfsMemFilePath[] = "/dev/mem";
constexpr char kMagentConfigPath[] = "/etc/google/magent/config.pb";

constexpr char kI2cDevicePathPrefix[] = "/sys/bus/i2c/devices/";
// smbus_ipass_fru_mux (defined in indus board file) is at address 0x77.
constexpr char kI2cDevicePathSuffixMobo[] = "-0077/channel-2";
// smbus_pcie_mux (defined in indus board file) is at address 0x75.
// Spicy16 interposer card at PE2
constexpr char kI2cDevicePathSuffixSpicy16Intp0[] = "-0075/channel-2";
// Spicy16 interposer card at PE3
constexpr char kI2cDevicePathSuffixSpicy16Intp4[] = "-0075/channel-3";

// Indus machine specific settings.
// Please refer to indus spec for more infomation.
// It's less brittle to read a hardcoded pci address(0000:00:1f.4) on indus
// machine then always use a hardcoded smbus address. For this to work, we
// need kernel module i2c_mux_pca954x to be loaded.
constexpr absl::string_view kSmbusPciAddressPath =
    "/sys/bus/pci/devices/0000:00:1f.4/";
absl::optional<ecclesia::SmbusBus> GetEepromSmbusBus(
    absl::string_view i2c_device_path_suffix) {
  ecclesia::ApifsDirectory dir(std::string{kSmbusPciAddressPath});
  auto maybe_entries = dir.ListEntryPaths();
  if (!maybe_entries.ok()) {
    return absl::nullopt;
  }

  // We need to find the pci bus number.
  // Search for a pattern "i2c-\d+".
  int pci_bus = -1;
  // int len = sizeof(kSmbusPciAddressPath) + 4;
  for (absl::string_view f : *maybe_entries) {
    f.remove_prefix(kSmbusPciAddressPath.size());
    if (absl::StartsWith(f, "i2c-") &&
        absl::SimpleAtoi(f.substr(4), &pci_bus)) {
      break;
    }
  }

  if (pci_bus == -1 || pci_bus > ecclesia::SmbusBus::kMaxValue) {
    return absl::nullopt;
  }

  ecclesia::ApifsFile fs(
      absl::StrCat(kI2cDevicePathPrefix, pci_bus, i2c_device_path_suffix));
  absl::StatusOr<std::string> maybe_link = fs.ReadLink();
  if (maybe_link.ok()) {
    int smbus_id;
    // link will have the format like : "../i2c-37".
    if (absl::SimpleAtoi(maybe_link->substr(maybe_link->find_last_of('-') + 1),
                         &smbus_id)) {
      return ecclesia::SmbusBus::TryMake(smbus_id);
    }
  }
  return absl::nullopt;
}

// We will read i2c bus offset 0x55 to get board information.
// Fru common header has 8 bytes.
// https://www.intel.com/content/dam/www/public/us/en/documents/product-briefs/platform-management-fru-document-rev-1-2-feb-2013.pdf
constexpr auto kEepromSmbusAddressMobo = ecclesia::SmbusAddress::Make<0x55>();

// The smbus address is sourced from spicy16 spec.
constexpr auto kEepromSmbusAddressSpicy16 =
    ecclesia::SmbusAddress::Make<0x51>();

// Info to Build DIMM thermal sensors using hardcoded infomation for SkylakeImc.
// Each entry is associated with a DIMM. Each sensor device can have up to 2
// DIMMs. The indices should coincide with the indices of DIMMs. The order of
// the devices (that are mapped to DIMM0, DIMM1, ..., DIMM23), according to the
// motherboard schematic mapping memory channels to silkscreen labels, is:
//
// - cpu0: 1lms3, 1lms4, 1lms5, 1lms2, 1lms1, 1lms0
// - cpu1: 1lms0, 1lms1, 1lms2, 1lms5, 1lms4, 1lms3
//
// Thermal info offset for channel0 is 0x150, for channel1 is 0x154.
constexpr ecclesia::PciSensorParams dimm_channel_info[]{
    {"dimm0",  // cpu0_1lms_3
     ecclesia::PciDbdfLocation::Make<0, 0x3a, 0x0c, 2>(), 0x150, 85},
    {"dimm1",  // cpu0_1lms_3
     ecclesia::PciDbdfLocation::Make<0, 0x3a, 0x0c, 2>(), 0x154, 85},
    {"dimm2",  // cpu0_1lms_4
     ecclesia::PciDbdfLocation::Make<0, 0x3a, 0x0c, 6>(), 0x150, 85},
    {"dimm3",  // cpu0_1lms_4
     ecclesia::PciDbdfLocation::Make<0, 0x3a, 0x0c, 6>(), 0x154, 85},
    {"dimm4",  // cpu0_1lms_5
     ecclesia::PciDbdfLocation::Make<0, 0x3a, 0x0d, 2>(), 0x150, 85},
    {"dimm5",  // cpu0_1lms_5
     ecclesia::PciDbdfLocation::Make<0, 0x3a, 0x0d, 2>(), 0x154, 85},
    {"dimm6",  // cpu0_1lms_2
     ecclesia::PciDbdfLocation::Make<0, 0x3a, 0x0b, 2>(), 0x154, 85},
    {"dimm7",  // cpu0_1lms_2
     ecclesia::PciDbdfLocation::Make<0, 0x3a, 0x0b, 2>(), 0x150, 85},
    {"dimm8",  // cpu0_1lms_1
     ecclesia::PciDbdfLocation::Make<0, 0x3a, 0x0a, 6>(), 0x154, 85},
    {"dimm9",  // cpu0_1lms_1
     ecclesia::PciDbdfLocation::Make<0, 0x3a, 0x0a, 6>(), 0x150, 85},
    {"dimm10",  // cpu0_1lms_0
     ecclesia::PciDbdfLocation::Make<0, 0x3a, 0x0a, 2>(), 0x154, 85},
    {"dimm11",  // cpu0_1lms_0
     ecclesia::PciDbdfLocation::Make<0, 0x3a, 0x0a, 2>(), 0x150, 85},

    {"dimm12",  // cpu1_1lms_0
     ecclesia::PciDbdfLocation::Make<0, 0xae, 0x0a, 2>(), 0x150, 85},
    {"dimm13",  // cpu1_1lms_0
     ecclesia::PciDbdfLocation::Make<0, 0xae, 0x0a, 2>(), 0x154, 85},
    {"dimm14",  // cpu1_1lms_1
     ecclesia::PciDbdfLocation::Make<0, 0xae, 0x0a, 6>(), 0x150, 85},
    {"dimm15",  // cpu1_1lms_1
     ecclesia::PciDbdfLocation::Make<0, 0xae, 0x0a, 6>(), 0x154, 85},
    {"dimm16",  // cpu1_1lms_2
     ecclesia::PciDbdfLocation::Make<0, 0xae, 0x0b, 2>(), 0x150, 85},
    {"dimm17",  // cpu1_1lms_2
     ecclesia::PciDbdfLocation::Make<0, 0xae, 0x0b, 2>(), 0x154, 85},
    {"dimm18",  // cpu1_1lms_5
     ecclesia::PciDbdfLocation::Make<0, 0xae, 0x0d, 2>(), 0x154, 85},
    {"dimm19",  // cpu1_1lms_5
     ecclesia::PciDbdfLocation::Make<0, 0xae, 0x0d, 2>(), 0x150, 85},
    {"dimm20",  // cpu1_1lms_4
     ecclesia::PciDbdfLocation::Make<0, 0xae, 0x0c, 6>(), 0x154, 85},
    {"dimm21",  // cpu1_1lms_4
     ecclesia::PciDbdfLocation::Make<0, 0xae, 0x0c, 6>(), 0x150, 85},
    {"dimm22",  // cpu1_1lms_3
     ecclesia::PciDbdfLocation::Make<0, 0xae, 0x0c, 2>(), 0x154, 85},
    {"dimm23",  // cpu1_1lms_3
     ecclesia::PciDbdfLocation::Make<0, 0xae, 0x0c, 2>(), 0x150, 85},
};

}  // namespace

int main(int argc, char** argv) {
  std::vector<char*> args = absl::ParseCommandLine(argc, argv);
  ecclesia::TrySetGlobalLoggerToFileLogger(args[0]);

  if (absl::GetFlag(FLAGS_assemblies_dir).empty()) {
    ecclesia::ErrorLog() << "a non-empty --assemblies_dir must be specified";
    return EXIT_FAILURE;
  }

  // Interfaces into the underlying platform.
  ecclesia::SysIoctl ioctl_intf;
  ecclesia::KernelSmbusAccess access("/dev", &ioctl_intf);

  std::vector<ecclesia::SmbusEeprom2ByteAddr::Option> eeprom_options;
  std::vector<ecclesia::SysmodelFruReaderFactory> fru_factories;
  if (!absl::GetFlag(FLAGS_mobo_raw_fru_path).empty()) {
    fru_factories.push_back(ecclesia::SysmodelFruReaderFactory(
        "motherboard",
        [&]() -> std::unique_ptr<ecclesia::SysmodelFruReaderIntf> {
          return absl::make_unique<ecclesia::FileSysmodelFruReader>(
              absl::GetFlag(FLAGS_mobo_raw_fru_path));
        }));
  } else {
    fru_factories.push_back(ecclesia::SysmodelFruReaderFactory(
        "motherboard",
        [&]() -> std::unique_ptr<ecclesia::SysmodelFruReaderIntf> {
          return absl::make_unique<ecclesia::SmbusEepromFruReader>(
              absl::make_unique<ecclesia::SmbusEeprom2ByteAddr>(
                  ecclesia::SmbusEeprom::Option{
                      .name = "motherboard",
                      .size = {.type = ecclesia::Eeprom::SizeType::kFixed,
                               .size = 8 * 1024},
                      .mode = {.readable = 1, .writable = 0},
                      .get_device =
                          [&]() -> absl::optional<ecclesia::SmbusDevice> {
                        auto eeprom_smbus_bus =
                            GetEepromSmbusBus(kI2cDevicePathSuffixMobo);
                        if (!eeprom_smbus_bus) return absl::nullopt;
                        ecclesia::SmbusLocation loc(*eeprom_smbus_bus,
                                                    kEepromSmbusAddressMobo);
                        ecclesia::SmbusDevice device(loc, &access);
                        return device;
                      }}));
        }));
  }

  fru_factories.push_back(ecclesia::SysmodelFruReaderFactory(
      "spicy16_intp0",
      [&]() -> std::unique_ptr<ecclesia::SysmodelFruReaderIntf> {
        return absl::make_unique<ecclesia::SmbusEepromFruReader>(
            absl::make_unique<ecclesia::SmbusEeprom2K>(
                ecclesia::SmbusEeprom::Option{
                    .name = "spicy16_intp0",
                    .mode = {.readable = 1, .writable = 0},
                    .get_device =
                        [&]() -> absl::optional<ecclesia::SmbusDevice> {
                      auto eeprom_smbus_bus =
                          GetEepromSmbusBus(kI2cDevicePathSuffixSpicy16Intp0);
                      if (!eeprom_smbus_bus) return absl::nullopt;
                      ecclesia::SmbusLocation loc(*eeprom_smbus_bus,
                                                  kEepromSmbusAddressSpicy16);
                      ecclesia::SmbusDevice device(loc, &access);
                      return device;
                    }}));
      }));

  fru_factories.push_back(ecclesia::SysmodelFruReaderFactory(
      "spicy16_intp4",
      [&]() -> std::unique_ptr<ecclesia::SysmodelFruReaderIntf> {
        return absl::make_unique<ecclesia::SmbusEepromFruReader>(
            absl::make_unique<ecclesia::SmbusEeprom2K>(
                ecclesia::SmbusEeprom::Option{
                    .name = "spicy16_intp4",
                    .mode = {.readable = 1, .writable = 0},
                    .get_device =
                        [&]() -> absl::optional<ecclesia::SmbusDevice> {
                      auto eeprom_smbus_bus =
                          GetEepromSmbusBus(kI2cDevicePathSuffixSpicy16Intp4);
                      if (!eeprom_smbus_bus) return absl::nullopt;
                      ecclesia::SmbusLocation loc(*eeprom_smbus_bus,
                                                  kEepromSmbusAddressSpicy16);
                      ecclesia::SmbusDevice device(loc, &access);
                      return device;
                    }}));
      }));

  // Construct an IPMI interface to Sleipnir BMC and add FRUs if there is any.
  ecclesia::Ipmitool ipmi(ecclesia::GetIpmiCredentialFromPb(kMagentConfigPath));
  auto ipmi_frus = ipmi.GetAllFrus();
  for (const auto& fru : ipmi_frus) {
    fru_factories.push_back(ecclesia::SysmodelFruReaderFactory(
        absl::StrCat("sleipnir_", fru.name),
        [&]() -> std::unique_ptr<ecclesia::SysmodelFruReaderIntf> {
          return absl::make_unique<ecclesia::IpmiSysmodelFruReader>(&ipmi,
                                                                    fru.fru_id);
        }));
  }

  auto ipmi_sensors = ipmi.GetAllSensors();

  ecclesia::SysmodelParams params = {
      .field_translator =
          absl::make_unique<ecclesia::IndusSmbiosFieldTranslator>(),
      .smbios_entry_point_path = kSmbiosEntryPointPath,
      .smbios_tables_path = kSmbiosTablesPath,
      .mced_socket_path = absl::GetFlag(FLAGS_mced_socket_path),
      .sysfs_mem_file_path = kSysfsMemFilePath,
      .fru_factories = absl::MakeSpan(fru_factories),
      .dimm_thermal_params = absl::MakeSpan(dimm_channel_info),
      .ipmi_sensors = absl::MakeSpan(ipmi_sensors),
      .nvme_discover_getter =
          [](ecclesia::PciTopologyInterface* pci_topology) {
            return std::make_unique<ecclesia::IndusNvmeDiscover>(pci_topology);
          },
      .pci_storage_discover_getter = nullptr};

  std::unique_ptr<ecclesia::SystemModel> system_model =
      absl::make_unique<ecclesia::SystemModel>(std::move(params));

  auto server = ecclesia::CreateServer(absl::GetFlag(FLAGS_port));
  server->RegisterRequestHandler(
      "/healthz",
      [](tensorflow::serving::net_http::ServerRequestInterface* req) {
        tensorflow::serving::net_http::SetContentType(
            req, "text/html;charset=utf-8");
        req->WriteResponseString("ok");
        req->ReplyWithStatus(tensorflow::serving::net_http::HTTPStatusCode::OK);
      },
      tensorflow::serving::net_http::RequestHandlerOptions());

  ecclesia::IndusRedfishService redfish_service(
      server.get(), system_model.get(), absl::GetFlag(FLAGS_assemblies_dir),
      absl::GetFlag(FLAGS_odata_metadata_file_path));

  bool success = server->StartAcceptingRequests();
  if (server != nullptr && success) {
    server->WaitForTermination();
    return EXIT_SUCCESS;
  } else {
    ecclesia::ErrorLog() << "Failed to start the server.";
    return EXIT_FAILURE;
  }
}
