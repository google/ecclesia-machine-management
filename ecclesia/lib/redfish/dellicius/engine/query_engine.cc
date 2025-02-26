/*
 * Copyright 2022 Google LLC
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

#include "ecclesia/lib/redfish/dellicius/engine/query_engine.h"

#include <sys/types.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/functional/function_ref.h"
#include "absl/log/check.h"
#include "absl/log/die_if_null.h"
#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ecclesia/lib/redfish/dellicius/engine/factory.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/interface.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/passkey.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/query_planner.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_errors.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_variables.pb.h"
#include "ecclesia/lib/redfish/dellicius/utils/id_assigner.h"
#include "ecclesia/lib/redfish/dellicius/utils/parsers.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_engine/query_spec.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_engine/redpath_subscription.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/converter.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/redfish/redpath/engine/normalizer.h"
#include "ecclesia/lib/redfish/redpath/engine/query_planner.h"
#include "ecclesia/lib/redfish/redpath/engine/query_planner_impl.h"
#include "ecclesia/lib/redfish/topology.h"
#include "ecclesia/lib/redfish/transport/http_redfish_intf.h"
#include "ecclesia/lib/redfish/transport/metrical_transport.h"
#include "ecclesia/lib/redfish/transport/transport_metrics.pb.h"
#include "ecclesia/lib/status/macros.h"
#include "ecclesia/lib/time/clock.h"
#include "ecclesia/lib/time/proto.h"

namespace ecclesia {

namespace {

using QueryExecutionResult = QueryPlannerIntf::QueryExecutionResult;

// RAII style wrapper to timestamp query.
ABSL_DEPRECATED("Use RedpathQueryTimestamp instead.")
class QueryTimestamp {
 public:
  QueryTimestamp(DelliciusQueryResult *result, const Clock *clock)
      : result_(*ABSL_DIE_IF_NULL(result)),
        clock_(*ABSL_DIE_IF_NULL(clock)),
        start_time_(clock_.Now()) {}

  ~QueryTimestamp() {
    auto set_time = [](absl::Time time, google::protobuf::Timestamp &field) {
      if (auto timestamp = AbslTimeToProtoTime(time); timestamp.ok()) {
        field = *std::move(timestamp);
      }
    };
    set_time(start_time_, *result_.mutable_start_timestamp());
    set_time(clock_.Now(), *result_.mutable_end_timestamp());
  }

 private:
  DelliciusQueryResult &result_;
  const Clock &clock_;
  absl::Time start_time_;
};

// Translates vector of  DelliciusQueryResult to new QueryResult format.
QueryIdToResult TranslateLegacyResults(
    const std::vector<DelliciusQueryResult> &legacy_results) {
  QueryIdToResult translated_results;
  std::for_each(legacy_results.begin(), legacy_results.end(),
                [&](const DelliciusQueryResult &result) {
                  translated_results.mutable_results()->insert(
                      {result.query_id(), ToQueryResult(result)});
                });
  return translated_results;
}

}  // namespace

// Main method for ExecuteQuery that triggers the QueryPlanner to execute
// queries and provide transport metrics as part of the Statistics in each
// QueryResult.
std::vector<DelliciusQueryResult> QueryEngine::ExecuteQueryLegacy(
    absl::Span<const absl::string_view> query_ids,
    QueryEngine::ServiceRootType service_root_uri,
    const QueryVariableSet &query_arguments) {
  std::vector<DelliciusQueryResult> response_entries;
  const RedfishMetrics *metrics = nullptr;
  // Each metrical_transport object has a thread local RedfishMetrics object.
  if (metrical_transport_ != nullptr) {
    metrics = MetricalRedfishTransport::GetConstMetrics();
  }
  for (const absl::string_view query_id : query_ids) {
    auto it = id_to_query_plans_.find(query_id);
    if (it == id_to_query_plans_.end()) {
      LOG(ERROR) << "Query plan does not exist for id " << query_id;
      continue;
    }
    QueryVariables vars = QueryVariables();
    auto it_vars = query_arguments.find(query_id);
    if (it_vars != query_arguments.end()) vars = query_arguments.at(query_id);
    // Clear metrics every query.
    if (metrical_transport_ != nullptr) {
      MetricalRedfishTransport::ResetMetrics();
    }
    DelliciusQueryResult result_single;
    ExecutionFlags planner_execution_flags{
        .execution_mode =
            features_.fail_on_first_error()
                ? ExecutionFlags::ExecutionMode::kFailOnFirstError
                : ExecutionFlags::ExecutionMode::kContinueOnSubqueryErrors,
        .log_redfish_traces = features_.log_redfish_traces(),
        .enable_url_annotation = features_.enable_url_annotation()};
    {
      auto query_timer = QueryTimestamp(&result_single, clock_);
      if (service_root_uri == QueryEngine::ServiceRootType::kCustom) {
        result_single = it->second->Run(*clock_, nullptr, vars, metrics,
                                        planner_execution_flags);
      } else {
        result_single = it->second->Run(
            redfish_interface_->GetRoot(
                GetParams{},
                service_root_uri == QueryEngine::ServiceRootType::kGoogle
                    ? ServiceRootUri::kGoogle
                    : ServiceRootUri::kRedfish),
            *clock_, nullptr, vars, metrics, planner_execution_flags);
      }
    }
    response_entries.push_back(std::move(result_single));
  }
  return response_entries;
}

// Executes Redpath query and returns results in updated QueryResult format.
QueryIdToResult QueryEngine::ExecuteRedpathQuery(
    absl::Span<const absl::string_view> query_ids,
    const RedpathQueryOptions &options) {
  if (!features_.enable_streaming()) {
    return TranslateLegacyResults(ExecuteQueryLegacy(
        query_ids, options.service_root_uri, options.query_arguments));
  }

  QueryIdToResult query_id_to_result;
  for (const absl::string_view query_id : query_ids) {
    auto it = id_to_redpath_query_plans_.find(query_id);
    if (it == id_to_redpath_query_plans_.end()) {
      LOG(ERROR) << "Query plan does not exist for id " << query_id;
      continue;
    }
    QueryVariables vars = QueryVariables();
    auto it_vars = options.query_arguments.find(query_id);
    if (it_vars != options.query_arguments.end())
      vars = options.query_arguments.at(query_id);

    QueryExecutionResult result_single;
    ExecutionFlags planner_execution_flags{
        .execution_mode =
            features_.fail_on_first_error()
                ? ExecutionFlags::ExecutionMode::kFailOnFirstError
                : ExecutionFlags::ExecutionMode::kContinueOnSubqueryErrors,
        .log_redfish_traces = features_.log_redfish_traces(),
        .enable_url_annotation = features_.enable_url_annotation()};
    {
      auto query_timer = RedpathQueryTimestamp(&result_single, clock_);
      result_single = it->second->Run(
          {.variables = vars,
           .enable_url_annotation =
               planner_execution_flags.enable_url_annotation,
           .log_redfish_traces = planner_execution_flags.log_redfish_traces,
           .custom_service_root =
               options.service_root_uri == QueryEngine::ServiceRootType::kGoogle
                   ? "/google/v1"
                   : "",
           .redfish_interface = redfish_interface_.get()});
    }
    query_id_to_result.mutable_results()->insert(
        {result_single.query_result.query_id(), result_single.query_result});
  }
  return query_id_to_result;
}

void QueryEngine::HandleRedfishEvent(
    const RedfishVariant &variant,
    const RedPathSubscription::EventContext &event_context,
    absl::FunctionRef<
        void(const QueryResult &result,
             const RedPathSubscription::EventContext &event_context)>
        on_event_callback) {
  auto find_context = id_to_subscription_context_.find(event_context.query_id);
  if (find_context != id_to_subscription_context_.end()) {
    auto find_trie_node =
        find_context->second->redpath_to_trie_node.find(event_context.redpath);
    if (find_trie_node == find_context->second->redpath_to_trie_node.end()) {
      LOG(ERROR) << "Cannot resume query. RedpathTrieNode not found for "
                 << event_context.query_id;
      return;
    }

    // Get query plan to resume query operation with the received Redfish event.
    const std::unique_ptr<QueryPlannerIntf> &query_plan =
        id_to_redpath_query_plans_.at(event_context.query_id);

    QueryResult resume_query_result = query_plan->Resume(
        {.trie_node = find_trie_node->second,
         .redfish_variant = variant,
         .variables = std::move(find_context->second->query_variables),
         .redfish_interface = redfish_interface_.get()});

    if (resume_query_result.has_status()) {
      std::string error_message = resume_query_result.status().errors().empty()
                                      ? ""
                                      : resume_query_result.status().errors(0);
      LOG(ERROR) << "Cannot resume query. Error: " << error_message;
      return;
    }
    on_event_callback(resume_query_result, event_context);
  }
}

absl::StatusOr<SubscriptionQueryResult> QueryEngine::ExecuteSubscriptionQuery(
    absl::Span<const absl::string_view> query_ids,
    const QueryVariableSet &query_arguments,
    StreamingOptions streaming_options) {
  QueryIdToResult query_id_to_result;
  std::vector<RedPathSubscription::Configuration> subscription_configs;
  for (const absl::string_view query_id : query_ids) {
    auto it = id_to_redpath_query_plans_.find(query_id);
    if (it == id_to_redpath_query_plans_.end()) {
      return absl::InternalError(
          absl::StrCat("Query plan does not exist for id ", query_id));
    }
    QueryVariables vars = QueryVariables();
    auto it_vars = query_arguments.find(query_id);
    if (it_vars != query_arguments.end()) vars = query_arguments.at(query_id);
    QueryPlannerIntf::QueryExecutionResult result_single;
    {
      auto query_timer = RedpathQueryTimestamp(&result_single, clock_);
      result_single =
          it->second->Run({.variables = vars,
                           .query_type = QueryType::kSubscription,
                           .redfish_interface = redfish_interface_.get()});
      if (result_single.query_result.has_status()) {
        std::string error_message =
            result_single.query_result.status().errors().empty()
                ? ""
                : result_single.query_result.status().errors(0);
        return absl::InternalError(
            absl::StrCat("Query execution failed for id ", query_id,
                         ", message: ", error_message));
      }
    }

    query_id_to_result.mutable_results()->insert(
        {result_single.query_result.query_id(), result_single.query_result});
    if (result_single.subscription_context) {
      subscription_configs.insert(
          subscription_configs.end(),
          result_single.subscription_context->subscription_configs.begin(),
          result_single.subscription_context->subscription_configs.end());
      id_to_subscription_context_[query_id] =
          std::move(result_single.subscription_context);
    }
  }

  if (subscription_configs.empty()) {
    return absl::InternalError("No subscription configs found.");
  }

  // Create redfish event subscription.
  // Here we register callbacks with SubscriptionBroker for on_event and on_stop
  // event handling. SubscriptionBroker on successful stream creation will
  // return a `RedPathSubscription` object.
  ECCLESIA_ASSIGN_OR_RETURN(
      auto subscription,
      streaming_options.subscription_broker(
          subscription_configs, *redfish_interface_.get(),
          [on_event_callback = std::move(streaming_options.on_event_callback),
           this](const RedfishVariant &variant,
                 const RedPathSubscription::EventContext &event_context) {
            HandleRedfishEvent(variant, event_context, on_event_callback);
          },
          [on_stop_callback(std::move(streaming_options.on_stop_callback))](
              const absl::Status &status) { on_stop_callback(status); }));

  // Populate subscription result.
  SubscriptionQueryResult subscription_result;
  subscription_result.subscription = std::move(subscription);
  subscription_result.result = std::move(query_id_to_result);
  return subscription_result;
}

absl::StatusOr<RedfishInterface *> QueryEngine::GetRedfishInterface(
    RedfishInterfacePasskey unused_passkey) {
  if (redfish_interface_ == nullptr) {
    return absl::InternalError(
        "QueryEngine contains uninitialized RedfishInterface");
  }
  return redfish_interface_.get();
}

absl::Status QueryEngine::ExecuteOnRedfishInterface(
    RedfishInterfacePasskey unused_passkey,
    const RedfishInterfaceOptions &options) {
  if (redfish_interface_ == nullptr) {
    return absl::InternalError(
        "QueryEngine contains uninitialized RedfishInterface");
  }
  return options.callback(*redfish_interface_);
}

absl::StatusOr<std::unique_ptr<QueryEngineIntf>> QueryEngine::Create(
    QuerySpec query_spec, QueryEngineParams engine_params,
    std::unique_ptr<IdAssigner> id_assigner,
    RedpathNormalizer::QueryIdToNormalizerMap id_to_normalizers) {
  std::unique_ptr<RedfishInterface> redfish_interface;
  MetricalRedfishTransport *metrical_transport_ptr = nullptr;
  if (engine_params.features.enable_redfish_metrics()) {
    auto metrical_transport = std::make_unique<MetricalRedfishTransport>(
        std::move(engine_params.transport), ecclesia::Clock::RealClock());
    metrical_transport_ptr = metrical_transport.get();
    redfish_interface = NewHttpInterface(std::move(metrical_transport),
                                         std::move(engine_params.cache_factory),
                                         RedfishInterface::kTrusted);
  } else {
    redfish_interface = NewHttpInterface(std::move(engine_params.transport),
                                         std::move(engine_params.cache_factory),
                                         RedfishInterface::kTrusted);
  }

  if (redfish_interface == nullptr) {
    return absl::InternalError("Can't create redfish interface");
  }

  std::unique_ptr<Normalizer> legacy_normalizer;
  std::unique_ptr<RedpathNormalizer> redpath_normalizer;

  if (id_assigner == nullptr) {
    redpath_normalizer = BuildLocalDevpathRedpathNormalizer(
        redfish_interface.get(),
        QueryEngineParams::GetRedpathNormalizerStableIdType(
            engine_params.stable_id_type),
        engine_params.redfish_topology_config_name);
  } else {
    redpath_normalizer = GetMachineDevpathRedpathNormalizer(
        QueryEngineParams::GetRedpathNormalizerStableIdType(
            engine_params.stable_id_type),
        engine_params.redfish_topology_config_name, std::move(id_assigner),
        redfish_interface.get());
  }

  // Build RedPath trie based query planner.
  absl::flat_hash_map<std::string, std::unique_ptr<QueryPlannerIntf>>
      id_to_redpath_trie_plans;
  for (auto &[query_id, query_info] : query_spec.query_id_to_info) {
    std::vector<RedpathNormalizer *> additional_normalizers;
    if (auto it = id_to_normalizers.find(query_id);
        it != id_to_normalizers.end() && it->second != nullptr) {
      additional_normalizers.push_back(it->second.get());
    }

    ECCLESIA_ASSIGN_OR_RETURN(
        auto query_planner,
        BuildQueryPlanner(
            {.query = &query_info.query,
             .normalizer = redpath_normalizer.get(),
             .additional_normalizers = std::move(additional_normalizers),
             .redpath_rules = CreateRedPathRules(std::move(query_info.rule)),
             .clock = query_spec.clock,
             .query_timeout = query_info.timeout,
             .execution_mode =
                 engine_params.features.fail_on_first_error()
                     ? QueryPlanner::ExecutionMode::kFailOnFirstError
                     : QueryPlanner::ExecutionMode::
                           kContinueOnSubqueryErrors}));
    id_to_redpath_trie_plans[query_id] = std::move(query_planner);
  }

  return absl::WrapUnique(new QueryEngine(
      engine_params.entity_tag, std::move(id_to_redpath_trie_plans),
      query_spec.clock, std::move(legacy_normalizer),
      std::move(redpath_normalizer), std::move(redfish_interface),
      std::move(engine_params.features), metrical_transport_ptr,
      std::move(id_to_normalizers)));
}

}  // namespace ecclesia
