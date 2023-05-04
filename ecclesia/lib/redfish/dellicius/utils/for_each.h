/*
 * Copyright 2023 Google LLC
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

#ifndef ECCLESIA_LIB_REDFISH_DELLICIUS_UTILS_FOR_EACH_H_
#define ECCLESIA_LIB_REDFISH_DELLICIUS_UTILS_FOR_EACH_H_

#include <vector>

#include "absl/functional/any_invocable.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"

namespace ecclesia {

// Utility function to visit all the datasets in all the subqueries in the query
// result. This can be used to walk through all the level-1 dataset across all
// subquery results.
void ForEachDataSet(
    const std::vector<ecclesia::DelliciusQueryResult>& result,
    absl::AnyInvocable<void(const ecclesia::SubqueryDataSet&)> visitor);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_DELLICIUS_UTILS_FOR_EACH_H_
