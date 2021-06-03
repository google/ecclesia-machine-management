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

#include "ecclesia/magent/redfish/indus/redfish_service.h"

#include <sys/types.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/meta/type_traits.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "ecclesia/lib/io/pci/discovery.h"
#include "ecclesia/lib/io/pci/location.h"
#include "ecclesia/lib/io/usb/ids.h"
#include "ecclesia/lib/io/usb/usb.h"
#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/lib/types/fixed_range_int.h"
#include "ecclesia/magent/lib/event_logger/indus/system_event_visitors.h"
#include "ecclesia/magent/redfish/common/memory.h"
#include "ecclesia/magent/redfish/common/memory_collection.h"
#include "ecclesia/magent/redfish/common/memory_metrics.h"
#include "ecclesia/magent/redfish/common/pcie_device.h"
#include "ecclesia/magent/redfish/common/pcie_device_collection.h"
#include "ecclesia/magent/redfish/common/pcie_function.h"
#include "ecclesia/magent/redfish/common/processor.h"
#include "ecclesia/magent/redfish/common/processor_collection.h"
#include "ecclesia/magent/redfish/common/processor_metrics.h"
#include "ecclesia/magent/redfish/common/root.h"
#include "ecclesia/magent/redfish/common/service_root.h"
#include "ecclesia/magent/redfish/common/session_collection.h"
#include "ecclesia/magent/redfish/common/session_service.h"
#include "ecclesia/magent/redfish/common/software.h"
#include "ecclesia/magent/redfish/common/software_inventory.h"
#include "ecclesia/magent/redfish/common/storage_collection.h"
#include "ecclesia/magent/redfish/common/storage_controller_collection.h"
#include "ecclesia/magent/redfish/common/system.h"
#include "ecclesia/magent/redfish/common/systems.h"
#include "ecclesia/magent/redfish/common/thermal.h"
#include "ecclesia/magent/redfish/common/update_service.h"
#include "ecclesia/magent/redfish/core/assembly.h"
#include "ecclesia/magent/redfish/core/assembly_modifiers.h"
#include "ecclesia/magent/redfish/core/odata_metadata.h"
#include "ecclesia/magent/redfish/core/redfish_keywords.h"
#include "ecclesia/magent/redfish/core/resource.h"
#include "ecclesia/magent/redfish/indus/chassis.h"
#include "ecclesia/magent/redfish/indus/drive.h"
#include "ecclesia/magent/redfish/indus/pcie_slots.h"
#include "ecclesia/magent/redfish/indus/sleipnir_sensor.h"
#include "ecclesia/magent/redfish/indus/sleipnir_sensor_collection.h"
#include "ecclesia/magent/redfish/indus/storage.h"
#include "ecclesia/magent/redfish/indus/storage_controller.h"
#include "ecclesia/magent/sysmodel/x86/fru.h"
#include "ecclesia/magent/sysmodel/x86/nvme.h"
#include "ecclesia/magent/sysmodel/x86/sysmodel.h"
#include "json/json.h"
#include "json/value.h"
#include "tensorflow_serving/util/net_http/server/public/httpserver_interface.h"

namespace ecclesia {

namespace {

// This helper function creates an AssemblyModifier that can be used to add some
// sysmodel FRU info (e.g., part and serial numbers) to the assemblies resource.
// For example, CreateModifierToAddFruInfo(fru1,
// "/redfish/v1/Chassis/Sleipnir/Assembly", "bmc_riser") returns an
// AssemblyModifier which will find the assembly resource with URL
// "/redfish/v1/Chassis/Sleipnir/Assembly" and add part & serial numbers of fru1
// to the assembly named "bmc_riser".
Assembly::AssemblyModifier CreateModifierToAddFruInfo(
    const SysmodelFru &sysmodel_fru, const std::string &assembly_url,
    const std::string &assembly_name) {
  return [sysmodel_fru, assembly_url, assembly_name](
             absl::flat_hash_map<std::string, Json::Value> &assemblies) {
    auto assembly_iter = assemblies.find(assembly_url);
    if (assembly_iter == assemblies.end()) {
      return absl::NotFoundError(absl::StrCat(
          "Failed to find a matched assembly with URL: ", assembly_url));
    }
    auto &assembly_resource_json = assembly_iter->second;
    for (auto &assembly : assembly_resource_json[kAssemblies]) {
      if (assembly[kName] == assembly_name) {
        assembly[kPartNumber] = std::string(sysmodel_fru.GetPartNumber());
        assembly[kSerialNumber] = std::string(sysmodel_fru.GetSerialNumber());
        assembly[kVendor] = std::string(sysmodel_fru.GetManufacturer());
        return absl::OkStatus();
      }
    }
    return absl::NotFoundError(
        absl::StrFormat("Failed to find subassembly of name %s at URL %s",
                        assembly_name, assembly_url));
  };
}

// A helper function to get valid SysmodelFru if existing or nullopt.
absl::optional<SysmodelFru> GetValidFru(SysmodelFruReaderIntf *fru_reader) {
  if (fru_reader == nullptr) {
    return absl::nullopt;
  }
  return fru_reader->Read();
}

// This is the static JSON content of the Spicy 16 Assembly. Some dynamic info
// (e.g., member id, plugin connector id) will be dynamically added.
constexpr char kSpicy16AssemblyStaticJson[] = R"json({
"Name" : "spicy16_intp",
"Oem" : {
  "Google" : {
     "AttachedTo" : [
     ],
     "Components" : [
        {
           "MemberId" : 0,
           "Name" : "spicy16_intp",
           "AssociatedWith" : [
           ]
        },
        {
           "MemberId" : 1,
           "Name" : "CDFP",
           "PhysicalContext" : "Connector"
        }
     ]
  }
}
})json";

// The indices of the PE2 & PE3 connectors as components of Indus mobo assembly.
constexpr int kPe2ConnectorComponentIndex = 29;
constexpr int kPe3ConnectorComponentIndex = 30;

constexpr absl::string_view kSweet16CableAssemblyName = "sweet16_cable";

// Mapping for PCIe Device ACPI Path to CPU roots (pulled from indus board
// layout)
constexpr absl::string_view kCascadeLakeAssemblyName = "cascadelake";

struct CpuPcieRoot {
  uint pcie_root_bridge_num;
  std::string processor_id;
};

const absl::flat_hash_map<std::string, CpuPcieRoot> &GetAcpiToCpuPcieRoot() {
  static const absl::flat_hash_map<std::string, CpuPcieRoot> *map =
      new absl::flat_hash_map<std::string, CpuPcieRoot>({
          {"\\_SB_.PC00", {0, "0"}},
          {"\\_SB_.PC01", {1, "0"}},
          {"\\_SB_.PC02", {2, "0"}},
          {"\\_SB_.PC03", {3, "0"}},
          {"\\_SB_.PC06", {0, "1"}},
          {"\\_SB_.PC07", {1, "1"}},
          {"\\_SB_.PC08", {2, "1"}},
          {"\\_SB_.PC09", {3, "1"}},
      });
  return *map;
}

std::string GetComponentNameForCpuPcieRoot(const uint bridge_num,
                                           const int pci_device_num) {
  return absl::StrFormat("iio:pcie_root@bridge%d:port%d.0", bridge_num,
                         pci_device_num);
}

constexpr absl::string_view kIoCardAssemblyName = "io_card";

std::string GetComponentNameForIoCardPciLocation(int pci_device_function) {
  return absl::StrFormat("function%d", pci_device_function);
}

constexpr absl::string_view kSystemName = "Indus";
constexpr absl::string_view kSoftwareName = "magent-indus";
}  // namespace

Assembly::AssemblyModifier CreateModifierToAttachSpicy16Fru(
    int connector_id, absl::optional<SysmodelFru> fru) {
  return [fru, connector_id](
             absl::flat_hash_map<std::string, Json::Value> &assemblies) {
    // Step 1: create and append the spicy16 FRU to the Indus Assemblies.
    const std::string indus_assembly_url =
        absl::StrCat(kChassisUri, "/Assembly");
    auto assembly_iter = assemblies.find(indus_assembly_url);
    if (assembly_iter == assemblies.end()) {
      return absl::NotFoundError(absl::StrCat(
          "Failed to find a matched assembly with URL: ", indus_assembly_url));
    }
    InfoLog() << "Attaching Spicy16 intp card to Assemblies";
    auto &indus_assembly_resource_json = assembly_iter->second;
    int member_id = indus_assembly_resource_json[kAssemblies].size();
    // 1.1 Construct JSON value with spicy16 assembly static info.
    Json::Reader reader;
    Json::Value value;
    reader.parse(kSpicy16AssemblyStaticJson, value);
    // Add dynamic info in the JSON content.
    std::string self_url =
        absl::StrFormat("%s/Assembly#/Assemblies/%d", kChassisUri, member_id);
    value[kOdataId] = self_url;
    value[kMemberId] = member_id;
    Json::Value attached_to;
    attached_to[kOdataId] =
        absl::StrFormat("%s/Assembly#/Assemblies/0/Oem/Google/Components/%d",
                        kChassisUri, connector_id);
    value[kOem][kGoogle][kAttachedTo].append(attached_to);
    Json::Value associated_with;
    associated_with[kOdataId] = self_url;
    value[kOem][kGoogle][kComponents][0][kAssociatedWith].append(
        associated_with);
    value[kOem][kGoogle][kComponents][0][kOdataId] =
        absl::StrFormat("%s/Assembly#/Assemblies/%d/Oem/Google/Components/0",
                        kChassisUri, member_id);

    const std::string spicy16_downstream_connector_url =
        absl::StrFormat("%s/Assembly#/Assemblies/%d/Oem/Google/Components/1",
                        kChassisUri, member_id);
    value[kOem][kGoogle][kComponents][1][kOdataId] =
        spicy16_downstream_connector_url;

    if (fru.has_value()) {
      value[kPartNumber] = std::string(fru->GetPartNumber());
      value[kSerialNumber] = std::string(fru->GetSerialNumber());
      value[kVendor] = std::string(fru->GetManufacturer());
    }
    // 1.2 Append the json to the end of the assemblies array. The member ID is
    // already updated.
    indus_assembly_resource_json[kAssemblies].append(value);

    // Step 2: modify the reference of the created spicy16 assembly in Sleipnir
    // Assemblies.
    const std::string sleipnir_assembly_url =
        absl::StrCat(kChassisCollectionUri, "/Sleipnir/Assembly");
    assembly_iter = assemblies.find(sleipnir_assembly_url);
    if (assembly_iter == assemblies.end()) {
      return absl::NotFoundError(
          absl::StrCat("Failed to find a matched assembly with URL: ",
                       sleipnir_assembly_url));
    }
    auto &sleipnir_assembly_resource_json = assembly_iter->second;
    // 2.1 Find the index of the sweet16 cable which should be plugged into the
    // spicy16 interposer card downstream connector.
    int matched_sweet16_idx = -1;
    for (int idx = 0; idx < sleipnir_assembly_resource_json[kAssemblies].size();
         idx++) {
      if (sleipnir_assembly_resource_json[kAssemblies][idx][kName].asString() ==
          kSweet16CableAssemblyName) {
        matched_sweet16_idx = idx;
        // Here it assumes there are two spicy16 intp cards and two sweet16
        // cables. And the first & second sweet16 cable match the spicy16 card
        // in PE2 & PE3 connectors, respectively.
        if (connector_id == kPe2ConnectorComponentIndex) {
          break;
        }
      }
    }
    if (matched_sweet16_idx == -1) {
      return absl::NotFoundError(absl::StrCat(
          "Failed to find a matched sweet16 assembly in resource with URL: ",
          sleipnir_assembly_url));
    }
    InfoLog() << "Modifying Sweet16 AttachedTo info";
    auto &sweet16_assembly =
        sleipnir_assembly_resource_json[kAssemblies][matched_sweet16_idx];
    sweet16_assembly[kOem][kGoogle][kAttachedTo][0][kOdataId] =
        spicy16_downstream_connector_url;
    return absl::OkStatus();
  };
}

IndusRedfishService::IndusRedfishService(
    tensorflow::serving::net_http::HTTPServerInterface *server,
    SystemModel *system_model, absl::string_view assemblies_dir,
    absl::string_view odata_metadata_path) {
  resources_.push_back(CreateResource<Root>(server));
  resources_.push_back(CreateResource<ServiceRoot>(server));
  resources_.push_back(CreateResource<ComputerSystemCollection>(server));
  resources_.push_back(CreateResource<ComputerSystem>(
      server, system_model, std::string(kSystemName)));
  resources_.push_back(CreateResource<ChassisCollection>(server, system_model));
  resources_.push_back(CreateResource<Chassis>(server, system_model));
  resources_.push_back(CreateResource<MemoryCollection>(server, system_model));
  resources_.push_back(CreateResource<Memory>(server, system_model));
  resources_.push_back(CreateResource<SessionCollection>(server));
  resources_.push_back(CreateResource<SessionService>(server));

  std::vector<Assembly::AssemblyModifier> assembly_modifiers;
  // Create two AssemblyModifiers that add the IPMI FRUs part number and serial
  // number to the corresponding assembly in Sleipnir assemblies.
  const std::string sleipnir_chassis_assembly_url =
      "/redfish/v1/Chassis/Sleipnir/Assembly";
  auto maybe_fru = GetValidFru(system_model->GetFruReader("sleipnir_hsbp"));
  if (maybe_fru.has_value()) {
    assembly_modifiers.push_back(CreateModifierToAddFruInfo(
        maybe_fru.value(), sleipnir_chassis_assembly_url,
        "sleipnir_mainboard"));
  }
  maybe_fru = GetValidFru(system_model->GetFruReader("sleipnir_bmc"));
  if (maybe_fru.has_value()) {
    assembly_modifiers.push_back(CreateModifierToAddFruInfo(
        maybe_fru.value(), sleipnir_chassis_assembly_url, "bmc_riser"));
  }

  // Create AssemblyModifier to add Spicy16 intp card into Indus assemblies.
  // There are a few conditions we need to handle:
  // 1. Indus alone without spicy16 or Sleipnir;
  //  1.1 Expect the spicy16 FRU reading failure and Sleipnir not detected,
  //  no spicy16 is added.
  // 2. Indus with spicy16 but without Sleipnir;
  //  2.1 If spicy16 FRU reading succeeds, add spicy16 and it's FRU info in
  //  Indus assemblies.
  //  2.2 If spicy16 FRU reading fails, no spicy16 is added even though it's
  //  physically plugged in.
  // 3. Indus with spicy16 and Sleipnir;
  //  3.1 If spicy16 FRU reading succeeds, add spicy16 and it's FRU info in
  //  Indus assemblies.
  //  3.2 If spicy16 FRU reading fails but Sleipnir is detected, still add
  //  spicy16 in Indus assemblies but no FRU info.
  //  3.3 If spicy16 FRU reading fails and no Sleipnir is detected, then no
  //  spicy16 is added.
  auto sleipnir_location = FindUsbDeviceWithSignature(
      system_model->GetUsbDiscoveryIntf(), kUsbSignatureSleipnirBmc);
  // Add spicy16 intp card that corresponds to PE2.
  maybe_fru = GetValidFru(system_model->GetFruReader("spicy16_intp0"));
  if (maybe_fru.has_value() || sleipnir_location.ok()) {
    assembly_modifiers.push_back(CreateModifierToAttachSpicy16Fru(
        kPe2ConnectorComponentIndex, maybe_fru));
  }
  // Add spicy16 intp card that corresponds to PE3.
  maybe_fru = GetValidFru(system_model->GetFruReader("spicy16_intp4"));
  if (maybe_fru.has_value() || sleipnir_location.ok()) {
    assembly_modifiers.push_back(CreateModifierToAttachSpicy16Fru(
        kPe3ConnectorComponentIndex, maybe_fru));
  }

  // Iterate through all storage devices to attach PCIeFunctions
  for (const auto &nvme_location : system_model->GetNvmePhysLocations()) {
    const std::string drive_assembly = absl::StrCat(
        absl::StrReplaceAll(kDriveUriPattern, {{"(\\w+)", nvme_location}}), "/",
        kAssembly);
    assembly_modifiers.push_back(CreateModifierToAssociatePcieFunction(
        system_model->GetNvmeByPhysLocation(nvme_location)
            ->location.pci_location,
        drive_assembly, "nvme_ssd", "nvme"));
  }

  // Iterate through all Pci Devices and attach them to appropriate component
  // based on ACPI Path
  // Also attach downstream IO module to the appropriate CpuPcieRoot and
  // PCIeDevice
  const CpuPcieRoot kIoCardUpstreamCpuPcieRoot = {3, "0"};
  const uint kIoCardUpstreamPciDeviceNumber = 0,
             kIoCardDownstreamPciDeviceNumber = 0;

  absl::flat_hash_map<std::pair<PciDomain, PciBusNum>, CpuPcieRoot>
      pci_bus_to_cpu_pcie_root;
  const absl::flat_hash_map<std::string, CpuPcieRoot> &acpi_to_cpu_pcie_root =
      GetAcpiToCpuPcieRoot();
  if (auto acpi_paths = system_model->GetAcpiPathsFromPciTopology();
      acpi_paths.ok()) {
    for (const auto &pci_acpi : acpi_paths.value()) {
      if (auto it = acpi_to_cpu_pcie_root.find(pci_acpi.acpi_path);
          it != acpi_to_cpu_pcie_root.end()) {
        pci_bus_to_cpu_pcie_root[std::make_pair(pci_acpi.domain,
                                                pci_acpi.bus)] = it->second;
      }
    }
  }
  for (const auto &pci_device : system_model->GetPcieDeviceLocations()) {
    if (auto it = pci_bus_to_cpu_pcie_root.find(
            std::make_pair(pci_device.domain(), pci_device.bus()));
        it != pci_bus_to_cpu_pcie_root.end()) {
      // First create AssemblyModifier to create PCIE root component in CPU
      const std::string assembly_uri =
          absl::StrCat(kProcessorCollectionUri, "/", it->second.processor_id,
                       "/", kAssembly);
      const std::string component_name = GetComponentNameForCpuPcieRoot(
          it->second.pcie_root_bridge_num, pci_device.device().value());
      assembly_modifiers.push_back(CreateModifierToCreateComponent(
          assembly_uri, std::string(kCascadeLakeAssemblyName), component_name));
      // Second create AssemblyModifier to associate PCI device with the created
      // component
      assembly_modifiers.push_back(CreateModifierToAssociatePcieFunction(
          pci_device, assembly_uri, std::string(kCascadeLakeAssemblyName),
          component_name));

      if (it->second.processor_id == kIoCardUpstreamCpuPcieRoot.processor_id &&
          it->second.pcie_root_bridge_num ==
              kIoCardUpstreamCpuPcieRoot.pcie_root_bridge_num &&
          pci_device.device().value() == kIoCardUpstreamPciDeviceNumber) {
        // Create function component in IO card assembly
        if (const auto &downstream_pci_devices =
                system_model->GetPciNodeConnections(pci_device);
            downstream_pci_devices.ok()) {
          for (const auto &downstream_pci : downstream_pci_devices->children) {
            if (downstream_pci.device().value() !=
                kIoCardDownstreamPciDeviceNumber)
              continue;
            const std::string io_card_component_name =
                GetComponentNameForIoCardPciLocation(
                    downstream_pci.function().value());
            assembly_modifiers.push_back(CreateModifierToCreateComponent(
                std::string(kAssemblyUri), std::string(kIoCardAssemblyName),
                io_card_component_name));
            assembly_modifiers.push_back(CreateModifierToAssociatePcieFunction(
                downstream_pci, std::string(kAssemblyUri),
                std::string(kIoCardAssemblyName), io_card_component_name));
          }
        }
      }
    }
  }

  resources_.push_back(CreateResource<Assembly>(server, assemblies_dir,
                                                std::move(assembly_modifiers)));
  resources_.push_back(CreateResource<MemoryMetrics>(
      server, system_model, CreateIndusDimmErrorCountingVisitor));
  resources_.push_back(
      CreateResource<ProcessorCollection>(server, system_model));

  resources_.push_back(CreateResource<Processor>(server, system_model));
  resources_.push_back(CreateResource<ProcessorMetrics>(server, system_model));
  resources_.push_back(CreateResource<Thermal>(server, system_model));
  resources_.push_back(CreateResource<PCIeSlots>(server, system_model));
  resources_.push_back(
      CreateResource<PCIeDeviceCollection>(server, system_model));
  resources_.push_back(CreateResource<PCIeDevice>(server, system_model));
  resources_.push_back(CreateResource<PCIeFunction>(server, system_model));
  resources_.push_back(CreateResource<UpdateService>(server));
  resources_.push_back(CreateResource<SoftwareInventoryCollection>(server));
  resources_.push_back(
      CreateResource<SoftwareInventory>(server, std::string(kSoftwareName)));
  resources_.push_back(CreateResource<StorageCollection>(server, system_model));
  resources_.push_back(CreateResource<Storage>(server, system_model));
  resources_.push_back(CreateResource<StorageControllerCollection>(server));
  resources_.push_back(CreateResource<StorageController>(server, system_model));
  resources_.push_back(CreateResource<Drive>(server, system_model));
  resources_.push_back(
      CreateResource<SleipnirIpmiSensor>(server, system_model));
  resources_.push_back(CreateResource<SensorCollection>(server, system_model));
  metadata_ = CreateMetadata(server, odata_metadata_path);
}

}  // namespace ecclesia
