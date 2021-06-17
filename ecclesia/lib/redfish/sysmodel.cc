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
// The following function overrides of QueryAllResources implement the search
// algorithms for the specific Redfish Resources in the URIs defined in the
// Redfish Schema Supplement. The supported URIs are non-exhaustive.

// Chassis:
// "/redfish/v1/Chassis/{id}"
void Sysmodel::QueryAllResourceInternal(
    ResourceChassis *, const std::function<void(std::unique_ptr<RedfishObject>)>
                           &result_callback) {
  auto root = redfish_intf_->GetRoot();
  root[kRfPropertyChassis].Each().Do([&](auto &chassis_obj) {
    result_callback(std::move(chassis_obj));
  });
}

// System:
// "/redfish/v1/System/{id}"
void Sysmodel::QueryAllResourceInternal(
    ResourceSystem *, const std::function<void(std::unique_ptr<RedfishObject>)>
                           &result_callback) {
  auto root = redfish_intf_->GetRoot();
  root[kRfPropertySystems].Each().Do([&](auto &sys_obj) {
    result_callback(std::move(sys_obj));
  });
}

// Memory:
// "/redfish/v1/Systems/{id}/Memory/{id}"
void Sysmodel::QueryAllResourceInternal(
    ResourceMemory *, const std::function<void(std::unique_ptr<RedfishObject>)>
                          &result_callback) {
  auto root = redfish_intf_->GetRoot();
  root[kRfPropertySystems].Each()[kRfPropertyMemory].Each()
      .Do([&](auto &memory_obj) {
        result_callback(std::move(memory_obj));
      });
}

// Storage:
// "/redfish/v1/Systems/{id}/Storage/{id}"
void Sysmodel::QueryAllResourceInternal(
    ResourceStorage *, const std::function<void(std::unique_ptr<RedfishObject>)>
                           &result_callback) {
  auto root = redfish_intf_->GetRoot();
  root[kRfPropertySystems].Each()[kRfPropertyStorage].Each()
      .Do([&](auto &storage_obj) {
        result_callback(std::move(storage_obj));
      });
}

// Drive:
// "/redfish/v1/Systems/{id}/Storage/{id}/Drives/{id}"
void Sysmodel::QueryAllResourceInternal(
    ResourceDrive *, const std::function<void(std::unique_ptr<RedfishObject>)>
                         &result_callback) {
  auto root = redfish_intf_->GetRoot();
  root[kRfPropertySystems].Each()[kRfPropertyStorage].Each()
      [kRfPropertyDrives].Each().Do([&](auto &drive_obj) {
        result_callback(std::move(drive_obj));
      });
}

// Processor:
// "/redfish/v1/Systems/{id}/Processors/{id}"
void Sysmodel::QueryAllResourceInternal(
    ResourceProcessor *,
    const std::function<void(std::unique_ptr<RedfishObject>)>
        &result_callback) {
  auto root = redfish_intf_->GetRoot();
  root[kRfPropertySystems].Each()[kRfPropertyProcessors].Each()
      .Do([&](auto &processor_obj) {
        result_callback(std::move(processor_obj));
      });
}

// EthernetInterface:
// "/redfish/v1/Systems/{id}/EthernetInterfaces/{id}"
void Sysmodel::QueryAllResourceInternal(
    ResourceEthernetInterface *,
    const std::function<void(std::unique_ptr<RedfishObject>)>
        &result_callback) {
  auto root = redfish_intf_->GetRoot();
  root[kRfPropertySystems].Each()[kRfPropertyEthernetInterfaces].Each()
      .Do([&](auto &eth_obj) {
        result_callback(std::move(eth_obj));
      });
}

// Thermal:
// "/redfish/v1/Chassis/{id}/Thermal"
void Sysmodel::QueryAllResourceInternal(
    ResourceTemperature *,
    const std::function<void(std::unique_ptr<RedfishObject>)>
        &result_callback) {
  auto root = redfish_intf_->GetRoot();
  root[kRfPropertyChassis].Each()[kRfPropertyThermal][kRfPropertyTemperatures]
      .Each().Do([&](auto &temp_obj) {
        result_callback(std::move(temp_obj));
      });
}

// Voltage:
// "/redfish/v1/Chassis/{id}/Power#/Voltages/{sensor}"
void Sysmodel::QueryAllResourceInternal(
    ResourceVoltage *, const std::function<void(std::unique_ptr<RedfishObject>)>
                           &result_callback) {
  auto root = redfish_intf_->GetRoot();
  root[kRfPropertyChassis]
      .Each()[kRfPropertyPower][kRfPropertyVoltages]
      .Each()
      .Do([&](auto &volt_obj) { result_callback(std::move(volt_obj)); });
}

// Fan:
// "/redfish/v1/Chassis/{id}/Thermal"
void Sysmodel::QueryAllResourceInternal(
    ResourceFan *,
    const std::function<void(std::unique_ptr<RedfishObject>)>
        &result_callback) {
  auto root = redfish_intf_->GetRoot();
  root[kRfPropertyChassis].Each()[kRfPropertyThermal][kRfPropertyFans]
      .Each().Do([&](auto &temp_obj) {
        result_callback(std::move(temp_obj));
      });
}

// Sensors:
// "/redfish/v1/Chassis/{id}/Sensors/{sensor}"
void Sysmodel::QueryAllResourceInternal(
    ResourceSensor *,
    const std::function<void(std::unique_ptr<RedfishObject>)>
        &result_callback) {
  auto root = redfish_intf_->GetRoot();
  root[kRfPropertyChassis].Each()[kRfPropertySensors][PropertyMembers::Name]
      .Each().Do([&](auto &sensor_obj) {
        result_callback(std::move(sensor_obj));
      });
}

// Pcie Function:
// "/redfish/v1/Systems/{id}/PCIeDevices/{id}/PCIeFunctions/{id}"
void Sysmodel::QueryAllResourceInternal(
    ResourcePcieFunction *,
    const std::function<void(std::unique_ptr<RedfishObject>)>
    &result_callback) {
  auto root = redfish_intf_->GetRoot();
  root[kRfPropertySystems].Each()[kRfPropertyPcieDevices].Each()
      [kRfPropertyLinks][kRfPropertyPcieFunctions].Each()
          .Do([&](auto &pcie_function_obj) {
            result_callback(std::move(pcie_function_obj));
          });
}

// ComputerSystem:
// "/redfish/v1/Systems/{id}"
void Sysmodel::QueryAllResourceInternal(
    ResourceComputerSystem *,
    const std::function<void(std::unique_ptr<RedfishObject>)>
        &result_callback) {
  auto root = redfish_intf_->GetRoot();
  root[kRfPropertySystems].Each().Do([&](auto &sys_obj) {
    result_callback(std::move(sys_obj));
  });
}

// Manager
// "/redfish/v1/Managers/{id}"
void Sysmodel::QueryAllResourceInternal(
    ResourceManager *, const std::function<void(std::unique_ptr<RedfishObject>)>
                           &result_callback) {
  auto root = redfish_intf_->GetRoot();
  root[kRfPropertyManagers].Each().Do(
      [&](auto &sys_obj) { result_callback(std::move(sys_obj)); });
}

}  // namespace libredfish
