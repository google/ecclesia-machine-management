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

  // A struct parameter for Querying resources. This parameter only applies to
  // the last Get call on each QueryAllResourceInternal, excluding all
  // intermediate resources
  struct QueryParams {
    size_t expand_levels = 0;
    GetParams::Freshness freshness = GetParams::Freshness::kOptional;
  };

  // QueryAllResources invokes result_callback with a RedfishObject representing
  // the desired resources of the requested type found in the Redfish backend.
  //
  // Template specializations of this function will need to be implemented in
  // order to support the various URI locations for the resources defined in the
  // Redfish Schema Supplement.
  template <typename ResourceT>
  void QueryAllResources(ResultCallback result_callback) {
    // Invoke the overload using a Token of the appropriate type.
    QueryAllResourceInternal(
        Token<ResourceT>(), result_callback,
        {.expand_levels = 0, .freshness = GetParams::Freshness::kOptional});
  }

  // QueryAllResources invokes result_callback with a RedfishObject representing
  // the desired resources of the requested type found in the Redfish backend.
  //
  // Template specializations of this function will need to be implemented in
  // order to support the various URI locations for the resources defined in the
  // Redfish Schema Supplement.
  template <typename ResourceT>
  void QueryAllResources(ResultCallback result_callback,
                         const QueryParams &query_params) {
    // Invoke the overload using a Token of the appropriate type.
    QueryAllResourceInternal(Token<ResourceT>(), result_callback, query_params);
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
                                const QueryParams &query_params);
  void QueryAllResourceInternal(Token<ResourceSystem>,
                                ResultCallback result_callback,
                                const QueryParams &query_params);
  void QueryAllResourceInternal(Token<ResourceEthernetInterface>,
                                ResultCallback result_callback,
                                const QueryParams &query_params);
  void QueryAllResourceInternal(Token<ResourceMemory>,
                                ResultCallback result_callback,
                                const QueryParams &query_params);
  void QueryAllResourceInternal(Token<ResourceStorage>,
                                ResultCallback result_callback,
                                const QueryParams &query_params);
  void QueryAllResourceInternal(Token<ResourceStorageController>,
                                ResultCallback result_callback,
                                const QueryParams &query_params);
  void QueryAllResourceInternal(Token<ResourceDrive>,
                                ResultCallback result_callback,
                                const QueryParams &query_params);
  void QueryAllResourceInternal(Token<ResourceProcessor>,
                                ResultCallback result_callback,
                                const QueryParams &query_params);
  void QueryAllResourceInternal(Token<AbstractionPhysicalLpu>,
                                ResultCallback result_callback,
                                const QueryParams &query_params);
  void QueryAllResourceInternal(Token<ResourceThermal>,
                                ResultCallback result_callback,
                                const QueryParams &query_params);
  void QueryAllResourceInternal(Token<ResourceTemperature>,
                                ResultCallback result_callback,
                                const QueryParams &query_params);
  void QueryAllResourceInternal(Token<ResourceVoltage>,
                                ResultCallback result_callback,
                                const QueryParams &query_params);
  void QueryAllResourceInternal(Token<ResourceFan>,
                                ResultCallback result_callback,
                                const QueryParams &query_params);
  void QueryAllResourceInternal(Token<ResourceSensor>,
                                ResultCallback result_callback,
                                const QueryParams &query_params);
  void QueryAllResourceInternal(Token<ResourceSensorCollection>,
                                ResultCallback result_callback,
                                const QueryParams &query_params);
  void QueryAllResourceInternal(Token<ResourcePcieFunction>,
                                ResultCallback result_callback,
                                const QueryParams &query_params);
  void QueryAllResourceInternal(Token<ResourceComputerSystem>,
                                ResultCallback result_callback,
                                const QueryParams &query_params);
  void QueryAllResourceInternal(Token<ResourceManager>,
                                ResultCallback result_callback,
                                const QueryParams &query_params);
  void QueryAllResourceInternal(Token<ResourceLogService>,
                                ResultCallback result_callback,
                                const QueryParams &query_params);
  void QueryAllResourceInternal(Token<ResourceLogEntry>,
                                ResultCallback result_callback,
                                const QueryParams &query_params);
  void QueryAllResourceInternal(Token<ResourceSoftwareInventory>,
                                ResultCallback result_callback,
                                const QueryParams &query_params);
  void QueryAllResourceInternal(Token<OemResourceRootOfTrust>,
                                ResultCallback result_callback,
                                const QueryParams &query_params);
  void QueryAllResourceInternal(Token<OemResourceRootOfTrustCollection>,
                                ResultCallback result_callback,
                                const QueryParams &query_params);
  void QueryAllResourceInternal(Token<ResourceComponentIntegrity>,
                                ResultCallback result_callback,
                                const QueryParams &query_params);
  void QueryAllResourceInternal(Token<ResourcePcieSlots>,
                                ResultCallback result_callback,
                                const QueryParams &query_params);
  void QueryAllResourceInternal(Token<ResourceSwitch>,
                                ResultCallback result_callback,
                                const QueryParams &query_params);
  RedfishInterface *redfish_intf_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_SYSMODEL_H_
