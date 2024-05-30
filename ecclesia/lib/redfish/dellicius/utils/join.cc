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

#include "ecclesia/lib/redfish/dellicius/utils/join.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/status/macros.h"

namespace ecclesia {

namespace {

// Returns a map between Parent subquery id and set of child subquery ids
// associated with the parent subquery.
absl::flat_hash_map<std::string, absl::flat_hash_set<std::string>>
BuildParentChildSubqueryMap(const DelliciusQuery& query) {
  absl::flat_hash_map<std::string, absl::flat_hash_set<std::string>>
      parent_child_subquery_map;
  for (const auto& subquery : query.subquery()) {
    for (const auto& parent_subquery_id : subquery.root_subquery_ids()) {
      parent_child_subquery_map[parent_subquery_id].insert(
          subquery.subquery_id());
    }
  }
  return parent_child_subquery_map;
}

// Returns a set of Root subquery ids.
// Iterates over all subqueries in the query to find subqueries that don't
// have root_subquery_ids defined.
absl::flat_hash_set<std::string> GetRootSubqueries(
    const DelliciusQuery& query) {
  absl::flat_hash_set<std::string> root_subqueries;
  for (const auto& subquery : query.subquery()) {
    if (subquery.root_subquery_ids().empty()) {
      root_subqueries.insert(subquery.subquery_id());
    }
  }
  return root_subqueries;
}

// Recursively joins `root_subquery_id` with associated child subqueries and
// store each joined subquery represented by vector `joined_subquery` in
// `all_joined_subqueries`.
// Returns OkStatus on successful join and InternalError when a loop is
// detected.
absl::Status JoinSubqueriesFromGivenRoot(
    const absl::flat_hash_map<std::string, absl::flat_hash_set<std::string>>&
        parent_child_subquery_map,
    std::vector<std::string>& joined_subquery,
    absl::flat_hash_set<std::vector<std::string>>& all_joined_subqueries,
    const std::string& root_subquery_id) {
  if (std::find(joined_subquery.begin(), joined_subquery.end(),
                root_subquery_id) != joined_subquery.end()) {
    return absl::InternalError(
        absl::StrCat(root_subquery_id, " already exists. Loop detected!"));
  }

  joined_subquery.push_back(root_subquery_id);

  auto maybe_child_subqueries =
      parent_child_subquery_map.find(root_subquery_id);
  if (maybe_child_subqueries == parent_child_subquery_map.end()) {
    // Leaf node found. Insert the joined subquery in global list and backtrack
    // to join other subqueries as applicable.
    all_joined_subqueries.insert(
        {joined_subquery.begin(), joined_subquery.end()});
  } else {
    for (const auto& child_subquery_id : maybe_child_subqueries->second) {
      ECCLESIA_RETURN_IF_ERROR(JoinSubqueriesFromGivenRoot(
          parent_child_subquery_map, joined_subquery, all_joined_subqueries,
          child_subquery_id));
    }
  }

  // Remove current subquery id from joined subquery to backtrack.
  joined_subquery.pop_back();
  return absl::OkStatus();
}

}  // namespace

absl::Status JoinSubqueries(
    const DelliciusQuery& query,
    absl::flat_hash_set<std::vector<std::string>>& all_joined_subqueries) {
  absl::flat_hash_set<std::string> root_subqueries = GetRootSubqueries(query);
  if (root_subqueries.empty()) {
    return absl::InvalidArgumentError("No root subqueries found in the query");
  }
  absl::flat_hash_map<std::string, absl::flat_hash_set<std::string>>
      parent_child_subquery_map = BuildParentChildSubqueryMap(query);

  for (const auto& root_subquery_id : root_subqueries) {
    std::vector<std::string> joined_subquery;
    ECCLESIA_RETURN_IF_ERROR(
        JoinSubqueriesFromGivenRoot(parent_child_subquery_map, joined_subquery,
                                    all_joined_subqueries, root_subquery_id));
  }

  return absl::OkStatus();
}

}  // namespace ecclesia
