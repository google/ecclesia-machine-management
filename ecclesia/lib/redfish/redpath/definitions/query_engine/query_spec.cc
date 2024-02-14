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

#include "ecclesia/lib/redfish/redpath/definitions/query_engine/query_spec.h"

#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "ecclesia/lib/apifs/apifs.h"
#include "ecclesia/lib/file/cc_embed_interface.h"
#include "ecclesia/lib/status/macros.h"
#include "ecclesia/lib/time/clock.h"
#include "google/protobuf/text_format.h"

namespace ecclesia {

absl::StatusOr<QuerySpec> QuerySpec::FromQueryContext(
    const QueryContext &query_context) {
  QuerySpec query_spec;
  for (const EmbeddedFile &query_file : query_context.query_files) {
    DelliciusQuery query;

      if (!google::protobuf::TextFormat::ParseFromString(std::string(query_file.data),
                                               &query)) {
      return absl::InternalError(absl::StrCat(
          "Cannot get RedPath query from embedded file ", query_file.name));
    }
    std::string query_id = query.query_id();
    auto [it, inserted] = query_spec.query_id_to_info.insert(
        {query_id, QuerySpec::QueryInfo{.query = std::move(query)}});
    if (!inserted) {
      return absl::InternalError(
          absl::StrCat("Duplicate query for: ", query_id));
    }
  }
  for (const EmbeddedFile &query_rule : query_context.query_rules) {
    QueryRules rules;
    if (!google::protobuf::TextFormat::ParseFromString(std::string(query_rule.data),
        &rules)) {
      return absl::InternalError(
          absl::StrCat("Cannot get RedPath query rules from embedded file ",
                       query_rule.name));
    }
    for (auto &[query_id, rule] : *rules.mutable_query_id_to_params_rule()) {
      if (auto it = query_spec.query_id_to_info.find(query_id);
          it != query_spec.query_id_to_info.end()) {
        it->second.rule = std::move(rule);
      }
    }
  }
  query_spec.clock = query_context.clock;
  return query_spec;
}

absl::StatusOr<QuerySpec> QuerySpec::FromQueryFiles(
    absl::Span<const std::string> query_files,
    absl::Span<const std::string> query_rules, const Clock *clock) {
  std::vector<EmbeddedFile> query_files_embedded;
  std::vector<EmbeddedFile> query_rules_embedded;

  std::vector<std::string> query_files_data;
  query_files_data.reserve(query_files.size());
  for (const std::string &query_file : query_files) {
    ApifsFile query_data_file(query_file);
    ECCLESIA_ASSIGN_OR_RETURN(std::string query_data, query_data_file.Read());
    query_files_data.push_back(std::move(query_data));
    query_files_embedded.push_back(
        EmbeddedFile{.name = query_file, .data = query_files_data.back()});
  }

  std::vector<std::string> query_rules_data;
  query_rules_data.reserve(query_rules.size());
  for (const std::string &query_rule : query_rules) {
    ApifsFile query_rule_file(query_rule);
    ECCLESIA_ASSIGN_OR_RETURN(std::string rule_data, query_rule_file.Read());
    query_rules_data.push_back(std::move(rule_data));
    query_rules_embedded.push_back(
        EmbeddedFile{.name = query_rule, .data = query_rules_data.back()});
  }

  QueryContext query_context = {.query_files = query_files_embedded,
                                .query_rules = query_rules_embedded,
                                .clock = clock};

  return QuerySpec::FromQueryContext(query_context);
}

}  // namespace ecclesia
