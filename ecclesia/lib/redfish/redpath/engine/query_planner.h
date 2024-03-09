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

#ifndef ECCLESIA_LIB_REDFISH_REDPATH_ENGINE_QUERY_PLANNER_H_
#define ECCLESIA_LIB_REDFISH_REDPATH_ENGINE_QUERY_PLANNER_H_

#include "absl/status/statusor.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_variables.pb.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"

namespace ecclesia {

// Provides an interface for planning and executing a RedPath Query.
class QueryPlannerIntf {
 public:
  // Configuration for the QueryPlanner.
  struct QueryExecutionOptions {
    QueryVariables &variables;
  };

  virtual ~QueryPlannerIntf() = default;

  // Plans and executes query plan relative to the service root specified in the
  // redpath query.
  virtual absl::StatusOr<QueryResult> Run(QueryExecutionOptions config) = 0;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_REDPATH_ENGINE_QUERY_PLANNER_H_
