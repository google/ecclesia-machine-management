/*
 * Copyright 2022 Google LLC
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

#ifndef ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_INTERNAL_INTERFACE_H_
#define ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_INTERNAL_INTERFACE_H_

#include "absl/status/statusor.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/time/clock.h"

namespace ecclesia {

// Provides an interface for normalizing a redfish response into SubqueryDataSet
// for the property specification in a Dellicius Subquery.
class Normalizer {
 public:
  virtual ~Normalizer() = default;
  virtual absl::StatusOr<SubqueryDataSet> Normalize(
      const RedfishVariant &variant,
      const DelliciusQuery::Subquery &query) const = 0;
};

// Provides an interface for executing a query plan instantiated for a Dellicius
// query.
class QueryPlannerInterface {
 public:
  virtual ~QueryPlannerInterface() = default;
  // Executes query plan using RedfishVariant as root.
  // The RedfishVariant can be the service root (redfish/v1) or any redfish
  // resource acting as local root for redfish subtree.
  // If invoked multiple times using the same result object, the datasets shall
  // be accumulated for each subquery and not replaced.
  virtual void Run(const RedfishVariant &variant, const Clock &clock,
                   DelliciusQueryResult &result) = 0;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_INTERNAL_INTERFACE_H_
