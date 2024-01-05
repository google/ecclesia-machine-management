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

#include "ecclesia/lib/redfish/dellicius/utils/query_validator.h"

#include <fcntl.h>
#include <cstddef>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/apifs/apifs.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/status/macros.h"
#include "google/protobuf/text_format.h"

namespace ecclesia {
namespace {

constexpr absl::string_view kTopLevelQueryId = "TopLevelQuery";

const int kDeepRedpathLimit = 5;
const int kUniqueRedpathsLimit = 5;

using Issue = RedPathQueryValidator::Issue;
using Subquery = DelliciusQuery::Subquery;

// Returns number of nodes in the given redpath.
size_t CountNodes(absl::string_view redpath) {
  return static_cast<std::vector<absl::string_view>>(
             absl::StrSplit(redpath, '/', absl::SkipEmpty()))
      .size();
}

void CheckForSubqueryIdRootPropertyConflicts(
    const Subquery& subquery, absl::string_view subquery_id,
    const absl::flat_hash_map<absl::string_view,
                              absl::flat_hash_set<absl::string_view>>&
        subquery_id_to_property_names,
    absl::string_view path, std::vector<Issue>& errors) {
  for (absl::string_view root_id : subquery.root_subquery_ids()) {
    const auto it = subquery_id_to_property_names.find(root_id);
    if (it == subquery_id_to_property_names.end()) continue;
    // Lookup the subquery id in the root subquery's properties.
    if (it->second.contains(subquery_id)) {
      errors.push_back(Issue{
          .type = Issue::Type::kConflictingIds,
          .message = absl::StrCat(
              "Subquery id ", subquery_id,
              " conflicts with a property in the root subquery ", root_id),
          .path = std::string(path)});
    }
  }
}

// Populates errors if the given subquery has an id matching another subquery
// under the same root subquery. Updates the parent_to_child_subqueries map.
void CheckForDuplicateSubqueryIds(
    const Subquery& subquery, absl::string_view subquery_id,
    absl::string_view path,
    absl::flat_hash_map<absl::string_view,
                        absl::flat_hash_set<absl::string_view>>&
        parent_to_child_subqueries,
    std::vector<Issue>& errors) {
  // For the top level subqueries, associate them with a top level root id.
  std::vector<absl::string_view> root_subquery_ids(
      subquery.root_subquery_ids().begin(), subquery.root_subquery_ids().end());
  if (root_subquery_ids.empty()) {
    root_subquery_ids.push_back(kTopLevelQueryId);
  }
  // Check if the current subquery id is already a child of the root subquery.
  // If not, mark it as such.
  for (absl::string_view root_id : root_subquery_ids) {
    const auto it = parent_to_child_subqueries.find(root_id);
    if (it == parent_to_child_subqueries.end()) {
      // Create new mapping from the root to this child subquery id.
      parent_to_child_subqueries[root_id].insert(subquery_id);
      continue;
    }
    // Insert current subquery id as a child of the root subquery.
    const auto [child_it, inserted] = it->second.insert(subquery_id);
    // If this subquery id already exists as a child, conflict found.
    if (!inserted) {
      errors.push_back(Issue{
          .type = Issue::Type::kConflictingIds,
          .message = absl::StrCat(
              "Multiple subqueries found with the subquery id: ", subquery_id),
          .path = std::string(path)});
    }
  }
}

// Populates errors for the following issues.
// 1. Duplicate subquery ids present within the same root subquery, or at the
// top level.
// 2. Duplicate property names within a subquery id.
// 3  Conflict between a subquery id and its root subquery's property names.
void TestForConflictingIds(const DelliciusQuery& redpath_query,
                           absl::string_view path, std::vector<Issue>& errors) {
  // map of a subquery's id to all its property names.
  absl::flat_hash_map<absl::string_view, absl::flat_hash_set<absl::string_view>>
      subquery_id_to_property_names;
  subquery_id_to_property_names.reserve(redpath_query.subquery_size());
  // map of parent subquery ids to its child subquery ids.
  absl::flat_hash_map<absl::string_view, absl::flat_hash_set<absl::string_view>>
      parent_to_child_subqueries;

  for (const Subquery& subquery : redpath_query.subquery()) {
    // Check for subquery id conflicts.
    absl::string_view subquery_id = subquery.subquery_id();
    CheckForDuplicateSubqueryIds(subquery, subquery_id, path,
                                 parent_to_child_subqueries, errors);

    // Check for property name conflicts within a subquery.
    absl::flat_hash_set<absl::string_view> property_names;
    property_names.reserve(subquery.properties_size());
    for (const Subquery::RedfishProperty& property : subquery.properties()) {
      absl::string_view effective_name =
          property.has_name() ? property.name() : property.property();
      if (property_names.contains(effective_name)) {
        errors.push_back(Issue{
            .type = Issue::Type::kConflictingIds,
            .message =
                absl::StrCat("Multiple properties with same name detected: ",
                             effective_name),
            .path = std::string(path)});
      }
      property_names.insert(effective_name);
    }
    subquery_id_to_property_names.insert({subquery_id, property_names});
    // Check that a subquery id doesn't match any property names in the
    // root subquery.
    CheckForSubqueryIdRootPropertyConflicts(
        subquery, subquery_id, subquery_id_to_property_names, path, errors);
  }
}

void TestForDeepRedPath(const DelliciusQuery& redpath_query,
                        absl::string_view path, std::vector<Issue>& warnings) {
  for (const Subquery& subquery : redpath_query.subquery()) {
    const std::string& redpath = subquery.redpath();
    if (CountNodes(redpath) >= kDeepRedpathLimit) {
      warnings.push_back(
          Issue{.type = Issue::Type::kDeepRedPath,
                .message = absl::StrCat("RedPath has 5+ nodes: ", redpath),
                .path = std::string(path)});
    }
  }
}

void TestForDeepQuery(const DelliciusQuery& redpath_query,
                      absl::string_view path, std::vector<Issue>& warnings) {
  absl::flat_hash_set<std::string> unique_redpaths;
  for (const Subquery& subquery : redpath_query.subquery()) {
    unique_redpaths.insert(subquery.redpath());
  }
  // Issue warnings for deep queries, deep redpaths, and wide branching.
  if (unique_redpaths.size() >= kUniqueRedpathsLimit) {
    warnings.push_back(
        Issue{.type = Issue::Type::kDeepQuery,
              .message = "There are 5+ RedPathResources in this query.",
              .path = std::string(path)});
  }
}

void TestForWideBranching(const DelliciusQuery& redpath_query,
                          absl::string_view path,
                          std::vector<Issue>& warnings) {
  // Map of query ids to dependant subquery redpaths.
  absl::flat_hash_map<absl::string_view, absl::flat_hash_set<absl::string_view>>
      query_id_to_child_redpaths;
  for (const Subquery& subquery : redpath_query.subquery()) {
    for (absl::string_view root_subquery_id : subquery.root_subquery_ids()) {
      query_id_to_child_redpaths[root_subquery_id].insert(subquery.redpath());
    }
  }
  for (const auto& [query_id, child_redpaths] : query_id_to_child_redpaths) {
    if (child_redpaths.size() > 1) {
      warnings.push_back(
          Issue{.type = Issue::Type::kWideBranching,
                .message = absl::StrCat(
                    "2+ RedPathResources depend on the same subquery with id: ",
                    query_id),
                .path = std::string(path)});
    }
  }
}

void TestForErrorLevelIssues(const DelliciusQuery& redpath_query,
                             absl::string_view path,
                             std::vector<Issue>& errors) {
  TestForConflictingIds(redpath_query, path, errors);
}

void TestForWarningLevelIssues(const DelliciusQuery& redpath_query,
                               absl::string_view path,
                               std::vector<Issue>& warnings) {
  TestForDeepRedPath(redpath_query, path, warnings);
  TestForDeepQuery(redpath_query, path, warnings);
  TestForWideBranching(redpath_query, path, warnings);
}

}  // namespace

absl::StatusOr<DelliciusQuery> RedPathQueryValidator::GetRedPathQuery(
    absl::string_view path) {
  ApifsFile file_reader((std::string(path)));
  ECCLESIA_ASSIGN_OR_RETURN(std::string file_contents, file_reader.Read());
  // Parse proto string to redpath query message.
  DelliciusQuery query;
  if (!google::protobuf::TextFormat::ParseFromString(file_contents, &query)) {
    return absl::InvalidArgumentError(
        absl::StrCat("Failed to parse message to DelliciusQuery: ", path));
  }
  return query;
}

// Performs Query Validation given a path to the query file.
absl::Status RedPathQueryValidator::ValidateQueryFile(absl::string_view path) {
  ECCLESIA_ASSIGN_OR_RETURN(DelliciusQuery redpath_query,
                            get_redpath_query_(path));
  // schemas b/279640460).
  TestForErrorLevelIssues(redpath_query, path, errors_);
  TestForWarningLevelIssues(redpath_query, path, warnings_);
  return absl::OkStatus();
}

// Validates a DelliciusQuery object directly.
absl::Status RedPathQueryValidator::ValidateQuery(
    const DelliciusQuery& redpath_query, absl::string_view path) {
  // schemas b/279640460).
  TestForErrorLevelIssues(redpath_query, path, errors_);
  TestForWarningLevelIssues(redpath_query, path, warnings_);
  return absl::OkStatus();
}
}  // namespace ecclesia
