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

#include "ecclesia/lib/redfish/sysmodel.h"

#include <functional>
#include <memory>
#include <utility>

#include "absl/container/flat_hash_set.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/property_definitions.h"

namespace ecclesia {

using ResultCallback = Sysmodel::ResultCallback;

// The following function overrides of QueryAllResources implement the search
// algorithms for the specific Redfish Resources in the URIs defined in the
// Redfish Schema Supplement. The supported URIs are non-exhaustive.

// Chassis:
// "/redfish/v1/Chassis/{id}"
void Sysmodel::QueryAllResourceInternal(Token<ResourceChassis> /*unused*/,
                                        ResultCallback result_callback,
                                        const QueryParams &query_params) {
  RedfishVariant root = redfish_intf_->GetRoot();
  root.AsIndexHelper()
      .Get(kRfPropertyChassis,
           {.freshness = query_params.freshness,
            .expand = RedfishQueryParamExpand({.levels = 0})})
      .Each()
      .Do([&](std::unique_ptr<RedfishObject> &chassis_obj) {
        return result_callback(std::move(chassis_obj));
      });
}

// System:
// "/redfish/v1/System/{id}"
void Sysmodel::QueryAllResourceInternal(Token<ResourceSystem> /*unused*/,
                                        ResultCallback result_callback,
                                        const QueryParams &query_params) {
  RedfishVariant root = redfish_intf_->GetRoot();
  root.AsIndexHelper()
      .Get(kRfPropertySystems,
           {.freshness = query_params.freshness,
            .expand = RedfishQueryParamExpand({.levels = 1})})
      .Each()
      .Do([&](std::unique_ptr<RedfishObject> &sys_obj) {
        return result_callback(std::move(sys_obj));
      });
}

// Memory:
// "/redfish/v1/Systems/{id}/Memory/{id}"
// Expanding memory is time consuming
void Sysmodel::QueryAllResourceInternal(Token<ResourceMemory> /*unused*/,
                                        ResultCallback result_callback,
                                        const QueryParams &query_params) {
  RedfishVariant root = redfish_intf_->GetRoot();
  root.AsIndexHelper()
      .Get(kRfPropertySystems,
           {.expand = RedfishQueryParamExpand({.levels = 1})})
      .Each()
      .Get(kRfPropertyMemory, {.freshness = query_params.freshness,
                               .expand = RedfishQueryParamExpand(
                                   {.levels = query_params.expand_levels})})
      .Each()
      .Do([&](std::unique_ptr<RedfishObject> &memory_obj) {
        return result_callback(std::move(memory_obj));
      });
}

// Storage:
// "/redfish/v1/Systems/{id}/Storage/{id}"
void Sysmodel::QueryAllResourceInternal(Token<ResourceStorage> /*unused*/,
                                        ResultCallback result_callback,
                                        const QueryParams &query_params) {
  RedfishVariant root = redfish_intf_->GetRoot();
  root.AsIndexHelper()
      .Get(kRfPropertySystems,
           {.expand = RedfishQueryParamExpand({.levels = 1})})
      .Each()
      .Get(kRfPropertyStorage,
           {.freshness = query_params.freshness,
            .auto_adjust_levels = false,
            .expand = RedfishQueryParamExpand({.levels = 0})})
      .Each()
      .Do([&](std::unique_ptr<RedfishObject> &storage_obj) {
        return result_callback(std::move(storage_obj));
      });
}

// Drive:
// "/redfish/v1/Systems/{id}/Storage/{id}/Drives/{id}"
// "/redfish/v1/Chassis/{ChassisId}/Drives/{DriveId}"
void Sysmodel::QueryAllResourceInternal(Token<ResourceDrive> /*unused*/,
                                        ResultCallback result_callback,
                                        const QueryParams &query_params) {
  // A given Drive resource may be referenced multiple times under one or more
  // Chassis and/or ComputerSystem resources, so we use a hash set of visited
  // URIs to avoid duplicate executions of the provided callback function for
  // the same Drive resource.
  absl::flat_hash_set<std::string> visited_uris;
  auto callback_once_per_uri =
      [&visited_uris, &result_callback](
          std::unique_ptr<RedfishObject> obj) -> RedfishIterReturnValue {
    std::optional<std::string> uri = obj->GetUriString();
    if (uri.has_value()) {
      bool was_inserted = visited_uris.insert(*std::move(uri)).second;
      if (was_inserted) {
        return result_callback(std::move(obj));
      }
    } else {
      // Because we are unable to definitively determine if we've run the
      // callback on objects without URIs, we fall back to running it on every
      // such object and leave it to the callback to handle any dedupication.
      return result_callback(std::move(obj));
    }
    return RedfishIterReturnValue::kContinue;
  };

  RedfishVariant root = redfish_intf_->GetRoot();
  root.AsIndexHelper()
      .Get(kRfPropertySystems,
           {.expand = RedfishQueryParamExpand({.levels = 1})})
      .Each()
      .Get(kRfPropertyStorage,  // Expand disks+drives
           {.freshness = query_params.freshness,
            .auto_adjust_levels = false,
            .expand = RedfishQueryParamExpand({.levels = 0})})
      .Each()[kRfPropertyDrives]
      .Each()
      .Do([&](std::unique_ptr<RedfishObject> &drive_obj) {
        return callback_once_per_uri(std::move(drive_obj));
      });

  root.AsIndexHelper()
      .Get(kRfPropertyChassis,
           {.expand = RedfishQueryParamExpand({.levels = 1})})
      .Each()
      .Get(kRfPropertyDrives,
           {.freshness = query_params.freshness,
            .auto_adjust_levels = false,
            .expand = RedfishQueryParamExpand({.levels = 0})})
      .Each()
      .Do([&](std::unique_ptr<RedfishObject> &drive_obj) {
        return callback_once_per_uri(std::move(drive_obj));
      });
}

// LegacyStorageController:
// "/redfish/v1/Systems/{id}/Storage/{id}#/StorageControllers/{id}"
void Sysmodel::QueryAllResourceInternal(
    Token<ResourceLegacyStorageController> /*unused*/,
    ResultCallback result_callback, const QueryParams &query_params) {
  RedfishVariant root = redfish_intf_->GetRoot();
  root.AsIndexHelper()
      .Get(kRfPropertySystems,
           {.expand = RedfishQueryParamExpand({.levels = 1})})
      .Each()
      .Get(kRfPropertyStorage,
           {.freshness = query_params.freshness,
            .auto_adjust_levels = false,
            .expand = RedfishQueryParamExpand({.levels = 0})})
      .Each()[kRfPropertyStorageControllers]
      .Each()
      .Do([&](std::unique_ptr<RedfishObject> &ctrl_obj) {
        return result_callback(std::move(ctrl_obj));
      });
}

// StorageController:
// "​redfish/​v1/​Systems/​{id}/​Storage/​{id}/​Controllers/​{id}"
void Sysmodel::QueryAllResourceInternal(
    Token<ResourceStorageController> /*unused*/, ResultCallback result_callback,
    const QueryParams &query_params) {
  RedfishVariant root = redfish_intf_->GetRoot();
  root.AsIndexHelper()
      .Get(kRfPropertySystems,
           {.expand = RedfishQueryParamExpand({.levels = 1})})
      .Each()
      .Get(kRfPropertyStorage,
           {.freshness = query_params.freshness,
            .auto_adjust_levels = false,
            .expand = RedfishQueryParamExpand({.levels = 0})})
      .Each()[kRfPropertyControllers]
      .Each()
      .Do([&](std::unique_ptr<RedfishObject> &ctrl_obj) {
        return result_callback(std::move(ctrl_obj));
      });
}

// Processor:
// "/redfish/v1/Systems/{id}/Processors/{id}"
void Sysmodel::QueryAllResourceInternal(Token<ResourceProcessor> /*unused*/,
                                        ResultCallback result_callback,
                                        const QueryParams &query_params) {
  RedfishVariant root = redfish_intf_->GetRoot();
  root.AsIndexHelper()
      .Get(kRfPropertySystems,
           {.expand = RedfishQueryParamExpand({.levels = 1})})
      .Each()
      .Get(kRfPropertyProcessors, {.freshness = query_params.freshness,
                                   .auto_adjust_levels = true,
                                   .expand = RedfishQueryParamExpand(
                                       {.levels = query_params.expand_levels})})
      .Each()
      .Do([&](std::unique_ptr<RedfishObject> &processor_obj) {
        return result_callback(std::move(processor_obj));
      });
}

// Physical LPU (thread-granularity processor resource):
// "/redfish/v1/Systems/{id}/Processors/{id}/SubProcessors/{id}/SubProcessors/{id}"
void Sysmodel::QueryAllResourceInternal(
    Token<AbstractionPhysicalLpu> /*unused*/, ResultCallback result_callback,
    const QueryParams &query_params) {
  RedfishVariant root = redfish_intf_->GetRoot();
  root.AsIndexHelper()
      .Get(kRfPropertySystems,
           {.expand = RedfishQueryParamExpand({.levels = 1})})
      .Each()
      .Get(kRfPropertyProcessors, {.freshness = query_params.freshness,
                                   .auto_adjust_levels = true,
                                   .expand = RedfishQueryParamExpand(
                                       {.levels = query_params.expand_levels})})
      .Each()[kRfPropertySubProcessors]  // core subprocessors collection
      .Each()[kRfPropertySubProcessors]  // thread subprocessors collection
      .Each()
      .Do([&](std::unique_ptr<RedfishObject> &phs_lpu_obj) {
        return result_callback(std::move(phs_lpu_obj));
      });
}

// EthernetInterface:
// "/redfish/v1/Systems/{id}/EthernetInterfaces/{id}"
void Sysmodel::QueryAllResourceInternal(
    Token<ResourceEthernetInterface> /*unused*/, ResultCallback result_callback,
    const QueryParams &query_params) {
  RedfishVariant root = redfish_intf_->GetRoot();
  root.AsIndexHelper()
      .Get(kRfPropertySystems,
           {.freshness = query_params.freshness,
            .expand = RedfishQueryParamExpand({.levels = 1})})
      .Each()[kRfPropertyEthernetInterfaces]
      .Each()
      .Do([&](std::unique_ptr<RedfishObject> &eth_obj) {
        return result_callback(std::move(eth_obj));
      });

  root.AsIndexHelper()
      .Get(kRfPropertyManagers)
      .Each()[kRfPropertyEthernetInterfaces]
      .Each()
      .Do([&](std::unique_ptr<RedfishObject> &eth_obj) {
        return result_callback(std::move(eth_obj));
      });
}

// Thermal:
// "/redfish/v1/Chassis/{id}/Thermal"
void Sysmodel::QueryAllResourceInternal(Token<ResourceThermal> /*unused*/,
                                        ResultCallback result_callback,
                                        const QueryParams &query_params) {
  RedfishVariant root = redfish_intf_->GetRoot();
  root.AsIndexHelper()
      .Get(kRfPropertyChassis,
           {.freshness = query_params.freshness,
            .expand = RedfishQueryParamExpand({.levels = 1})})
      .Each()[kRfPropertyThermal]
      .Do([&](std::unique_ptr<RedfishObject> &thermal_obj) {
        return result_callback(std::move(thermal_obj));
      });
}

// Temperatures:
// "/redfish/v1/Chassis/{id}/Thermal/Temperatures"
void Sysmodel::QueryAllResourceInternal(Token<ResourceTemperature> /*unused*/,
                                        ResultCallback result_callback,
                                        const QueryParams &query_params) {
  RedfishVariant root = redfish_intf_->GetRoot();
  root.AsIndexHelper()
      .Get(kRfPropertyChassis,
           {.freshness = query_params.freshness,
            .expand = RedfishQueryParamExpand({.levels = 1})})
      .Each()[kRfPropertyThermal][kRfPropertyTemperatures]
      .Each()
      .Do([&](std::unique_ptr<RedfishObject> &temp_obj) {
        return result_callback(std::move(temp_obj));
      });
}

// Voltage:
// "/redfish/v1/Chassis/{id}/Power#/Voltages/{sensor}"
void Sysmodel::QueryAllResourceInternal(Token<ResourceVoltage> /*unused*/,
                                        ResultCallback result_callback,
                                        const QueryParams &query_params) {
  RedfishVariant root = redfish_intf_->GetRoot();
  root.AsIndexHelper()
      .Get(kRfPropertyChassis,
           {.freshness = query_params.freshness,
            .expand = RedfishQueryParamExpand({.levels = 1})})
      .Each()[kRfPropertyPower][kRfPropertyVoltages]
      .Each()
      .Do([&](std::unique_ptr<RedfishObject> &volt_obj) {
        return result_callback(std::move(volt_obj));
      });
}

// Fan:
// "/redfish/v1/Chassis/{id}/Thermal/Fans"
void Sysmodel::QueryAllResourceInternal(Token<ResourceFan> /*unused*/,
                                        ResultCallback result_callback,
                                        const QueryParams &query_params) {
  RedfishVariant root = redfish_intf_->GetRoot();
  root.AsIndexHelper()
      .Get(kRfPropertyChassis,
           {.freshness = query_params.freshness,
            .expand = RedfishQueryParamExpand({.levels = 1})})
      .Each()[kRfPropertyThermal][kRfPropertyFans]
      .Each()
      .Do([&](std::unique_ptr<RedfishObject> &temp_obj) {
        return result_callback(std::move(temp_obj));
      });
}

// Sensors:
// "/redfish/v1/Chassis/{id}/Sensors/{sensor}"
void Sysmodel::QueryAllResourceInternal(Token<ResourceSensor> /*unused*/,
                                        ResultCallback result_callback,
                                        const QueryParams &query_params) {
  RedfishVariant root = redfish_intf_->GetRoot();
  root.AsIndexHelper()
      .Get(kRfPropertyChassis,
           {.expand = RedfishQueryParamExpand({.levels = 1})})
      .Each()
      .Get(kRfPropertySensors,
           {.freshness = query_params.freshness,
            .expand = RedfishQueryParamExpand({.levels = 1})})
      .Each()
      .Do([&](std::unique_ptr<RedfishObject> &sensor_obj) {
        return result_callback(std::move(sensor_obj));
      });
}

// SensorsCollection:
// "/redfish/v1/Chassis/{id}/Sensors"
void Sysmodel::QueryAllResourceInternal(
    Token<ResourceSensorCollection> /*unused*/, ResultCallback result_callback,
    const QueryParams &query_params) {
  RedfishVariant root = redfish_intf_->GetRoot();
  root.AsIndexHelper()
      .Get(kRfPropertyChassis,
           {.expand = RedfishQueryParamExpand({.levels = 1})})
      .Each()
      .Get(kRfPropertySensors,
           {.freshness = query_params.freshness,
            .expand = RedfishQueryParamExpand({.levels = 1})})
      .Do([&](std::unique_ptr<RedfishObject> &sensor_obj) {
        return result_callback(std::move(sensor_obj));
      });
}

// Pcie Function:
// "/redfish/v1/Systems/{id}/PCIeDevices/{id}/PCIeFunctions/{id}"
void Sysmodel::QueryAllResourceInternal(Token<ResourcePcieFunction> /*unused*/,
                                        ResultCallback result_callback,
                                        const QueryParams &query_params) {
  RedfishVariant root = redfish_intf_->GetRoot();
  root.AsIndexHelper()
      .Get(kRfPropertySystems,
           {.expand = RedfishQueryParamExpand({.levels = 1})})
      .Each()
      .Get(kRfPropertyPcieDevices,
           {.freshness = query_params.freshness,
            .auto_adjust_levels = true,
            .expand = RedfishQueryParamExpand(
                {.levels = query_params.expand_levels})})
      .Each()[kRfPropertyLinks][kRfPropertyPcieFunctions]
      .Each()
      .Do([&](std::unique_ptr<RedfishObject> &pcie_function_obj) {
        return result_callback(std::move(pcie_function_obj));
      });
}

// ComputerSystem:
// "/redfish/v1/Systems/{id}"
void Sysmodel::QueryAllResourceInternal(
    Token<ResourceComputerSystem> /*unused*/, ResultCallback result_callback,
    const QueryParams &query_params) {
  RedfishVariant root = redfish_intf_->GetRoot();
  root.AsIndexHelper()
      .Get(kRfPropertySystems,
           {.freshness = query_params.freshness,
            .expand = RedfishQueryParamExpand({.levels = 1})})
      .Each()
      .Do([&](std::unique_ptr<RedfishObject> &sys_obj) {
        return result_callback(std::move(sys_obj));
      });
}

// Manager
// "/redfish/v1/Managers/{id}"
void Sysmodel::QueryAllResourceInternal(Token<ResourceManager> /*unused*/,
                                        ResultCallback result_callback,
                                        const QueryParams &query_params) {
  RedfishVariant root = redfish_intf_->GetRoot();
  root[kRfPropertyManagers].Each().Do(
      [&](std::unique_ptr<RedfishObject> &sys_obj) {
        return result_callback(std::move(sys_obj));
      });
}

// LogService:
// "/redfish/v1/Systems/{id}/LogServices/{id}"
void Sysmodel::QueryAllResourceInternal(Token<ResourceLogService> /*unused*/,
                                        ResultCallback result_callback,
                                        const QueryParams &query_params) {
  RedfishVariant root = redfish_intf_->GetRoot();
  RedfishIterReturnValue return_val = RedfishIterReturnValue::kContinue;

  root.AsIndexHelper()
      .Get(kRfPropertySystems,
           {.freshness = query_params.freshness,
            .expand = RedfishQueryParamExpand({.levels = 1})})
      .Each()[kRfPropertyLogServices]
      .Each()
      .Do([&](std::unique_ptr<RedfishObject> &service) {
        return return_val = result_callback(std::move(service));
      });
  if (return_val == RedfishIterReturnValue::kStop) return;
  root.AsIndexHelper()
      .Get(kRfPropertyManagers,
           {.freshness = query_params.freshness,
            .expand = RedfishQueryParamExpand({.levels = 1})})
      .Each()[kRfPropertyLogServices]
      .Each()
      .Do([&](std::unique_ptr<RedfishObject> &service) {
        return result_callback(std::move(service));
      });
}

// LogEntry:
// "/redfish/v1/Chassis/{id}/LogServices/{id}/Entries/{id}"
void Sysmodel::QueryAllResourceInternal(Token<ResourceLogEntry> /*unused*/,
                                        ResultCallback result_callback,
                                        const QueryParams &query_params) {
  RedfishVariant root = redfish_intf_->GetRoot();
  root.AsIndexHelper()
      .Get(kRfPropertyChassis,
           {.freshness = query_params.freshness,
            .expand = RedfishQueryParamExpand({.levels = 1})})
      .Each()[kRfPropertyLogServices]
      .Each()[kRfPropertyEntries]
      .Each()
      .Do([&](std::unique_ptr<RedfishObject> &entry) {
        return result_callback(std::move(entry));
      });
}

// SoftwareInventory:
// "/redfish/v1/UpdateService/FirmwareInventory/{id}"
// "/redfish/v1/UpdateService/SoftwareInventory/{id}"
void Sysmodel::QueryAllResourceInternal(
    Token<ResourceSoftwareInventory> /*unused*/, ResultCallback result_callback,
    const QueryParams &query_params) {
  RedfishVariant root = redfish_intf_->GetRoot();
  RedfishIterReturnValue return_val = RedfishIterReturnValue::kContinue;
  root[kRfPropertyUpdateService][kRfPropertyFirmwareInventory].Each().Do(
      [&](std::unique_ptr<RedfishObject> &software) {
        return return_val = result_callback(std::move(software));
      });
  if (return_val == RedfishIterReturnValue::kStop) return;
  root[kRfPropertyUpdateService][ResourceSoftwareInventory::Name].Each().Do(
      [&](std::unique_ptr<RedfishObject> &software) {
        return result_callback(std::move(software));
      });
}

// RootOfTrust:
// "/google/v1/RootOfTrustCollection/{id}/"
void Sysmodel::QueryAllResourceInternal(
    Token<OemResourceRootOfTrust> /*unused*/, ResultCallback result_callback,
    const QueryParams &query_params) {
  RedfishVariant root = redfish_intf_->GetRoot({}, ServiceRootUri::kGoogle);
  root[kRfPropertyRootOfTrustCollection].Each().Do(
      [&](std::unique_ptr<RedfishObject> &rot) {
        return result_callback(std::move(rot));
      });
}
// ComponentIntegrity:
// "/redfish/v1/ComponentIntegrity/{id}"
void Sysmodel::QueryAllResourceInternal(
    Token<ResourceComponentIntegrity> /*unused*/,
    ResultCallback result_callback, const QueryParams &query_params) {
  RedfishVariant root = redfish_intf_->GetRoot();
  root[kRfPropertyComponentIntegrity].Each().Do(
      [&](auto &obj) { return result_callback(std::move(obj)); });
}

// PCIeSlots:
// "/redfish/v1/Chassis/{id}/PCIeSlots"
void Sysmodel::QueryAllResourceInternal(Token<ResourcePcieSlots> /*unused*/,
                                        ResultCallback result_callback,
                                        const QueryParams &query_params) {
  RedfishVariant root = redfish_intf_->GetRoot();
  root.AsIndexHelper()
      .Get(kRfPropertyChassis,
           {.freshness = query_params.freshness,
            .expand = RedfishQueryParamExpand({.levels = 1})})
      .Each()[kRfPropertyPcieSlots]
      .Do([&](std::unique_ptr<RedfishObject> &entry) {
        return result_callback(std::move(entry));
      });
}

// Switch:
// "/redfish/v1/Fabrics/{id}/Switches/{id}"
void Sysmodel::QueryAllResourceInternal(
    Token<ResourceSwitch> resource_switch /*unused*/,
    ResultCallback result_callback, const QueryParams &query_params) {
  RedfishVariant root = redfish_intf_->GetRoot();
  root.AsIndexHelper()
      .Get(kRfPropertyFabrics,
           {.freshness = query_params.freshness,
            .expand = RedfishQueryParamExpand({.levels = 1})})
      .Each()[kRfPropertySwitches]
      .Each()
      .Do([&](std::unique_ptr<RedfishObject> &entry) {
        return result_callback(std::move(entry));
      });
}

}  // namespace ecclesia
