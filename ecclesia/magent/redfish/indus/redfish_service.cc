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
#include "ecclesia/lib/io/pci/location.h"
#include "ecclesia/lib/io/usb/ids.h"
#include "ecclesia/lib/io/usb/usb.h"
#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/magent/redfish/common/memory_collection.h"
#include "ecclesia/magent/redfish/common/pcie_device.h"
#include "ecclesia/magent/redfish/common/pcie_device_collection.h"
#include "ecclesia/magent/redfish/common/pcie_function.h"
#include "ecclesia/magent/redfish/common/processor_collection.h"
#include "ecclesia/magent/redfish/common/processor_metrics.h"
#include "ecclesia/magent/redfish/common/root.h"
#include "ecclesia/magent/redfish/common/service_root.h"
#include "ecclesia/magent/redfish/common/session_collection.h"
#include "ecclesia/magent/redfish/common/session_service.h"
#include "ecclesia/magent/redfish/common/software_inventory.h"
#include "ecclesia/magent/redfish/common/storage_collection.h"
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
#include "ecclesia/magent/redfish/indus/memory.h"
#include "ecclesia/magent/redfish/indus/memory_metrics.h"
#include "ecclesia/magent/redfish/indus/pcie_slots.h"
#include "ecclesia/magent/redfish/indus/processor.h"
#include "ecclesia/magent/redfish/indus/software.h"
#include "ecclesia/magent/redfish/indus/storage.h"
#include "ecclesia/magent/redfish/indus/system.h"
#include "ecclesia/magent/sysmodel/x86/fru.h"
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

// A helper function to create an AssemblyModifier that can be used to append
// spicy16 interposer card FRU in Indus assemblies. If the input fru has value,
// the FRU info will also be added to the assembly.
Assembly::AssemblyModifier CreateModifierToAppendSpicy16Fru(
    int member_id, int connector_id, absl::optional<SysmodelFru> fru) {
  return [fru, member_id, connector_id](
             absl::flat_hash_map<std::string, Json::Value> &assemblies) {
    // Construct JSON value with spicy16 assembly static info.
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

    value[kOem][kGoogle][kComponents][1][kOdataId] =
        absl::StrFormat("%s/Assembly#/Assemblies/%d/Oem/Google/Components/1",
                        kChassisUri, member_id);
    if (fru.has_value()) {
      value[kPartNumber] = std::string(fru->GetPartNumber());
      value[kSerialNumber] = std::string(fru->GetSerialNumber());
      value[kVendor] = std::string(fru->GetManufacturer());
    }
    const std::string target_url = absl::StrCat(kChassisUri, "/Assembly");
    auto assembly_iter = assemblies.find(target_url);
    if (assembly_iter == assemblies.end()) {
      return absl::NotFoundError(absl::StrCat(
          "Failed to find a matched assembly with URL: ", target_url));
    }
    auto &assembly_resource_json = assembly_iter->second;
    assembly_resource_json[kAssemblies].append(value);
    return absl::OkStatus();
  };
}

// The indices of the spicy16 assembly in the Indus assemblies array.
constexpr int kPe2Spicy16AssemblyIndex = 18;
constexpr int kPe3Spicy16AssemblyIndex = 19;
// The indices of the PE2 & PE3 connectors as components of Indus mobo assembly.
constexpr int kPe2ConnectorComponentIndex = 31;
constexpr int kPe3ConnectorComponentIndex = 32;

}  // namespace

IndusRedfishService::IndusRedfishService(
    tensorflow::serving::net_http::HTTPServerInterface *server,
    SystemModel *system_model, absl::string_view assemblies_dir,
    absl::string_view odata_metadata_path) {
  resources_.push_back(CreateResource<Root>(server));
  resources_.push_back(CreateResource<ServiceRoot>(server));
  resources_.push_back(CreateResource<ComputerSystemCollection>(server));
  resources_.push_back(CreateResource<ComputerSystem>(server, system_model));
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
  // Add spicy16 intp that corresponds to PE2.
  maybe_fru = GetValidFru(system_model->GetFruReader("spicy16_intp0"));
  if (maybe_fru.has_value()) {
    assembly_modifiers.push_back(CreateModifierToAppendSpicy16Fru(
        kPe2Spicy16AssemblyIndex, kPe2ConnectorComponentIndex,
        maybe_fru.value()));
  } else if (sleipnir_location.ok()) {
    assembly_modifiers.push_back(CreateModifierToAppendSpicy16Fru(
        kPe2Spicy16AssemblyIndex, kPe2ConnectorComponentIndex, absl::nullopt));
  }
  // Add spicy16 intp that corresponds to PE3.
  maybe_fru = GetValidFru(system_model->GetFruReader("spicy16_intp4"));
  if (maybe_fru.has_value()) {
    assembly_modifiers.push_back(CreateModifierToAppendSpicy16Fru(
        kPe3Spicy16AssemblyIndex, kPe3ConnectorComponentIndex,
        maybe_fru.value()));
  } else if (sleipnir_location.ok()) {
    assembly_modifiers.push_back(CreateModifierToAppendSpicy16Fru(
        kPe3Spicy16AssemblyIndex, kPe3ConnectorComponentIndex, absl::nullopt));
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

  resources_.push_back(CreateResource<Assembly>(server, assemblies_dir,
                                                std::move(assembly_modifiers)));
  resources_.push_back(CreateResource<MemoryMetrics>(server, system_model));
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
  resources_.push_back(CreateResource<SoftwareInventory>(server));
  resources_.push_back(CreateResource<StorageCollection>(server, system_model));
  resources_.push_back(CreateResource<Storage>(server, system_model));
  resources_.push_back(CreateResource<Drive>(server, system_model));
  metadata_ = CreateMetadata(server, odata_metadata_path);
}

}  // namespace ecclesia
