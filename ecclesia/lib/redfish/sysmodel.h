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

#ifndef ECCLESIA_LIB_REDFISH_SYSMODEL_H_
#define ECCLESIA_LIB_REDFISH_SYSMODEL_H_

#include <functional>
#include <memory>

#include "absl/types/optional.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/property_definitions.h"

namespace libredfish {

// This helper class uses a provided redfish_intf to find resources in the
// Redfish system model.
class Sysmodel {
 public:
  Sysmodel(RedfishInterface *redfish_intf) : redfish_intf_(redfish_intf) {}
  Sysmodel(const Sysmodel &) = delete;
  Sysmodel &operator=(const Sysmodel &) = delete;

  // QueryAllResources invokes result_callback with a RedfishObject representing
  // the desired resources of the requested type found in the Redfish backend.
  //
  // Template specializations of this function will need to be implemented in
  // order to support the various URI locations for the resources defined in the
  // Redfish Schema Supplement.
  template <typename ResourceT>
  void QueryAllResources(
      const std::function<void(std::unique_ptr<RedfishObject>)>
          &result_callback) {
    // Instantiate a dummy pointer of the resource type in order to leverage
    // function overloading on the correct type.
    ResourceT *dummy;
    QueryAllResourceInternal(dummy, result_callback);
  }

 private:
  // Internal implementations for each resource type to find all instances of
  // a Redfish resource type. These functions overload QueryAllResourceInternal
  // using a typed dummy pointer as the first argument; the dummy pointer is
  // only there for function overloading and must not be used.
  void QueryAllResourceInternal(
      ResourceChassis *,
      const std::function<void(std::unique_ptr<RedfishObject>)>
          &result_callback);
  void QueryAllResourceInternal(
      ResourceSystem *,
      const std::function<void(std::unique_ptr<RedfishObject>)>
          &result_callback);
  void QueryAllResourceInternal(
      ResourceEthernetInterface *,
      const std::function<void(std::unique_ptr<RedfishObject>)>
          &result_callback);
  void QueryAllResourceInternal(
      ResourceMemory *,
      const std::function<void(std::unique_ptr<RedfishObject>)>
          &result_callback);
  void QueryAllResourceInternal(
      ResourceStorage *,
      const std::function<void(std::unique_ptr<RedfishObject>)>
          &result_callback);
  void QueryAllResourceInternal(
      ResourceDrive *, const std::function<void(std::unique_ptr<RedfishObject>)>
                           &result_callback);
  void QueryAllResourceInternal(
      ResourceProcessor *,
      const std::function<void(std::unique_ptr<RedfishObject>)>
          &result_callback);
  void QueryAllResourceInternal(
      ResourceTemperature *,
      const std::function<void(std::unique_ptr<RedfishObject>)>
          &result_callback);
  void QueryAllResourceInternal(
      ResourceVoltage *,
      const std::function<void(std::unique_ptr<RedfishObject>)>
          &result_callback);
  void QueryAllResourceInternal(
      ResourceFan *, const std::function<void(std::unique_ptr<RedfishObject>)>
                         &result_callback);
  void QueryAllResourceInternal(
      ResourceSensor *,
      const std::function<void(std::unique_ptr<RedfishObject>)>
      &result_callback);
  void QueryAllResourceInternal(
      ResourcePcieFunction *,
      const std::function<void(std::unique_ptr<RedfishObject>)>
          &result_callback);
  void QueryAllResourceInternal(
      ResourceComputerSystem *,
      const std::function<void(std::unique_ptr<RedfishObject>)>
          &result_callback);
  void QueryAllResourceInternal(
      ResourceManager *,
      const std::function<void(std::unique_ptr<RedfishObject>)>
          &result_callback);
  void QueryAllResourceInternal(
      ResourceLogService *,
      const std::function<void(std::unique_ptr<RedfishObject>)>
          &result_callback);

  RedfishInterface *redfish_intf_;
};

}  // namespace libredfish

#endif  // ECCLESIA_LIB_REDFISH_SYSMODEL_H_
