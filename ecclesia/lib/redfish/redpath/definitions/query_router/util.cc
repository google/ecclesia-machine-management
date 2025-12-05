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

#include <algorithm>
#include <optional>
#include <string>
#include <utility>

#include "google/protobuf/duration.pb.h"
#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ecclesia/lib/apifs/apifs.h"
#include "ecclesia/lib/redfish/dellicius/engine/query_engine.h"
#include "ecclesia/lib/redfish/redpath/definitions/node_class/node_class_assignment.pb.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_engine/query_spec.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_router/query_router_spec.pb.h"
#include "ecclesia/lib/status/macros.h"
#include "ecclesia/lib/time/clock.h"
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

SelectionSpec::SelectionClass::ServerClass ServerClassFromNodeClass(
    NodeClassAssignment::NodeClass node_class) {
  switch (node_class) {
    case NodeClassAssignment::NODE_CLASS_COMPUTE:
      return SelectionSpec::SelectionClass::SERVER_CLASS_COMPUTE;
    default:
      return SelectionSpec::SelectionClass::SERVER_CLASS_UNSPECIFIED;
  }
}

}  // namespace

using NodeClass = NodeClassAssignment::NodeClass;

absl::Status ExecuteOnMatchingSelections(
    const google::protobuf::RepeatedPtrField<SelectionSpec::SelectionClass>& select_specs,
    absl::string_view server_tag,
    SelectionSpec::SelectionClass::ServerType server_type,
    std::optional<SelectionSpec::SelectionClass::ServerClass> server_class,
    absl::AnyInvocable<absl::Status()> execute_fn) {
  for (const SelectionSpec::SelectionClass& select : select_specs) {
    if (!select.has_server_type() && select.server_tag().empty() &&
        select.node_class().empty() && select.server_class().empty()) {
      // This means that no select conditions are specified.
      return absl::FailedPreconditionError("No select conditions specified");
    }
    bool server_type_matched = true;
    if (select.has_server_type()) {
      if (select.server_type() ==
          SelectionSpec::SelectionClass::SERVER_TYPE_UNSPECIFIED) {
        return absl::FailedPreconditionError(
            "Server type cannot be SERVER_TYPE_UNSPECIFIED");
      }
      server_type_matched = select.server_type() == server_type;
    }
    bool server_tag_matched =
        select.server_tag().empty()
            ? true
            : std::find(select.server_tag().begin(), select.server_tag().end(),
                        server_tag) != select.server_tag().end();
    bool node_class_matched = true;
    if (!select.node_class().empty() && server_class.has_value()) {
      node_class_matched = false;
      for (int node_class_select : select.node_class()) {
        if (node_class_select == NodeClassAssignment::NODE_CLASS_UNSPECIFIED) {
          return absl::FailedPreconditionError(
              "Node class cannot be NODE_CLASS_UNSPECIFIED");
        }
        if (ServerClassFromNodeClass(
                static_cast<NodeClass>(node_class_select)) == *server_class) {
          node_class_matched = true;
          break;
        }
      }
    } else if (!select.server_class().empty() && server_class.has_value()) {
      node_class_matched = false;
      for (int server_class_select : select.server_class()) {
        if (server_class_select ==
            SelectionSpec::SelectionClass::SERVER_CLASS_UNSPECIFIED) {
          return absl::FailedPreconditionError(
              "Server class cannot be SERVER_CLASS_UNSPECIFIED");
        }
        if (server_class_select == server_class) {
          node_class_matched = true;
          break;
        }
      }
    }
    if (server_type_matched && server_tag_matched && node_class_matched) {
      ECCLESIA_RETURN_IF_ERROR(execute_fn());
    }
  }
  return absl::OkStatus();
}

absl::Status ExecuteOnMatchingQuerySelections(
    const QueryRouterSpec& router_spec, absl::string_view server_tag,
    SelectionSpec::SelectionClass::ServerType server_type,
    std::optional<SelectionSpec::SelectionClass::ServerClass> server_class,
    absl::AnyInvocable<absl::Status(absl::string_view,
                                    const SelectionSpec::QuerySelectionSpec&)>
        execute_fn) {
  for (const auto& [query_id, select_spec] : router_spec.selection_specs()) {
    for (const SelectionSpec::QuerySelectionSpec& query_select_spec :
         select_spec.query_selection_specs()) {
      if (query_select_spec.select().empty()) {
        return absl::FailedPreconditionError(absl::StrCat(
            "Both server tag and server type not specified for query: ",
            query_id));
      }
      ECCLESIA_RETURN_IF_ERROR(ExecuteOnMatchingSelections(
          query_select_spec.select(), server_tag, server_type, server_class,
          [&]() {
            ECCLESIA_RETURN_IF_ERROR(execute_fn(query_id, query_select_spec));
            return absl::OkStatus();
          }));
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<QuerySpec> GetQuerySpec(
    const QueryRouterSpec& router_spec, absl::string_view server_tag,
    SelectionSpec::SelectionClass::ServerType server_type,
    std::optional<SelectionSpec::SelectionClass::ServerClass> server_class,
    const Clock* clock) {
  QuerySpec query_spec;
  query_spec.clock = clock;
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
  ECCLESIA_RETURN_IF_ERROR(ExecuteOnMatchingQuerySelections(
      router_spec, server_tag, server_type, server_class, add_to_query_spec));
  return std::move(query_spec);
}

ecclesia::QueryRouterSpec::StableIdConfig::StableIdType
GetStableIdTypeFromRouterSpec(
    const ecclesia::QueryRouterSpec& router_spec,
    absl::string_view node_entity_tag,
    SelectionSpec::SelectionClass::ServerType server_type,
    SelectionSpec::SelectionClass::ServerClass server_class) {
  if (!router_spec.has_stable_id_config()) {
    return router_spec.default_stable_id_type();
  }
  // match inputs against the SelectionClasses specified in the devpath policies
  for (const ecclesia::QueryRouterSpec::StableIdConfig::Policy& policy :
       router_spec.stable_id_config().policies()) {
    // If agent is specified in the spec, it must match input being registered.
    if (policy.select().has_server_type()) {
      if (policy.select().server_type() != server_type) {
        continue;
      }
    }
    // Now ensure either the policy's server_class or node entity tag matches.
    if (absl::c_linear_search(policy.select().server_class(), server_class) ||
        absl::c_linear_search(policy.select().server_tag(), node_entity_tag)) {
      return policy.stable_id_type();
    }
  }
  // If nothing has matched, return the default.
  return router_spec.default_stable_id_type();
}

std::optional<std::string> GetTopologyConfigNameFromRouterSpec(
    const ecclesia::QueryRouterSpec& router_spec,
    absl::string_view node_entity_tag,
    SelectionSpec::SelectionClass::ServerType server_type,
    SelectionSpec::SelectionClass::ServerClass server_class) {
  if (!router_spec.has_stable_id_config()) {
    return std::nullopt;
  }
  for (const ecclesia::QueryRouterSpec::StableIdConfig::Policy& policy :
       router_spec.stable_id_config().policies()) {
    // If agent is specified in the spec, it must match input being registered.
    if (policy.select().has_server_type()) {
      if (policy.select().server_type() != server_type) {
        continue;
      }
    }
    // Now ensure either the policy's server_class or node entity tag matches.
    if (absl::c_linear_search(policy.select().server_class(), server_class) ||
        absl::c_linear_search(policy.select().server_tag(), node_entity_tag)) {
      return policy.topology_config_path();
    }
  }
  // If nothing has matched, return the default.
  return std::nullopt;
}

absl::flat_hash_map<std::string,
                    QueryRouterSpec::VersionConfig::Policy::BmcVersion>
GetQueryIdToBmcVersionFromRouterSpec(
    const ecclesia::QueryRouterSpec& router_spec,
    absl::string_view node_entity_tag,
    SelectionSpec::SelectionClass::ServerType server_type,
    SelectionSpec::SelectionClass::ServerClass server_class) {
  absl::flat_hash_map<std::string,
                      QueryRouterSpec::VersionConfig::Policy::BmcVersion>
      query_id_to_bmc_version;
  // match inputs against the SelectionClasses specified in the devpath policies
  for (const auto& [query_id, version_config] :
       router_spec.query_id_to_version_config()) {
    for (const ecclesia::QueryRouterSpec::VersionConfig::Policy& policy :
         version_config.policies()) {
      bool server_type_matched = true;
      if (policy.select().has_server_type()) {
        server_type_matched = policy.select().server_type() == server_type;
      }
      bool server_tag_matched =
          policy.select().server_tag().empty()
              ? true
              : std::find(policy.select().server_tag().begin(),
                          policy.select().server_tag().end(),
                          node_entity_tag) !=
                    policy.select().server_tag().end();
      bool node_class_matched = true;
      if (!policy.select().server_class().empty()) {
        node_class_matched = false;
        for (int server_class_select : policy.select().server_class()) {
          if (server_class_select == server_class) {
            node_class_matched = true;
            break;
          }
        }
      }
      if (server_type_matched && server_tag_matched && node_class_matched) {
        QueryRouterSpec::VersionConfig::Policy::BmcVersion bmc_version;

        if (policy.bmc_version().has_min_version()) {
          bmc_version.set_min_version(policy.bmc_version().min_version());
        }
        if (policy.bmc_version().has_max_version()) {
          bmc_version.set_max_version(policy.bmc_version().max_version());
        }
        query_id_to_bmc_version[query_id] = std::move(bmc_version);
      }
    }
  }
  // If nothing has matched, return the default.
  return query_id_to_bmc_version;
}

ecclesia::QueryEngineParams::RedfishStableIdType
RouterSpecStableIdToQueryEngineStableId(
    const ecclesia::QueryRouterSpec::StableIdConfig::StableIdType type) {
  if (type ==
      ecclesia::QueryRouterSpec::StableIdConfig::STABLE_ID_TOPOLOGY_DERIVED) {
    return ecclesia::QueryEngineParams::RedfishStableIdType::
        kRedfishLocationDerived;
  }
  return ecclesia::QueryEngineParams::RedfishStableIdType::kRedfishLocation;
}

}  // namespace ecclesia
