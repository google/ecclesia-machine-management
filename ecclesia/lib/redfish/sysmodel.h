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

#include <memory>

#include "absl/functional/function_ref.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/property_definitions.h"

namespace ecclesia {

// This helper class uses a provided redfish_intf to find resources in the
// Redfish system model.
class Sysmodel {
 public:
  using ResultCallback =
      absl::FunctionRef<RedfishIterReturnValue(std::unique_ptr<RedfishObject>)>;

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
  void QueryAllResources(ResultCallback result_callback) {
    // Invoke the overload using a Token of the appropriate type.
    QueryAllResourceInternal(Token<ResourceT>(), result_callback, 0);
  }

  // QueryAllResources invokes result_callback with a RedfishObject representing
  // the desired resources of the requested type found in the Redfish backend.
  //
  // Template specializations of this function will need to be implemented in
  // order to support the various URI locations for the resources defined in the
  // Redfish Schema Supplement.
  template <typename ResourceT>
  void QueryAllResources(size_t expand_levels, ResultCallback result_callback) {
    // Invoke the overload using a Token of the appropriate type.
    QueryAllResourceInternal(Token<ResourceT>(), result_callback,
                             expand_levels);
  }

 private:
  // Token used as a parameter on the QueryAllResourceInternal functions so that
  // it can be overloaded on different resources. All of the functions would
  // have the same signature otherwise and so we need this to distinguish them.
  template <typename T>
  struct Token {};

  // Internal implementations for each resource type to find all instances of
  // a Redfish resource type. These functions overload QueryAllResourceInternal
  // using a Token struct.
  void QueryAllResourceInternal(Token<ResourceChassis>,
                                ResultCallback result_callback,
                                size_t expand_levels);
  void QueryAllResourceInternal(Token<ResourceSystem>,
                                ResultCallback result_callback,
                                size_t expand_levels);
  void QueryAllResourceInternal(Token<ResourceEthernetInterface>,
                                ResultCallback result_callback,
                                size_t expand_levels);
  void QueryAllResourceInternal(Token<ResourceMemory>,
                                ResultCallback result_callback,
                                size_t expand_levels);
  void QueryAllResourceInternal(Token<ResourceStorage>,
                                ResultCallback result_callback,
                                size_t expand_levels);
  void QueryAllResourceInternal(Token<ResourceStorageController>,
                                ResultCallback result_callback,
                                size_t expand_levels);
  void QueryAllResourceInternal(Token<ResourceDrive>,
                                ResultCallback result_callback,
                                size_t expand_levels);
  void QueryAllResourceInternal(Token<ResourceProcessor>,
                                ResultCallback result_callback,
                                size_t expand_levels);
  void QueryAllResourceInternal(Token<AbstractionPhysicalLpu>,
                                ResultCallback result_callback,
                                size_t expand_levels);
  void QueryAllResourceInternal(Token<ResourceThermal>,
                                ResultCallback result_callback,
                                size_t expand_levels);
  void QueryAllResourceInternal(Token<ResourceTemperature>,
                                ResultCallback result_callback,
                                size_t expand_levels);
  void QueryAllResourceInternal(Token<ResourceVoltage>,
                                ResultCallback result_callback,
                                size_t expand_levels);
  void QueryAllResourceInternal(Token<ResourceFan>,
                                ResultCallback result_callback,
                                size_t expand_levels);
  void QueryAllResourceInternal(Token<ResourceSensor>,
                                ResultCallback result_callback,
                                size_t expand_levels);
  void QueryAllResourceInternal(Token<ResourceSensorCollection>,
                                ResultCallback result_callback,
                                size_t expand_levels);
  void QueryAllResourceInternal(Token<ResourcePcieFunction>,
                                ResultCallback result_callback,
                                size_t expand_levels);
  void QueryAllResourceInternal(Token<ResourceComputerSystem>,
                                ResultCallback result_callback,
                                size_t expand_levels);
  void QueryAllResourceInternal(Token<ResourceManager>,
                                ResultCallback result_callback,
                                size_t expand_levels);
  void QueryAllResourceInternal(Token<ResourceLogService>,
                                ResultCallback result_callback,
                                size_t expand_levels);
  void QueryAllResourceInternal(Token<ResourceLogEntry>,
                                ResultCallback result_callback,
                                size_t expand_levels);
  void QueryAllResourceInternal(Token<ResourceSoftwareInventory>,
                                ResultCallback result_callback,
                                size_t expand_levels);
  void QueryAllResourceInternal(Token<OemResourceRootOfTrust>,
                                ResultCallback result_callback,
                                size_t expand_levels);
  void QueryAllResourceInternal(Token<OemResourceRootOfTrustCollection>,
                                ResultCallback result_callback,
                                size_t expand_levels);
  void QueryAllResourceInternal(Token<ResourceComponentIntegrity>,
                                ResultCallback result_callback,
                                size_t expand_levels);
  void QueryAllResourceInternal(Token<ResourcePcieSlots>,
                                ResultCallback result_callback,
                                size_t expand_levels);
  RedfishInterface *redfish_intf_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_SYSMODEL_H_
