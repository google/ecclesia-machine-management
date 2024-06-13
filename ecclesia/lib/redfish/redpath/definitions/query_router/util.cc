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

#include "ecclesia/lib/redfish/redpath/definitions/query_router/util.h"

#include <string>
#include <utility>

#include "google/protobuf/duration.pb.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ecclesia/lib/apifs/apifs.h"
#include "ecclesia/lib/redfish/dellicius/engine/query_engine.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_engine/query_spec.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_router/query_router_spec.pb.h"
#include "ecclesia/lib/status/macros.h"
#include "google/protobuf/text_format.h"

namespace ecclesia {
namespace {

template <typename T>
absl::StatusOr<T> GetProto(const std::string& filename) {
  ApifsFile fs_file(filename);
  ECCLESIA_ASSIGN_OR_RETURN(std::string data, fs_file.Read());

  T proto;
  if (!google::protobuf::TextFormat::ParseFromString(data, &proto)) {
    return absl::FailedPreconditionError(
        absl::StrCat("Failed to parse file: ", filename));
  }
  return std::move(proto);
}

void AddTimeoutFromSelectionSpec(
    const SelectionSpec::QuerySelectionSpec& selection_spec,
    QuerySpec::QueryInfo& query_info) {
  if (selection_spec.has_timeout()) {
    query_info.timeout =
        absl::Seconds(selection_spec.timeout().seconds()) +
        absl::Nanoseconds(selection_spec.timeout().nanos());
  }
}

}  // namespace
absl::Status ProcessQueryRouterSpec(
    const QueryRouterSpec& router_spec, absl::string_view server_tag,
    SelectionSpec::SelectionClass::ServerType server_type,
    absl::AnyInvocable<absl::Status(absl::string_view,
                                    const SelectionSpec::QuerySelectionSpec&)>
        process_fn) {
  for (const auto& [query_id, select_spec] : router_spec.selection_specs()) {
    for (const SelectionSpec::QuerySelectionSpec& query_select_spec :
         select_spec.query_selection_specs()) {
      if (query_select_spec.select().empty()) {
        return absl::FailedPreconditionError(absl::StrCat(
            "Both server tag and server type not specified for query: ",
            query_id));
      }
      for (const SelectionSpec::SelectionClass& select :
           query_select_spec.select()) {
        if (select.has_server_type()) {
          if (select.server_type() ==
              SelectionSpec::SelectionClass::SERVER_TYPE_UNSPECIFIED) {
            return absl::FailedPreconditionError(absl::StrCat(
                "Server type is not specified for query: ", query_id));
          }
          if (select.server_type() != server_type) {
            continue;
          }
        }
        if (select.server_tag().empty()) {
          ECCLESIA_RETURN_IF_ERROR(process_fn(query_id, query_select_spec));
          continue;
        }
        for (absl::string_view tag : select.server_tag()) {
          if (tag == server_tag) {
            ECCLESIA_RETURN_IF_ERROR(process_fn(query_id, query_select_spec));
          }
        }
      }
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<QuerySpec> GetQuerySpec(
    const QueryRouterSpec& router_spec, absl::string_view server_tag,
    SelectionSpec::SelectionClass::ServerType server_type) {
  QuerySpec query_spec;
  auto add_to_query_spec =
      [&query_spec](absl::string_view query_id,
                    const SelectionSpec::QuerySelectionSpec& selection_spec)
      -> absl::Status {
    if (selection_spec.has_query_and_rule_path()) {
      const QueryAndRulePath& query_and_rule_path =
          selection_spec.query_and_rule_path();
      if (query_and_rule_path.query_path().empty()) {
        return absl::FailedPreconditionError(
            absl::StrCat("Query path is not specified for query: ", query_id));
      }
      ECCLESIA_ASSIGN_OR_RETURN(
          DelliciusQuery query,
          GetProto<DelliciusQuery>(query_and_rule_path.query_path()));
      if (query_id != query.query_id()) {
        return absl::FailedPreconditionError(
            absl::StrCat("Query id mismatch - router spec: ", query_id,
                         " vs query spec: ", query.query_id()));
      }

      QuerySpec::QueryInfo& query_info = query_spec.query_id_to_info[query_id];
      query_info.query = std::move(query);
      AddTimeoutFromSelectionSpec(selection_spec, query_info);

      if (query_and_rule_path.rule_path().empty()) {
        return absl::OkStatus();
      }

      ECCLESIA_ASSIGN_OR_RETURN(
          QueryRules query_rules,
          GetProto<QueryRules>(query_and_rule_path.rule_path()));
      if (auto it =
              query_rules.mutable_query_id_to_params_rule()->find(query_id);
          it != query_rules.mutable_query_id_to_params_rule()->end()) {
        query_info.rule = std::move(it->second);
      }
    } else if (selection_spec.has_query_and_rule()) {
      QuerySpec::QueryInfo& query_info = query_spec.query_id_to_info[query_id];
      query_info.query = selection_spec.query_and_rule().query();
      if (selection_spec.query_and_rule().has_rule()) {
        query_info.rule = selection_spec.query_and_rule().rule();
      }
      AddTimeoutFromSelectionSpec(selection_spec, query_info);
    }
    return absl::OkStatus();
  };
  ECCLESIA_RETURN_IF_ERROR(ProcessQueryRouterSpec(
      router_spec, server_tag, server_type, add_to_query_spec));
  return std::move(query_spec);
}

}  // namespace ecclesia
