/*
 * Copyright 2024 Google LLC
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

#ifndef ECCLESIA_LIB_REDFISH_REDPATH_ENGINE_QUERY_PLANNER_IMPL_H_
#define ECCLESIA_LIB_REDFISH_REDPATH_ENGINE_QUERY_PLANNER_IMPL_H_

#include <memory>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/statusor.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/redfish/redpath/engine/normalizer.h"
#include "ecclesia/lib/redfish/redpath/engine/query_planner.h"
#include "ecclesia/lib/redfish/transport/metrical_transport.h"

namespace ecclesia {

// Configures query planner.
struct QueryPlannerOptions {
  struct RedPathRules {
    absl::flat_hash_map<std::string /* RedPath */, GetParams>
        redpath_to_query_params;
    absl::flat_hash_set<std::string /* RedPath */> redpaths_to_subscribe;
  };
  const DelliciusQuery &query;
  RedPathRules redpath_rules;
  RedpathNormalizer *normalizer;
  RedfishInterface *redfish_interface;
  MetricalRedfishTransport *metrical_transport;
};

absl::StatusOr<std::unique_ptr<QueryPlannerIntf>> BuildQueryPlanner(
    QueryPlannerOptions query_planner_options);
}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_REDPATH_ENGINE_QUERY_PLANNER_IMPL_H_
