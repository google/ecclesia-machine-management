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

#include "ecclesia/lib/redfish/redpath/definitions/query_router/query_router.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ecclesia/lib/redfish/dellicius/engine/query_engine.h"
#include "ecclesia/lib/redfish/dellicius/query/query_variables.pb.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_engine/query_engine_features.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_engine/query_engine_features.pb.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_engine/query_spec.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_router/default_template_variable_names.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_router/query_router_spec.pb.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_router/util.h"
#include "ecclesia/lib/redfish/transport/cache.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/status/macros.h"

namespace ecclesia {

namespace {

// Makes new QueryVariableSet from query_arguments with SYSTEM_ID variable set.
QueryEngineIntf::QueryVariableSet CreateQueryArgumentsWithSystemId(
    const QueryEngineIntf::QueryVariableSet &query_arguments,
    const std::vector<absl::string_view> &query_ids,
    const std::string &node_local_system_id) {
  if (query_arguments.contains(kNodeLocalSystemIdVariableName)) {
    return query_arguments;
  }
  QueryEngineIntf::QueryVariableSet query_arguments_with_system_id =
      query_arguments;
  QueryVariables::VariableValue system_id_value;
  system_id_value.set_name(std::string(kNodeLocalSystemIdVariableName));
  system_id_value.add_values(node_local_system_id);
  for (absl::string_view query_id : query_ids) {
    *query_arguments_with_system_id[std::string(query_id)]
         .add_variable_values() = system_id_value;
  }
  return query_arguments_with_system_id;
}
}  // namespace

absl::StatusOr<std::unique_ptr<QueryRouterIntf>> QueryRouter::Create(
    const QueryRouterSpec &router_spec, std::vector<ServerSpec> server_specs,
    QueryEngineFactory query_engine_factory) {
  if (router_spec.query_pattern() != QueryPattern::PATTERN_SERIAL_ALL) {
    return absl::FailedPreconditionError(
        "QueryRouter only supports serial queries currently.");
  }

  RoutingTable routing_table;
  routing_table.reserve(server_specs.size());

  for (ServerSpec &server_spec : server_specs) {
    const ServerInfo &server_info = server_spec.server_info;

    QueryEngineParams query_engine_params = {
        .transport = std::move(server_spec.transport),
        .entity_tag = server_info.server_tag,
    };

    if (router_spec.has_features()) {
      query_engine_params.features = router_spec.features();
    } else {
      query_engine_params.features = DefaultQueryEngineFeatures();
    }

    if (router_spec.cache_duration_ms() > 0) {
      query_engine_params.cache_factory =
          [cache_duration =
               router_spec.cache_duration_ms()](RedfishTransport *transport) {
            return TimeBasedCache::Create(transport,
                                          absl::Milliseconds(cache_duration));
          };
    }

    ECCLESIA_ASSIGN_OR_RETURN(QuerySpec query_spec,
                              GetQuerySpec(router_spec, server_info.server_tag,
                                           server_info.server_type));
    absl::flat_hash_set<std::string> query_ids;
    query_ids.reserve(query_spec.query_id_to_info.size());
    for (const auto &[query_id, info] : query_spec.query_id_to_info) {
      query_ids.insert(query_id);
    }

    ECCLESIA_ASSIGN_OR_RETURN(
        auto query_engine,
        query_engine_factory(std::move(query_spec),
                             std::move(query_engine_params),
                             std::move(server_spec.id_assigner), nullptr));

    routing_table.push_back({
        .server_info = std::move(server_spec.server_info),
        .query_engine = std::move(query_engine),
        .query_ids = std::move(query_ids),
        .node_local_system_id = std::move(server_spec.node_local_system_id),
    });
  }

  return absl::WrapUnique(new QueryRouter(std::move(routing_table)));
}

void QueryRouter::ExecuteQuery(
    absl::Span<const absl::string_view> query_ids,
    const QueryEngineIntf::QueryVariableSet &query_arguments,
    const ResultCallback &callback) const {
  for (const QueryRoutingInfo &routing_info : routing_table_) {
    std::vector<absl::string_view> queries;
    queries.reserve(query_ids.size());
    for (absl::string_view query_id : query_ids) {
      if (routing_info.query_ids.contains(query_id)) {
        queries.push_back(query_id);
      }
    }
    if (queries.empty()) {
      continue;
    }
    QueryIdToResult result;
    if (routing_info.node_local_system_id.has_value()) {
      result = routing_info.query_engine->ExecuteRedpathQuery(
          queries, QueryEngine::ServiceRootType::kCustom,
          CreateQueryArgumentsWithSystemId(query_arguments, queries,
                                           *routing_info.node_local_system_id));
    } else {
      result = routing_info.query_engine->ExecuteRedpathQuery(
          queries, QueryEngine::ServiceRootType::kCustom, query_arguments);
    }
    for (auto &[query_id, query_result] : *result.mutable_results()) {
      callback(routing_info.server_info, std::move(query_result));
    }
  }
}

}  // namespace ecclesia
