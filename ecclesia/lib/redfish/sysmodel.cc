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

#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/property_definitions.h"

namespace ecclesia {

using ResultCallback = Sysmodel::ResultCallback;

// The following function overrides of QueryAllResources implement the search
// algorithms for the specific Redfish Resources in the URIs defined in the
// Redfish Schema Supplement. The supported URIs are non-exhaustive.

// Chassis:
// "/redfish/v1/Chassis/{id}"
void Sysmodel::QueryAllResourceInternal(Token<ResourceChassis>,
                                        ResultCallback result_callback,
                                        size_t expand_levels) {
  auto root = redfish_intf_->GetRoot();
  root.AsIndexHelper()
      .Get(kRfPropertyChassis,
           {.expand = RedfishQueryParamExpand({.levels = 1})})
      .Each()
      .Do([&](auto &chassis_obj) {
        return result_callback(std::move(chassis_obj));
      });
}

// System:
// "/redfish/v1/System/{id}"
void Sysmodel::QueryAllResourceInternal(Token<ResourceSystem>,
                                        ResultCallback result_callback,
                                        size_t expand_levels) {
  auto root = redfish_intf_->GetRoot();
  root.AsIndexHelper()
      .Get(kRfPropertySystems,
           {.expand = RedfishQueryParamExpand({.levels = 1})})
      .Each()
      .Do([&](auto &sys_obj) { return result_callback(std::move(sys_obj)); });
}

// Memory:
// "/redfish/v1/Systems/{id}/Memory/{id}"
// Expanding memory is time consuming
void Sysmodel::QueryAllResourceInternal(Token<ResourceMemory>,
                                        ResultCallback result_callback,
                                        size_t expand_levels) {
  auto root = redfish_intf_->GetRoot();
  root.AsIndexHelper()
      .Get(kRfPropertySystems,
           {.expand = RedfishQueryParamExpand({.levels = 1})})
      .Each()[kRfPropertyMemory]
      .Each()
      .Do([&](auto &memory_obj) {
        return result_callback(std::move(memory_obj));
      });
}

// Storage:
// "/redfish/v1/Systems/{id}/Storage/{id}"
void Sysmodel::QueryAllResourceInternal(Token<ResourceStorage>,
                                        ResultCallback result_callback,
                                        size_t expand_levels) {
  auto root = redfish_intf_->GetRoot();
  root.AsIndexHelper()
      .Get(kRfPropertySystems,
           {.expand = RedfishQueryParamExpand({.levels = 1})})
      .Each()
      .Get(kRfPropertyStorage,
           {.auto_adjust_levels = true,
            .expand = RedfishQueryParamExpand({.levels = expand_levels})})
      .Each()
      .Do([&](auto &storage_obj) {
        return result_callback(std::move(storage_obj));
      });
}

// Drive:
// "/redfish/v1/Systems/{id}/Storage/{id}/Drives/{id}"
// "/redfish/v1/Chassis/{ChassisId}/Drives/{DriveId}"
void Sysmodel::QueryAllResourceInternal(Token<ResourceDrive>,
                                        ResultCallback result_callback,
                                        size_t expand_levels) {
  auto root = redfish_intf_->GetRoot();
  root.AsIndexHelper()
      .Get(kRfPropertySystems,
           {.expand = RedfishQueryParamExpand({.levels = 1})})
      .Each()
      .Get(kRfPropertyStorage,  // Expand disks+drives
           {.auto_adjust_levels = true,
            .expand = RedfishQueryParamExpand({.levels = expand_levels})})
      .Each()[kRfPropertyDrives]
      .Each()
      .Do([&](auto &drive_obj) {
        return result_callback(std::move(drive_obj));
      });

  root.AsIndexHelper()
      .Get(kRfPropertyChassis,
           {.expand = RedfishQueryParamExpand({.levels = 1})})
      .Each()
      .Get(kRfPropertyDrives,
           {.auto_adjust_levels = true,
            .expand = RedfishQueryParamExpand({.levels = expand_levels})})
      .Each()
      .Do([&](auto &drive_obj) {
        return result_callback(std::move(drive_obj));
      });
}

// StorageController:
// "/redfish/v1/Systems/{id}/Storage/{id}#/StorageControllers/{id}"
void Sysmodel::QueryAllResourceInternal(Token<ResourceStorageController>,
                                        ResultCallback result_callback,
                                        size_t expand_levels) {
  auto root = redfish_intf_->GetRoot();
  root[kRfPropertySystems]
      .Each()
      .Get(kRfPropertyStorage,
           {.auto_adjust_levels = true,
            .expand = RedfishQueryParamExpand({.levels = expand_levels})})
      .Each()[kRfPropertyStorageControllers]
      .Each()
      .Do([&](auto &ctrl_obj) { return result_callback(std::move(ctrl_obj)); });
}

// Processor:
// "/redfish/v1/Systems/{id}/Processors/{id}"
void Sysmodel::QueryAllResourceInternal(Token<ResourceProcessor>,
                                        ResultCallback result_callback,
                                        size_t expand_levels) {
  auto root = redfish_intf_->GetRoot();
  root.AsIndexHelper()
      .Get(kRfPropertySystems,
           {.expand = RedfishQueryParamExpand({.levels = 1})})
      .Each()
      .Get(kRfPropertyProcessors,
           {.auto_adjust_levels = true,
            .expand = RedfishQueryParamExpand({.levels = expand_levels})})
      .Each()
      .Do([&](auto &processor_obj) {
        return result_callback(std::move(processor_obj));
      });
}

// Physical LPU (thread-granularity processor resource):
// "/redfish/v1/Systems/{id}/Processors/{id}/SubProcessors/{id}/SubProcessors/{id}"
void Sysmodel::QueryAllResourceInternal(Token<AbstractionPhysicalLpu>,
                                        ResultCallback result_callback,
                                        size_t expand_levels) {
  auto root = redfish_intf_->GetRoot();
  root.AsIndexHelper()
      .Get(kRfPropertySystems,
           {.expand = RedfishQueryParamExpand({.levels = 1})})
      .Each()
      .Get(kRfPropertyProcessors,
           {.auto_adjust_levels = true,
            .expand = RedfishQueryParamExpand({.levels = expand_levels})})
      .Each()[kRfPropertySubProcessors]  // core subprocessors collection
      .Each()[kRfPropertySubProcessors]  // thread subprocessors collection
      .Each()
      .Do([&](auto &phs_lpu_obj) {
        return result_callback(std::move(phs_lpu_obj));
      });
}

// EthernetInterface:
// "/redfish/v1/Systems/{id}/EthernetInterfaces/{id}"
void Sysmodel::QueryAllResourceInternal(Token<ResourceEthernetInterface>,
                                        ResultCallback result_callback,
                                        size_t expand_levels) {
  auto root = redfish_intf_->GetRoot();
  root.AsIndexHelper()
      .Get(kRfPropertySystems,
           {.expand = RedfishQueryParamExpand({.levels = 1})})
      .Each()[kRfPropertyEthernetInterfaces]
      .Each()
      .Do([&](auto &eth_obj) { return result_callback(std::move(eth_obj)); });
}

// Thermal:
// "/redfish/v1/Chassis/{id}/Thermal"
void Sysmodel::QueryAllResourceInternal(Token<ResourceThermal>,
                                        ResultCallback result_callback,
                                        size_t expand_levels) {
  auto root = redfish_intf_->GetRoot();
  root.AsIndexHelper()
      .Get(kRfPropertyChassis,
           {.expand = RedfishQueryParamExpand({.levels = 1})})
      .Each()[kRfPropertyThermal]
      .Do([&](auto &thermal_obj) {
        return result_callback(std::move(thermal_obj));
      });
}

// Temperatures:
// "/redfish/v1/Chassis/{id}/Thermal/Temperatures"
void Sysmodel::QueryAllResourceInternal(Token<ResourceTemperature>,
                                        ResultCallback result_callback,
                                        size_t expand_levels) {
  auto root = redfish_intf_->GetRoot();
  root.AsIndexHelper()
      .Get(kRfPropertyChassis,
           {.expand = RedfishQueryParamExpand({.levels = 1})})
      .Each()[kRfPropertyThermal][kRfPropertyTemperatures]
      .Each()
      .Do([&](auto &temp_obj) { return result_callback(std::move(temp_obj)); });
}

// Voltage:
// "/redfish/v1/Chassis/{id}/Power#/Voltages/{sensor}"
void Sysmodel::QueryAllResourceInternal(Token<ResourceVoltage>,
                                        ResultCallback result_callback,
                                        size_t expand_levels) {
  auto root = redfish_intf_->GetRoot();
  root.AsIndexHelper()
      .Get(kRfPropertyChassis,
           {.expand = RedfishQueryParamExpand({.levels = 1})})
      .Each()[kRfPropertyPower][kRfPropertyVoltages]
      .Each()
      .Do([&](auto &volt_obj) { return result_callback(std::move(volt_obj)); });
}

// Fan:
// "/redfish/v1/Chassis/{id}/Thermal/Fans"
void Sysmodel::QueryAllResourceInternal(Token<ResourceFan>,
                                        ResultCallback result_callback,
                                        size_t expand_levels) {
  auto root = redfish_intf_->GetRoot();
  root.AsIndexHelper()
      .Get(kRfPropertyChassis,
           {.expand = RedfishQueryParamExpand({.levels = 1})})
      .Each()[kRfPropertyThermal][kRfPropertyFans]
      .Each()
      .Do([&](auto &temp_obj) { return result_callback(std::move(temp_obj)); });
}

// Sensors:
// "/redfish/v1/Chassis/{id}/Sensors/{sensor}"
void Sysmodel::QueryAllResourceInternal(Token<ResourceSensor>,
                                        ResultCallback result_callback,
                                        size_t expand_levels) {
  auto root = redfish_intf_->GetRoot();
  root.AsIndexHelper()
      .Get(kRfPropertyChassis,
           {.expand = RedfishQueryParamExpand({.levels = 1})})
      .Each()[kRfPropertySensors]
      .Each()
      .Do([&](auto &sensor_obj) {
        return result_callback(std::move(sensor_obj));
      });
}

// Pcie Function:
// "/redfish/v1/Systems/{id}/PCIeDevices/{id}/PCIeFunctions/{id}"
void Sysmodel::QueryAllResourceInternal(Token<ResourcePcieFunction>,
                                        ResultCallback result_callback,
                                        size_t expand_levels) {
  auto root = redfish_intf_->GetRoot();
  root.AsIndexHelper()
      .Get(kRfPropertySystems,
           {.expand = RedfishQueryParamExpand({.levels = 1})})
      .Each()
      .Get(kRfPropertyPcieDevices,
           {.auto_adjust_levels = true,
            .expand = RedfishQueryParamExpand({.levels = expand_levels})})
      .Each()[kRfPropertyLinks][kRfPropertyPcieFunctions]
      .Each()
      .Do([&](auto &pcie_function_obj) {
        return result_callback(std::move(pcie_function_obj));
      });
}

// ComputerSystem:
// "/redfish/v1/Systems/{id}"
void Sysmodel::QueryAllResourceInternal(Token<ResourceComputerSystem>,
                                        ResultCallback result_callback,
                                        size_t expand_levels) {
  auto root = redfish_intf_->GetRoot();
  root.AsIndexHelper()
      .Get(kRfPropertySystems,
           {.expand = RedfishQueryParamExpand({.levels = 1})})
      .Each()
      .Do([&](auto &sys_obj) { return result_callback(std::move(sys_obj)); });
}

// Manager
// "/redfish/v1/Managers/{id}"
void Sysmodel::QueryAllResourceInternal(Token<ResourceManager>,
                                        ResultCallback result_callback,
                                        size_t expand_levels) {
  auto root = redfish_intf_->GetRoot();
  root[kRfPropertyManagers].Each().Do(
      [&](auto &sys_obj) { return result_callback(std::move(sys_obj)); });
}

// LogService:
// "/redfish/v1/Systems/{id}/LogServices/{id}"
void Sysmodel::QueryAllResourceInternal(Token<ResourceLogService>,
                                        ResultCallback result_callback,
                                        size_t expand_levels) {
  auto root = redfish_intf_->GetRoot();
  root.AsIndexHelper()
      .Get(kRfPropertySystems,
           {.expand = RedfishQueryParamExpand({.levels = 1})})
      .Each()[kRfPropertyLogServices]
      .Each()
      .Do([&](std::unique_ptr<RedfishObject> &service) {
        return result_callback(std::move(service));
      });
}

// LogEntry:
// "/redfish/v1/Chassis/{id}/LogServices/{id}/Entries/{id}"
void Sysmodel::QueryAllResourceInternal(Token<ResourceLogEntry>,
                                        ResultCallback result_callback,
                                        size_t expand_levels) {
  auto root = redfish_intf_->GetRoot();
  root.AsIndexHelper()
      .Get(kRfPropertyChassis,
           {.expand = RedfishQueryParamExpand({.levels = 1})})
      .Each()[kRfPropertyLogServices]
      .Each()[kRfPropertyEntries]
      .Each()
      .Do([&](std::unique_ptr<RedfishObject> &entry) {
        return result_callback(std::move(entry));
      });
}

// SoftwareInventory:
// "/redfish/v1/UpdateService/FirmwareInventory/{id}"
void Sysmodel::QueryAllResourceInternal(Token<ResourceSoftwareInventory>,
                                        ResultCallback result_callback,
                                        size_t expand_levels) {
  auto root = redfish_intf_->GetRoot();
  root[kRfPropertyUpdateService][kRfPropertyFirmwareInventory].Each().Do(
      [&](std::unique_ptr<RedfishObject> &software) {
        return result_callback(std::move(software));
      });
}

// RootOfTrust:
// "/google/v1/Chassis/RootOfTrust/"
void Sysmodel::QueryAllResourceInternal(Token<OemResourceRootOfTrust>,
                                        ResultCallback result_callback,
                                        size_t expand_levels) {
  auto root = redfish_intf_->GetRoot();
  auto root_of_trust =
      root[kRfPropertyChassis][OemPropertyRootOfTrust::Name].AsObject();
  if (root_of_trust != nullptr) {
    result_callback(std::move(root_of_trust));
  }
}

// ComponentIntegrity:
// "/redfish/v1/ComponentIntegrity/{id}"
void Sysmodel::QueryAllResourceInternal(Token<ResourceComponentIntegrity>,
                                        ResultCallback result_callback,
                                        size_t expand_levels) {
  auto root = redfish_intf_->GetRoot();
  root[kRfPropertyComponentIntegrity].Each().Do(
      [&](auto &obj) { return result_callback(std::move(obj)); });
}

// PCIeSlots:
// "/redfish/v1/Chassis/{id}/PCIeSlots"
void Sysmodel::QueryAllResourceInternal(Token<ResourcePcieSlots>,
                                        ResultCallback result_callback,
                                        size_t expand_levels) {
  auto root = redfish_intf_->GetRoot();
  root.AsIndexHelper()
      .Get(kRfPropertyChassis,
           {.expand = RedfishQueryParamExpand({.levels = 1})})
      .Each()[kRfPropertyPcieSlots]
      .Do([&](std::unique_ptr<RedfishObject> &entry) {
        return result_callback(std::move(entry));
      });
}

}  // namespace ecclesia
