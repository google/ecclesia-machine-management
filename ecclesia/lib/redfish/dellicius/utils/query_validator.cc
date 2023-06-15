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
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/apifs/apifs.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/status/macros.h"
#include "google/protobuf/text_format.h"

namespace ecclesia {
namespace {

const int kDeepRedpathLimit = 5;
const int kUniqueRedpathsLimit = 5;

using Warning = RedPathQueryValidator::Warning;
using WarningFinder = std::function<bool>(const DelliciusQuery&,
                                          std::vector<Warning>&);
using ::ecclesia::DelliciusQuery;

// Returns number of nodes in the given redpath.
size_t CountNodes(absl::string_view redpath) {
  return static_cast<std::vector<absl::string_view>>(
             absl::StrSplit(redpath, '/', absl::SkipEmpty()))
      .size();
}

bool TestForDeepRedPath(const DelliciusQuery& redpath_query,
                        absl::string_view path,
                        std::vector<Warning>& warnings) {
  for (const DelliciusQuery::Subquery& subquery : redpath_query.subquery()) {
    const std::string& redpath = subquery.redpath();
    if (CountNodes(redpath) >= kDeepRedpathLimit) {
      warnings.push_back(
          Warning{.type = Warning::Type::kDeepRedPath,
                  .message = absl::StrCat("RedPath has 5+ nodes: ", redpath),
                  .path = std::string(path)});
    }
  }
  return true;
}

bool TestForDeepQuery(const DelliciusQuery& redpath_query,
                      absl::string_view path, std::vector<Warning>& warnings) {
  absl::flat_hash_set<std::string> unique_redpaths;
  for (const DelliciusQuery::Subquery& subquery : redpath_query.subquery()) {
    unique_redpaths.insert(subquery.redpath());
  }
  // Issue warnings for deep queries, deep redpaths, and wide branching.
  if (unique_redpaths.size() >= kUniqueRedpathsLimit) {
    warnings.push_back(
        Warning{.type = Warning::Type::kDeepQuery,
                .message = "There are 5+ RedPathResources in this query.",
                .path = std::string(path)});
  }
  return true;
}

bool TestForWideBranching(const DelliciusQuery& redpath_query,
                          absl::string_view path,
                          std::vector<Warning>& warnings) {
  // Map of query ids to dependant subquery redpaths.
  absl::flat_hash_map<absl::string_view, absl::flat_hash_set<absl::string_view>>
      query_id_to_child_redpaths;
  for (const DelliciusQuery::Subquery& subquery : redpath_query.subquery()) {
    for (absl::string_view root_subquery_id : subquery.root_subquery_ids()) {
      query_id_to_child_redpaths[root_subquery_id].insert(subquery.redpath());
    }
  }
  for (const auto& [query_id, child_redpaths] : query_id_to_child_redpaths) {
    if (child_redpaths.size() > 1) {
      warnings.push_back(Warning{
          .type = Warning::Type::kWideBranching,
          .message = absl::StrCat(
              "2+ RedPathResources depend on the same subquery with id: ",
              query_id),
          .path = std::string(path)});
    }
  }
  return true;
}

// Validates a given DelliciusQuery while collecting errors and warnings.
// Returns false if errors occur when validating against the most recent CSDL
// schema, true otherwise.
bool ValidateRedPathQuery(const DelliciusQuery& redpath_query,
                          std::vector<Warning>& warnings,
                          absl::string_view path) {
  // schemas b/279640460).
  // For now, no errors will occur and this method will always return true.
  return TestForDeepRedPath(redpath_query, path, warnings) &
         TestForDeepQuery(redpath_query, path, warnings) &
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

absl::Status RedPathQueryValidator::ValidateQueryFile(absl::string_view path) {
  ECCLESIA_ASSIGN_OR_RETURN(DelliciusQuery redpath_query,
                            get_redpath_query_(path));
  if (!ValidateRedPathQuery(redpath_query, warnings_, path)) {
    errors_.push_back(absl::StrCat("RedPath query failed validation: ", path));
  }
  return absl::OkStatus();
}
}  // namespace ecclesia
