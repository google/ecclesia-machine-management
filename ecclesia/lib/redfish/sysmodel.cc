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
  auto root_obj = redfish_intf_->GetRoot().AsObject();
  if (!root_obj) return;
  auto chassis_itr = root_obj->GetNode(kRfPropertyChassis).AsIterable();
  if (!chassis_itr) return;
  for (auto chassis : *chassis_itr) {
    if (std::unique_ptr<RedfishObject> obj = chassis.AsObject()) {
      result_callback(std::move(obj));
    }
  }
}

// System:
// "/redfish/v1/System/{id}"
void Sysmodel::QueryAllResourceInternal(
    ResourceSystem *, const std::function<void(std::unique_ptr<RedfishObject>)>
                           &result_callback) {
  auto root_obj = redfish_intf_->GetRoot().AsObject();
  if (!root_obj) return;
  auto sys_itr = root_obj->GetNode(kRfPropertySystems).AsIterable();
  if (!sys_itr) return;
  for (auto sys : *sys_itr) {
    if (std::unique_ptr<RedfishObject> obj = sys.AsObject()) {
      result_callback(std::move(obj));
    }
  }
}

// Memory:
// "/redfish/v1/Systems/{id}/Memory/{id}"
void Sysmodel::QueryAllResourceInternal(
    ResourceMemory *, const std::function<void(std::unique_ptr<RedfishObject>)>
                          &result_callback) {
  auto root_obj = redfish_intf_->GetRoot().AsObject();
  if (!root_obj) return;
  auto systems_itr = root_obj->GetNode(kRfPropertySystems).AsIterable();
  if (!systems_itr) return;
  for (auto system : *systems_itr) {
    if (std::unique_ptr<RedfishObject> sys_obj = system.AsObject()) {
      auto memory_itr = sys_obj->GetNode(kRfPropertyMemory).AsIterable();
      if (!memory_itr) continue;
      for (auto memory : *memory_itr) {
        if (std::unique_ptr<RedfishObject> memory_obj = memory.AsObject()) {
          result_callback(std::move(memory_obj));
        }
      }
    }
  }
}

// Storage:
// "/redfish/v1/Systems/{id}/Storage/{id}"
void Sysmodel::QueryAllResourceInternal(
    ResourceStorage *, const std::function<void(std::unique_ptr<RedfishObject>)>
                           &result_callback) {
  auto root_obj = redfish_intf_->GetRoot().AsObject();
  if (!root_obj) return;
  auto systems_itr = root_obj->GetNode(kRfPropertySystems).AsIterable();
  if (!systems_itr) return;
  for (auto system : *systems_itr) {
    if (std::unique_ptr<RedfishObject> sys_obj = system.AsObject()) {
      auto storage_itr = sys_obj->GetNode(kRfPropertyStorage).AsIterable();
      if (!storage_itr) continue;
      for (auto storage : *storage_itr) {
        if (std::unique_ptr<RedfishObject> storage_obj = storage.AsObject()) {
          result_callback(std::move(storage_obj));
        }
      }
    }
  }
}

// Drive:
// "/redfish/v1/Systems/{id}/Storage/{id}/Drives/{id}"
void Sysmodel::QueryAllResourceInternal(
    ResourceDrive *, const std::function<void(std::unique_ptr<RedfishObject>)>
                         &result_callback) {
  auto root_obj = redfish_intf_->GetRoot().AsObject();
  if (!root_obj) return;
  auto systems_itr = root_obj->GetNode(kRfPropertySystems).AsIterable();
  if (!systems_itr) return;
  for (auto system : *systems_itr) {
    if (std::unique_ptr<RedfishObject> sys_obj = system.AsObject()) {
      auto storage_itr = sys_obj->GetNode(kRfPropertyStorage).AsIterable();
      if (!storage_itr) continue;
      for (auto storage : *storage_itr) {
        if (std::unique_ptr<RedfishObject> storage_obj = storage.AsObject()) {
          auto drive_itr = storage_obj->GetNode(kRfPropertyDrives).AsIterable();
          if (!drive_itr) continue;
          for (auto drive : *drive_itr) {
            if (std::unique_ptr<RedfishObject> drive_obj = drive.AsObject()) {
              result_callback(std::move(drive_obj));
            }
          }
        }
      }
    }
  }
}

// Processor:
// "/redfish/v1/Systems/{id}/Processors/{id}"
void Sysmodel::QueryAllResourceInternal(
    ResourceProcessor *,
    const std::function<void(std::unique_ptr<RedfishObject>)>
        &result_callback) {
  auto root_obj = redfish_intf_->GetRoot().AsObject();
  if (!root_obj) return;
  auto systems_itr = root_obj->GetNode(kRfPropertySystems).AsIterable();
  if (!systems_itr) return;
  for (auto system : *systems_itr) {
    if (std::unique_ptr<RedfishObject> sys_obj = system.AsObject()) {
      auto processor_itr = sys_obj->GetNode(kRfPropertyProcessors).AsIterable();
      if (!processor_itr) continue;
      for (auto processor : *processor_itr) {
        if (std::unique_ptr<RedfishObject> processor_obj =
                processor.AsObject()) {
          result_callback(std::move(processor_obj));
        }
      }
    }
  }
}

// EthernetInterface:
// "/redfish/v1/Systems/{id}/EthernetInterfaces/{id}"
void Sysmodel::QueryAllResourceInternal(
    ResourceEthernetInterface *,
    const std::function<void(std::unique_ptr<RedfishObject>)>
        &result_callback) {
  auto root_obj = redfish_intf_->GetRoot().AsObject();
  if (!root_obj) return;
  auto systems_itr = root_obj->GetNode(kRfPropertySystems).AsIterable();
  if (!systems_itr) return;
  for (auto system : *systems_itr) {
    if (std::unique_ptr<RedfishObject> sys_obj = system.AsObject()) {
      auto eth_itr =
          sys_obj->GetNode(kRfPropertyEthernetInterfaces).AsIterable();
      if (!eth_itr) continue;
      for (auto eth : *eth_itr) {
        if (std::unique_ptr<RedfishObject> eth_obj = eth.AsObject()) {
          result_callback(std::move(eth_obj));
        }
      }
    }
  }
}

// Thermal:
// "/redfish/v1/Chassis/{id}/Thermal"
void Sysmodel::QueryAllResourceInternal(
    ResourceTemperature *,
    const std::function<void(std::unique_ptr<RedfishObject>)>
        &result_callback) {
  auto root_obj = redfish_intf_->GetRoot().AsObject();
  if (!root_obj) return;
  auto chassis_itr = root_obj->GetNode(kRfPropertyChassis).AsIterable();
  if (!chassis_itr) return;
  for (auto chassis : *chassis_itr) {
    if (std::unique_ptr<RedfishObject> chassis_obj = chassis.AsObject()) {
      if (std::unique_ptr<RedfishObject> thermal_obj =
         chassis_obj->GetNode(kRfPropertyThermal).AsObject()) {
        if (auto temp_itr =
                thermal_obj->GetNode(kRfPropertyTemperatures).AsIterable()) {
          for (auto temp : *temp_itr) {
            if (std::unique_ptr<RedfishObject> temp_obj = temp.AsObject()) {
              result_callback(std::move(temp_obj));
            }
          }
        }
      }
    }
  }
}

// Pcie Function:
// "/redfish/v1/Systems/{id}/PCIeDevices/{id}/PCIeFunctions/{id}"
void Sysmodel::QueryAllResourceInternal(
    ResourcePcieFunction *,
    const std::function<void(std::unique_ptr<RedfishObject>)>
        &result_callback) {
  auto root_obj = redfish_intf_->GetRoot().AsObject();
  if (!root_obj) return;
  auto systems_itr = root_obj->GetNode(kRfPropertySystems).AsIterable();
  if (!systems_itr) return;
  for (auto system : *systems_itr) {
    if (std::unique_ptr<RedfishObject> sys_obj = system.AsObject()) {
      auto pcie_devices_itr =
          sys_obj->GetNode(kRfPropertyPcieDevices).AsIterable();
      if (!pcie_devices_itr) continue;
      for (auto pcie_device : *pcie_devices_itr) {
        if (std::unique_ptr<RedfishObject> pcie_device_obj =
                pcie_device.AsObject()) {
          auto links_obj =
              pcie_device_obj->GetNode(kRfPropertyLinks).AsObject();
          if (!links_obj) continue;
          auto pcie_function_itr =
              links_obj->GetNode(kRfPropertyPcieFunctions).AsIterable();
          if (!pcie_function_itr) continue;
          for (auto pcie_function : *pcie_function_itr) {
            if (std::unique_ptr<RedfishObject> pcie_function_obj =
                    pcie_function.AsObject()) {
              result_callback(std::move(pcie_function_obj));
            }
          }
        }
      }
    }
  }
}

// ComputerSystem:
// "/redfish/v1/Systems/{id}"
void Sysmodel::QueryAllResourceInternal(
    ResourceComputerSystem *,
    const std::function<void(std::unique_ptr<RedfishObject>)>
        &result_callback) {
  auto root_obj = redfish_intf_->GetRoot().AsObject();
  if (!root_obj) return;
  auto systems_itr = root_obj->GetNode(kRfPropertySystems).AsIterable();
  if (!systems_itr) return;
  for (auto system : *systems_itr) {
    if (std::unique_ptr<RedfishObject> sys_obj = system.AsObject()) {
      result_callback(std::move(sys_obj));
    }
  }
}

}  // namespace libredfish
