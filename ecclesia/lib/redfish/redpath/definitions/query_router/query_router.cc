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

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/functional/bind_front.h"
#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/passkey.h"
#include "ecclesia/lib/redfish/dellicius/engine/query_engine.h"
#include "ecclesia/lib/redfish/dellicius/engine/transport_arbiter_query_engine.h"
#include "ecclesia/lib/redfish/dellicius/query/query_variables.pb.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_engine/query_engine_features.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_engine/query_engine_features.pb.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_engine/query_spec.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_router/default_template_variable_names.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_router/query_router_spec.pb.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_router/util.h"
#include "ecclesia/lib/redfish/redpath/engine/normalizer.h"
#include "ecclesia/lib/redfish/transport/cache.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/status/macros.h"
#include "ecclesia/lib/thread/thread_pool.h"

namespace ecclesia {

namespace {

// Makes new QueryVariableSet from query_arguments with SYSTEM_ID variable set.
QueryEngineIntf::QueryVariableSet CreateQueryArgumentsWithSystemId(
    absl::Span<const absl::string_view> query_ids,
    const QueryEngineIntf::QueryVariableSet &query_arguments,
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

void PopulateQueryWithCancelledErrorStatus(
    absl::Span<const absl::string_view> queries,
    const QueryRouterIntf::ServerInfo &server_info,
    const QueryRouterIntf::ResultCallback &callback,
    absl::Mutex &callback_mutex) {
  for (const absl::string_view &query_id : queries) {
    QueryResult result;
    result.set_query_id(std::string(query_id));
    result.mutable_status()->add_errors("Query execution has been cancelled.");
    result.mutable_status()->set_error_code(
        ecclesia::ErrorCode::ERROR_CANCELLED);
    absl::MutexLock lock(&callback_mutex);
    callback(server_info, std::move(result));
  }
}

void ExecuteQueries(QueryEngineIntf &query_engine,
                    absl::Span<const absl::string_view> queries,
                    const QueryRouterIntf::RedpathQueryOptions &options,
                    const QueryRouterIntf::ServerInfo &server_info,
                    const std::optional<std::string> &node_local_system_id,
                    absl::Mutex &callback_mutex, bool is_query_cancelled) {
  if (is_query_cancelled) {
    return PopulateQueryWithCancelledErrorStatus(
        queries, server_info, options.callback, callback_mutex);
  }
  QueryIdToResult result;
  QueryEngineIntf::RedpathQueryOptions redpath_query_options{
      .service_root_uri = QueryEngine::ServiceRootType::kCustom,
      .priority_label = options.priority_label};
  if (node_local_system_id.has_value()) {
    redpath_query_options.query_arguments = CreateQueryArgumentsWithSystemId(
        queries, options.query_arguments, *node_local_system_id);
    result = query_engine.ExecuteRedpathQuery(queries, redpath_query_options);
  } else {
    redpath_query_options.query_arguments = options.query_arguments;
    result = query_engine.ExecuteRedpathQuery(queries, redpath_query_options);
  }
  for (auto &[query_id, query_result] : *result.mutable_results()) {
    absl::MutexLock lock(&callback_mutex);
    options.callback(server_info, std::move(query_result));
  }
}

}  // namespace

absl::StatusOr<std::unique_ptr<QueryRouterIntf>> QueryRouter::Create(
    const QueryRouterSpec &router_spec, std::vector<ServerSpec> server_specs,
    QueryEngineFactory query_engine_factory,
    RedpathNormalizer::RedpathNormalizersFactory redpath_normalizers_factory,
    TransportArbiterQueryEngineFactory transport_arbiter_query_engine_factory) {
  switch (router_spec.query_pattern()) {
    case QueryPattern::PATTERN_SERIAL_ALL:
      break;
    case QueryPattern::PATTERN_SERIAL_AGENT:
    case QueryPattern::PATTERN_PARALLEL_ALL:
      if (router_spec.max_concurrent_threads() <= 0) {
        return absl::FailedPreconditionError(
            "QueryRouter requires a positive non-zero value for "
            "max_concurrent_threads when using patterns other than "
            "PATTERN_SERIAL_ALL.");
      }
      break;
    default:
      return absl::FailedPreconditionError(
          "Invalid query router pattern specified.");
  }

  RoutingTable routing_table;
  routing_table.reserve(server_specs.size());

  for (ServerSpec &server_spec : server_specs) {
    const ServerInfo &server_info = server_spec.server_info;

    QueryEngineParams query_engine_params = {
        .transport = std::move(server_spec.transport),
        .entity_tag = server_info.server_tag,
    };

    if (!server_spec.parsed_stable_id_type_from_spec) {
      ecclesia::QueryRouterSpec::StableIdConfig::StableIdType stable_id_type =
          GetStableIdTypeFromRouterSpec(router_spec, server_info.server_tag,
                                        server_info.server_type,
                                        server_info.server_class);
      // Set devpath policy for the query engine based on QueryRouterSpec.
      query_engine_params.stable_id_type =
          RouterSpecStableIdToQueryEngineStableId(stable_id_type);
    } else {
      query_engine_params.stable_id_type = server_spec.stable_id_type;
    }

    if (router_spec.has_features()) {
      query_engine_params.features = router_spec.features();
    } else {
      query_engine_params.features = StandardQueryEngineFeatures();
    }

    if (router_spec.cache_duration_ms() > 0) {
      query_engine_params.cache_factory =
          [cache_duration =
               router_spec.cache_duration_ms()](RedfishTransport *transport) {
            return TimeBasedCache::Create(transport,
                                          absl::Milliseconds(cache_duration));
          };
    }

    ECCLESIA_ASSIGN_OR_RETURN(
        QuerySpec query_spec,
        GetQuerySpec(router_spec, server_info.server_tag,
                     server_info.server_type, server_info.server_class));
    absl::flat_hash_set<std::string> query_ids;
    query_ids.reserve(query_spec.query_id_to_info.size());
    for (const auto &[query_id, info] : query_spec.query_id_to_info) {
      query_ids.insert(query_id);
    }

    std::unique_ptr<ecclesia::QueryEngineIntf> query_engine;
    if (server_spec.transport == nullptr &&
        server_spec.transport_factory.has_value() &&
        server_spec.transport_arbiter_type.has_value()) {
      QueryEngineWithTransportArbiter::Params
          transport_arbiter_query_engine_params = {
              .cache_factory = std::move(query_engine_params.cache_factory),
              .entity_tag = query_engine_params.entity_tag,
              .stable_id_type = query_engine_params.stable_id_type,
              .features = std::move(query_engine_params.features),
              .redfish_topology_config_name =
                  query_engine_params.redfish_topology_config_name,
              .transport_factory = std::move(*server_spec.transport_factory),
              .transport_arbiter_type = *server_spec.transport_arbiter_type,
          };

      if (router_spec.has_transport_arbiter_refresh()) {
        transport_arbiter_query_engine_params.transport_arbiter_refresh =
            absl::Seconds(router_spec.transport_arbiter_refresh());
      }
      ECCLESIA_ASSIGN_OR_RETURN(
          query_engine, transport_arbiter_query_engine_factory(
                            std::move(query_spec),
                            std::move(transport_arbiter_query_engine_params),
                            std::move(server_spec.id_assigner),
                            redpath_normalizers_factory()));
    } else {
      ECCLESIA_ASSIGN_OR_RETURN(
          query_engine, query_engine_factory(std::move(query_spec),
                                             std::move(query_engine_params),
                                             std::move(server_spec.id_assigner),
                                             redpath_normalizers_factory()));
    }

    routing_table.push_back({
        .server_info = std::move(server_spec.server_info),
        .query_engine = std::move(query_engine),
        .query_ids = std::move(query_ids),
        .node_local_system_id = std::move(server_spec.node_local_system_id),
    });
  }

  return absl::WrapUnique(
      new QueryRouter(std::move(routing_table), router_spec.query_pattern(),
                      router_spec.max_concurrent_threads()));
}

QueryRouter::QueryRouter(QueryRouter::RoutingTable routing_table,
                         QueryPattern query_pattern, int max_concurrent_threads)
    : routing_table_(std::move(routing_table)),
      max_concurrent_threads_(max_concurrent_threads) {
  switch (query_pattern) {
    case QueryPattern::PATTERN_SERIAL_ALL:
      execute_function_ =
          absl::bind_front(&QueryRouter::ExecuteQuerySerialAll, this);
      break;
    case QueryPattern::PATTERN_SERIAL_AGENT:
      execute_function_ =
          absl::bind_front(&QueryRouter::ExecuteQuerySerialAgent, this);
      break;
    case QueryPattern::PATTERN_PARALLEL_ALL:
      execute_function_ =
          absl::bind_front(&QueryRouter::ExecuteQueryParallelAll, this);
      break;
    default:
      // This should never happen
      LOG(FATAL) << "Unknown query pattern: " << query_pattern;
  }
}

void QueryRouter::ExecuteQuery(const RedpathQueryOptions &options) const {
  {
    absl::MutexLock lock(&execute_ref_count_mutex_);
    ++execute_ref_count_;
  }
  execute_function_(options);
  absl::MutexLock lock(&execute_ref_count_mutex_);
  --execute_ref_count_;

  // If query cancellation is initiated and all query executions are
  // completed/terminated, notify waiting
  // cancellation thread to reset query cancellation state indicating that query
  // cancellation is completed.
  // If no query cancellation is initiated, the cv.Signal() call is a no-op.
  if (execute_ref_count_ == 0) {
    cancel_completion_cond_.Signal();
  }
}

void QueryRouter::ExecuteQuerySerialAll(
    const RedpathQueryOptions &options) const {
  absl::Mutex callback_mutex;
  bool is_query_cancelled = IsQueryExecutionCancelled();
  for (const QueryRoutingInfo &routing_info : routing_table_) {
    std::vector<absl::string_view> queries;
    queries.reserve(options.query_ids.size());
    for (absl::string_view query_id : options.query_ids) {
      if (routing_info.query_ids.contains(query_id)) {
        queries.push_back(query_id);
      }
    }
    if (!queries.empty()) {
      ExecuteQueries(*routing_info.query_engine, queries, options,
                     routing_info.server_info,
                     routing_info.node_local_system_id, callback_mutex,
                     is_query_cancelled);
    }

    // Caches the query cancellation state if cancellation has been initiated.
    // This is to avoid multiple calls to IsQueryExecutionCancelled() which
    // acquires the mutex lock.
    if (!is_query_cancelled) {
      is_query_cancelled = IsQueryExecutionCancelled();
    }
  }
}

void QueryRouter::ExecuteQuerySerialAgent(
    const RedpathQueryOptions &options) const {
  std::vector<QueryBatch> query_batches;
  query_batches.reserve(routing_table_.size());
  for (const QueryRoutingInfo &routing_info : routing_table_) {
    // Combine all queries per agent into a single batch
    QueryBatch query_batch(&routing_info);
    for (absl::string_view query_id : options.query_ids) {
      if (routing_info.query_ids.contains(query_id)) {
        query_batch.queries.push_back(query_id);
      }
    }
    if (!query_batch.queries.empty()) {
      query_batches.push_back(std::move(query_batch));
    }
  }

  ExecuteQueryBatches(query_batches, options);
}

void QueryRouter::ExecuteQueryParallelAll(
    const RedpathQueryOptions &options) const {
  std::vector<QueryBatch> query_batches;
  // Group individual queries into their own batches
  for (const QueryRoutingInfo &routing_info : routing_table_) {
    for (absl::string_view query_id : options.query_ids) {
      if (routing_info.query_ids.contains(query_id)) {
        QueryBatch query_batch(&routing_info);
        query_batch.queries.push_back(query_id);
        query_batches.push_back(std::move(query_batch));
      }
    }
  }
  ExecuteQueryBatches(query_batches, options);
}

void QueryRouter::ExecuteQueryBatches(
    absl::Span<const QueryBatch> query_batches,
    const RedpathQueryOptions &options) const {
  int num_threads =
      std::min(static_cast<int>(query_batches.size()), max_concurrent_threads_);

  absl::Mutex callback_mutex;
  ThreadPool thread_pool(num_threads);
  bool is_query_cancelled = IsQueryExecutionCancelled();
  for (const QueryBatch &query_batch : query_batches) {
    thread_pool.Schedule([&, is_query_cancelled]() {
      const QueryRoutingInfo &routing_info = query_batch.routing_info;
      ExecuteQueries(*routing_info.query_engine, query_batch.queries, options,
                     routing_info.server_info,
                     routing_info.node_local_system_id, callback_mutex,
                     is_query_cancelled);
    });

    // Caches the query cancellation state if cancellation has been initiated.
    // This is to avoid multiple calls to IsQueryExecutionCancelled() which
    // acquires the mutex lock.
    if (!is_query_cancelled) {
      is_query_cancelled = IsQueryExecutionCancelled();
    }
  }
}

absl::StatusOr<RedfishInterface *> QueryRouter::GetRedfishInterface(
    const ServerInfo &server_info,
    RedfishInterfacePasskey unused_passkey) const {
  for (const QueryRoutingInfo &routing_info : routing_table_) {
    if (routing_info.server_info == server_info) {
      return routing_info.query_engine->GetRedfishInterface(unused_passkey);
    }
  }
  return absl::NotFoundError(absl::StrFormat(
      "RedfishInterface not found for server: %s server type: %s server class: "
      "%s",
      server_info.server_tag,
      SelectionSpec::SelectionClass::ServerType_Name(server_info.server_type),
      SelectionSpec::SelectionClass::ServerClass_Name(
          server_info.server_class)));
}

absl::Status QueryRouter::ExecuteOnRedfishInterface(
    const ServerInfo &server_info, RedfishInterfacePasskey unused_passkey,
    const QueryEngineIntf::RedfishInterfaceOptions &options) const {
  for (const QueryRoutingInfo &routing_info : routing_table_) {
    if (routing_info.server_info == server_info) {
      return routing_info.query_engine->ExecuteOnRedfishInterface(
          unused_passkey, options);
    }
  }
  return absl::NotFoundError(absl::StrFormat(
      "Server: %s server type: %s server class: "
      "%s was not found in the QueryRouter",
      server_info.server_tag,
      SelectionSpec::SelectionClass::ServerType_Name(server_info.server_type),
      SelectionSpec::SelectionClass::ServerClass_Name(
          server_info.server_class)));
}

void QueryRouter::CancelQueryExecution(absl::Notification *notification) {
  // If there are no active query executions, return early.
  if (GetExecuteRefCount() == 0) {
    return;
  }

  absl::MutexLock lock(&query_cancellation_state_mutex_);
  // If query execution is already cancelled, return early.
  if (query_cancellation_state_) {
    return;
  }

  query_cancellation_state_ = true;

  int num_threads = static_cast<int>(routing_table_.size());
  ThreadPool thread_pool(num_threads);
  for (const QueryRoutingInfo &routing_info : routing_table_) {
    thread_pool.Schedule([&]() {
      routing_info.query_engine->CancelQueryExecution(notification);
    });
  }

  // The Wait() call atomically unlocks "query_cancellation_state_mutex_"
  // (which the cancel thread must hold), and blocks on the condition variable
  // "cancel_completion_cond_". When "Execute" thread signals the condition
  // variable, the thread will reacquire the mutex.
  cancel_completion_cond_.Wait(&query_cancellation_state_mutex_);
  query_cancellation_state_ = false;
}

}  // namespace ecclesia
