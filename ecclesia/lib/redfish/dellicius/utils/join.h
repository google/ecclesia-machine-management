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

#ifndef ECCLESIA_LIB_REDFISH_DELLICIUS_UTILS_JOIN_H_
#define ECCLESIA_LIB_REDFISH_DELLICIUS_UTILS_JOIN_H_

#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"

namespace ecclesia {

// Utility function to join subqueries using "root_subquery_id" property.
// Populates "all_joined_subqueries" with one or more vectors of
// subquery ids where each vector represents chain of subqueries formed by
// resolving "root_subquery_id" relationship between subqueries.
absl::Status JoinSubqueries(
    const DelliciusQuery &query,
    absl::flat_hash_set<std::vector<std::string>> &all_joined_subqueries);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_DELLICIUS_UTILS_JOIN_H_
