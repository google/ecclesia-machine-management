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

namespace libredfish {

inline constexpr char kGoogleRoot[] = "/redfish/v1";

// The following function overrides of QueryAllResources implement the search
// algorithms for the specific Redfish Resources in the URIs defined in the
// Redfish Schema Supplement. The supported URIs are non-exhaustive.

// Chassis:
// "/redfish/v1/Chassis/{id}"
void Sysmodel::QueryAllResourceInternal(
    Token<ResourceChassis>,
    absl::FunctionRef<RedfishIterReturnValue(std::unique_ptr<RedfishObject>)>
        result_callback) {
  auto root = redfish_intf_->GetRoot();
  root[kRfPropertyChassis].Each().Do([&](auto &chassis_obj) {
    return result_callback(std::move(chassis_obj));
  });
}

// System:
// "/redfish/v1/System/{id}"
void Sysmodel::QueryAllResourceInternal(
    Token<ResourceSystem>,
    absl::FunctionRef<RedfishIterReturnValue(std::unique_ptr<RedfishObject>)>
        result_callback) {
  auto root = redfish_intf_->GetRoot();
  root[kRfPropertySystems].Each().Do(
      [&](auto &sys_obj) { return result_callback(std::move(sys_obj)); });
}

// Memory:
// "/redfish/v1/Systems/{id}/Memory/{id}"
void Sysmodel::QueryAllResourceInternal(
    Token<ResourceMemory>,
    absl::FunctionRef<RedfishIterReturnValue(std::unique_ptr<RedfishObject>)>
        result_callback) {
  auto root = redfish_intf_->GetRoot();
  root[kRfPropertySystems].Each()[kRfPropertyMemory].Each().Do(
      [&](auto &memory_obj) { return result_callback(std::move(memory_obj)); });
}

// Storage:
// "/redfish/v1/Systems/{id}/Storage/{id}"
void Sysmodel::QueryAllResourceInternal(
    Token<ResourceStorage>,
    absl::FunctionRef<RedfishIterReturnValue(std::unique_ptr<RedfishObject>)>
        result_callback) {
  auto root = redfish_intf_->GetRoot();
  root[kRfPropertySystems].Each()[kRfPropertyStorage].Each().Do(
      [&](auto &storage_obj) {
        return result_callback(std::move(storage_obj));
      });
}

// Drive:
// "/redfish/v1/Systems/{id}/Storage/{id}/Drives/{id}"
void Sysmodel::QueryAllResourceInternal(
    Token<ResourceDrive>,
    absl::FunctionRef<RedfishIterReturnValue(std::unique_ptr<RedfishObject>)>
        result_callback) {
  auto root = redfish_intf_->GetRoot();
  root[kRfPropertySystems]
      .Each()[kRfPropertyStorage]
      .Each()[kRfPropertyDrives]
      .Each()
      .Do([&](auto &drive_obj) {
        return result_callback(std::move(drive_obj));
      });
}

// Processor:
// "/redfish/v1/Systems/{id}/Processors/{id}"
void Sysmodel::QueryAllResourceInternal(
    Token<ResourceProcessor>,
    absl::FunctionRef<RedfishIterReturnValue(std::unique_ptr<RedfishObject>)>
        result_callback) {
  auto root = redfish_intf_->GetRoot();
  root[kRfPropertySystems].Each()[kRfPropertyProcessors].Each().Do(
      [&](auto &processor_obj) {
        return result_callback(std::move(processor_obj));
      });
}

// EthernetInterface:
// "/redfish/v1/Systems/{id}/EthernetInterfaces/{id}"
void Sysmodel::QueryAllResourceInternal(
    Token<ResourceEthernetInterface>,
    absl::FunctionRef<RedfishIterReturnValue(std::unique_ptr<RedfishObject>)>
        result_callback) {
  auto root = redfish_intf_->GetRoot();
  root[kRfPropertySystems].Each()[kRfPropertyEthernetInterfaces].Each().Do(
      [&](auto &eth_obj) { return result_callback(std::move(eth_obj)); });
}

// Thermal:
// "/redfish/v1/Chassis/{id}/Thermal"
void Sysmodel::QueryAllResourceInternal(
    Token<ResourceTemperature>,
    absl::FunctionRef<RedfishIterReturnValue(std::unique_ptr<RedfishObject>)>
        result_callback) {
  auto root = redfish_intf_->GetRoot();
  root[kRfPropertyChassis]
      .Each()[kRfPropertyThermal][kRfPropertyTemperatures]
      .Each()
      .Do([&](auto &temp_obj) { return result_callback(std::move(temp_obj)); });
}

// Voltage:
// "/redfish/v1/Chassis/{id}/Power#/Voltages/{sensor}"
void Sysmodel::QueryAllResourceInternal(
    Token<ResourceVoltage>,
    absl::FunctionRef<RedfishIterReturnValue(std::unique_ptr<RedfishObject>)>
        result_callback) {
  auto root = redfish_intf_->GetRoot();
  root[kRfPropertyChassis]
      .Each()[kRfPropertyPower][kRfPropertyVoltages]
      .Each()
      .Do([&](auto &volt_obj) { return result_callback(std::move(volt_obj)); });
}

// Fan:
// "/redfish/v1/Chassis/{id}/Thermal"
void Sysmodel::QueryAllResourceInternal(
    Token<ResourceFan>,
    absl::FunctionRef<RedfishIterReturnValue(std::unique_ptr<RedfishObject>)>
        result_callback) {
  auto root = redfish_intf_->GetRoot();
  root[kRfPropertyChassis]
      .Each()[kRfPropertyThermal][kRfPropertyFans]
      .Each()
      .Do([&](auto &temp_obj) { return result_callback(std::move(temp_obj)); });
}

// Sensors:
// "/redfish/v1/Chassis/{id}/Sensors/{sensor}"
void Sysmodel::QueryAllResourceInternal(
    Token<ResourceSensor>,
    absl::FunctionRef<RedfishIterReturnValue(std::unique_ptr<RedfishObject>)>
        result_callback) {
  auto root = redfish_intf_->GetRoot();
  root[kRfPropertyChassis]
      .Each()[kRfPropertySensors][PropertyMembers::Name]
      .Each()
      .Do([&](auto &sensor_obj) {
        return result_callback(std::move(sensor_obj));
      });
}

// Pcie Function:
// "/redfish/v1/Systems/{id}/PCIeDevices/{id}/PCIeFunctions/{id}"
void Sysmodel::QueryAllResourceInternal(
    Token<ResourcePcieFunction>,
    absl::FunctionRef<RedfishIterReturnValue(std::unique_ptr<RedfishObject>)>
        result_callback) {
  auto root = redfish_intf_->GetRoot();
  root[kRfPropertySystems]
      .Each()[kRfPropertyPcieDevices]
      .Each()[kRfPropertyLinks][kRfPropertyPcieFunctions]
      .Each()
      .Do([&](auto &pcie_function_obj) {
        return result_callback(std::move(pcie_function_obj));
      });
}

// ComputerSystem:
// "/redfish/v1/Systems/{id}"
void Sysmodel::QueryAllResourceInternal(
    Token<ResourceComputerSystem>,
    absl::FunctionRef<RedfishIterReturnValue(std::unique_ptr<RedfishObject>)>
        result_callback) {
  auto root = redfish_intf_->GetRoot();
  root[kRfPropertySystems].Each().Do(
      [&](auto &sys_obj) { return result_callback(std::move(sys_obj)); });
}

// Manager
// "/redfish/v1/Managers/{id}"
void Sysmodel::QueryAllResourceInternal(
    Token<ResourceManager>,
    absl::FunctionRef<RedfishIterReturnValue(std::unique_ptr<RedfishObject>)>
        result_callback) {
  auto root = redfish_intf_->GetRoot();
  root[kRfPropertyManagers].Each().Do(
      [&](auto &sys_obj) { return result_callback(std::move(sys_obj)); });
}

// LogService:
// "/redfish/v1/Systems/{id}/LogServices/{id}"
void Sysmodel::QueryAllResourceInternal(
    Token<ResourceLogService>,
    absl::FunctionRef<RedfishIterReturnValue(std::unique_ptr<RedfishObject>)>
        result_callback) {
  auto root = redfish_intf_->GetRoot();
  root[kRfPropertySystems].Each()[kRfPropertyLogServices].Each().Do(
      [&](std::unique_ptr<RedfishObject> &service) {
        return result_callback(std::move(service));
      });
}

// LogEntry:
// "/redfish/v1/Chassis/{id}/LogServices/{id}/Entries/{id}"
void Sysmodel::QueryAllResourceInternal(
    Token<ResourceLogEntry>,
    absl::FunctionRef<RedfishIterReturnValue(std::unique_ptr<RedfishObject>)>
        result_callback) {
  auto root = redfish_intf_->GetRoot();
  root[kRfPropertyChassis]
      .Each()[kRfPropertyLogServices]
      .Each()[kRfPropertyEntries]
      .Each()
      .Do([&](std::unique_ptr<RedfishObject> &entry) {
        return result_callback(std::move(entry));
      });
}

// SoftwareInventory:
// "/redfish/v1/UpdateService/FirmwareInventory/{id}"
void Sysmodel::QueryAllResourceInternal(
    Token<ResourceSoftwareInventory>,
    absl::FunctionRef<RedfishIterReturnValue(std::unique_ptr<RedfishObject>)>
        result_callback) {
  auto root = redfish_intf_->GetRoot();
  root[kRfPropertyUpdateService][kRfPropertyFirmwareInventory].Each().Do(
      [&](std::unique_ptr<RedfishObject> &software) {
        return result_callback(std::move(software));
      });
}

// RootOfTrust:
// "/google/v1/Chassis/RootOfTrust/"
void Sysmodel::QueryAllResourceInternal(
    Token<OemResourceRootOfTrust>,
    absl::FunctionRef<RedfishIterReturnValue(std::unique_ptr<RedfishObject>)>
        result_callback) {
  auto root = redfish_intf_->GetUri(kGoogleRoot);
  auto root_of_trust =
      root[kRfPropertyChassis][OemPropertyRootOfTrust::Name].AsObject();
  if (root_of_trust != nullptr) {
    result_callback(std::move(root_of_trust));
  }
}

}  // namespace libredfish
