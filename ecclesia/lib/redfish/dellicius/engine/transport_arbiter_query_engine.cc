/*
 * Copyright 2025 Google LLC
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

#include "ecclesia/lib/redfish/dellicius/engine/transport_arbiter_query_engine.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/interface.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/passkey.h"
#include "ecclesia/lib/redfish/dellicius/engine/query_engine.h"
#include "ecclesia/lib/redfish/dellicius/utils/id_assigner.h"
#include "ecclesia/lib/redfish/dellicius/utils/parsers.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_engine/query_spec.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/redfish/redpath/engine/normalizer.h"
#include "ecclesia/lib/redfish/redpath/engine/query_planner.h"
#include "ecclesia/lib/redfish/redpath/engine/query_planner_impl.h"
#include "ecclesia/lib/redfish/transport/http_redfish_intf.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/status/macros.h"
#include "ecclesia/lib/stubarbiter/arbiter.h"
#include "ecclesia/lib/time/proto.h"

namespace ecclesia {
namespace {
void ProcessMetrics(const StubArbiterInfo::Metrics &metrics,
                    const absl::flat_hash_map<StubArbiterInfo::PriorityLabel,
                                              ErrorCode> &transport_status,
                    QueryResult &result_single) {
  auto set_timestamp = [](const absl::Time source,
                          google::protobuf::Timestamp *target) {
    if (absl::StatusOr<google::protobuf::Timestamp> proto_time =
            ecclesia::AbslTimeToProtoTime(source);
        proto_time.ok()) {
      *target = std::move(*proto_time);
    }
  };

  auto set_status = [&transport_status](StubArbiterInfo::PriorityLabel label,
                                        ecclesia::TransportPriority priority,
                                        Status *status) {
    if (auto status_it = transport_status.find(label);
        status_it != transport_status.end()) {
      TransportErrorCode *transport_code = status->add_transport_code();
      transport_code->set_transport_priority(priority);
      transport_code->set_error_code(status_it->second);
    }
  };

  auto process_endpoint_metrics =
      [&set_timestamp, &set_status](
          StubArbiterInfo::PriorityLabel label,
          const StubArbiterInfo::EndpointMetrics &endpoint_metrics,
          TransportMetrics *query_result_metrics, Status *status) {
        ecclesia::TransportPriority priority;
        switch (label) {
          case StubArbiterInfo::PriorityLabel::kPrimary:
            priority = TransportPriority::TRANSPORT_PRIORITY_PRIMARY;
            break;
          case StubArbiterInfo::PriorityLabel::kSecondary:
            priority = TransportPriority::TRANSPORT_PRIORITY_SECONDARY;
            break;
          default:
            priority = TransportPriority::TRANSPORT_PRIORITY_UNKNOWN;
            break;
        }
        query_result_metrics->set_transport_priority(priority);
        set_timestamp(endpoint_metrics.start_time,
                      query_result_metrics->mutable_start_time());
        set_timestamp(endpoint_metrics.end_time,
                      query_result_metrics->mutable_end_time());
        set_status(label, priority, status);
      };

  if (auto primary_it = metrics.endpoint_metrics.find(
          StubArbiterInfo::PriorityLabel::kPrimary);
      primary_it != metrics.endpoint_metrics.end()) {
    process_endpoint_metrics(
        StubArbiterInfo::PriorityLabel::kPrimary, primary_it->second,
        result_single.mutable_stats()->add_transport_metrics(),
        result_single.mutable_status());
  }

  if (auto secondary_it = metrics.endpoint_metrics.find(
          StubArbiterInfo::PriorityLabel::kSecondary);
      secondary_it != metrics.endpoint_metrics.end()) {
    process_endpoint_metrics(
        StubArbiterInfo::PriorityLabel::kSecondary, secondary_it->second,
        result_single.mutable_stats()->add_transport_metrics(),
        result_single.mutable_status());
  }
}
}  // namespace

absl::StatusOr<std::unique_ptr<QueryEngineIntf>>
QueryEngineWithTransportArbiter::CreateTransportArbiterQueryEngine(
    ecclesia::QuerySpec query_spec, Params engine_params,
    std::unique_ptr<ecclesia::IdAssigner> id_assigner,
    ecclesia::RedpathNormalizer::QueryIdToNormalizerMap id_to_normalizers) {
  if (!engine_params.features.enable_streaming()) {
    return absl::InvalidArgumentError(
        "Streaming feature must be enabled for "
        "QueryEngineWithTransportArbiter.");
  }

  ECCLESIA_ASSIGN_OR_RETURN(
      std::unique_ptr<TransportArbiter> arbiter,
      TransportArbiter::Create(
          {.type = engine_params.transport_arbiter_type.value_or(
               StubArbiterInfo::Type::kFailover),
           .refresh = engine_params.transport_arbiter_refresh.value_or(
               absl::Seconds(5))},
          [transport_factory = std::move(engine_params.transport_factory),
           cache_factory = std::move(engine_params.cache_factory)](
              StubArbiterInfo::PriorityLabel label)
              -> absl::StatusOr<std::unique_ptr<RedfishInterface>> {
            ECCLESIA_ASSIGN_OR_RETURN(
                std::unique_ptr<RedfishTransport> transport,
                transport_factory(label));

            auto cache = cache_factory(transport.get());
            return NewHttpInterface(std::move(transport), std::move(cache),
                                    ecclesia::RedfishInterface::kTrusted);
          },
          query_spec.clock));

  std::unique_ptr<ecclesia::RedpathNormalizer> redpath_normalizer;
  arbiter->Execute([&](ecclesia::RedfishInterface *redfish_interface,
                       StubArbiterInfo::PriorityLabel label) -> absl::Status {
    if (id_assigner == nullptr) {
      redpath_normalizer = BuildLocalDevpathRedpathNormalizer(
          redfish_interface,
          QueryEngineParams::GetRedpathNormalizerStableIdType(
              engine_params.stable_id_type),
          engine_params.redfish_topology_config_name);
    } else {
      redpath_normalizer = GetMachineDevpathRedpathNormalizer(
          QueryEngineParams::GetRedpathNormalizerStableIdType(
              engine_params.stable_id_type),
          engine_params.redfish_topology_config_name, std::move(id_assigner),
          redfish_interface);
    }
    return absl::OkStatus();
  });

  if (redpath_normalizer == nullptr) {
    return absl::InternalError("Failed to create redpath normalizer.");
  }

  // Build RedPath trie based query planner.
  absl::flat_hash_map<std::string, std::unique_ptr<ecclesia::QueryPlannerIntf>>
      id_to_redpath_trie_plans;
  for (auto &[query_id, query_info] : query_spec.query_id_to_info) {
    std::vector<ecclesia::RedpathNormalizer *> additional_normalizers;
    if (auto it = id_to_normalizers.find(query_id);
        it != id_to_normalizers.end() && it->second != nullptr) {
      additional_normalizers.push_back(it->second.get());
    }

    ECCLESIA_ASSIGN_OR_RETURN(
        std::unique_ptr<ecclesia::QueryPlannerIntf> query_planner,
        ecclesia::BuildQueryPlanner(
            {.query = &query_info.query,
             .normalizer = redpath_normalizer.get(),
             .additional_normalizers = std::move(additional_normalizers),
             .redpath_rules =
                 ecclesia::CreateRedPathRules(std::move(query_info.rule)),
             .clock = query_spec.clock,
             .query_timeout = query_info.timeout,
             .execution_mode =
                 engine_params.features.fail_on_first_error()
                     ? ecclesia::QueryPlanner::ExecutionMode::kFailOnFirstError
                     : ecclesia::QueryPlanner::ExecutionMode::
                           kContinueOnSubqueryErrors}));
    id_to_redpath_trie_plans[query_id] = std::move(query_planner);
  }

  return absl::WrapUnique(new QueryEngineWithTransportArbiter(
      engine_params.entity_tag, std::move(id_to_redpath_trie_plans),
      query_spec.clock, std::move(redpath_normalizer), std::move(arbiter),
      std::move(engine_params.features), std::move(id_to_normalizers)));
}

absl::Status QueryEngineWithTransportArbiter::ExecuteOnRedfishInterface(
    ecclesia::RedfishInterfacePasskey unused_passkey,
    const RedfishInterfaceOptions &options) {
  StubArbiterInfo::PriorityLabel priority_label =
      StubArbiterInfo::PriorityLabel::kPrimary;
  if (options.priority_label != StubArbiterInfo::PriorityLabel::kUnknown) {
    priority_label = options.priority_label;
  }

  StubArbiterInfo::Metrics metrics = transport_arbiter_->Execute(
      [&callback = options.callback](
          ecclesia::RedfishInterface *redfish_interface,
          StubArbiterInfo::PriorityLabel label) -> absl::Status {
        return callback(*redfish_interface);
      },
      priority_label);
  return metrics.overall_status;
}

ecclesia::QueryIdToResult QueryEngineWithTransportArbiter::ExecuteRedpathQuery(
    absl::Span<const absl::string_view> query_ids,
    const RedpathQueryOptions &options) {
  ecclesia::QueryIdToResult query_id_to_result;
  for (const absl::string_view query_id : query_ids) {
    auto it = id_to_redpath_query_plans_.find(query_id);
    if (it == id_to_redpath_query_plans_.end()) {
      LOG(ERROR) << "Query plan does not exist for id " << query_id;
      continue;
    }
    ecclesia::QueryVariables vars = ecclesia::QueryVariables();
    auto it_vars = options.query_arguments.find(query_id);
    if (it_vars != options.query_arguments.end()) {
      vars = it_vars->second;
    }

    ecclesia::QueryPlanner::QueryExecutionResult result_single;
    ecclesia::ExecutionFlags planner_execution_flags{
        .execution_mode =
            features_.fail_on_first_error()
                ? ecclesia::ExecutionFlags::ExecutionMode::kFailOnFirstError
                : ecclesia::ExecutionFlags::ExecutionMode::
                      kContinueOnSubqueryErrors,
        .log_redfish_traces = features_.log_redfish_traces(),
        .enable_url_annotation = features_.enable_url_annotation()};
    {
      auto query_timer =
          ecclesia::RedpathQueryTimestamp(&result_single, clock_);

      StubArbiterInfo::PriorityLabel priority_label =
          StubArbiterInfo::PriorityLabel::kPrimary;
      if (options.priority_label != StubArbiterInfo::PriorityLabel::kUnknown) {
        priority_label = options.priority_label;
      }
      absl::flat_hash_map<StubArbiterInfo::PriorityLabel, ecclesia::ErrorCode>
          transport_status;
      StubArbiterInfo::Metrics metrics = transport_arbiter_->Execute(
          [&](ecclesia::RedfishInterface *redfish_interface,
              StubArbiterInfo::PriorityLabel label) -> absl::Status {
            ecclesia::QueryPlannerIntf::QueryExecutionResult local_result =
                it->second->Run(
                    {.variables = vars,
                     .enable_url_annotation =
                         planner_execution_flags.enable_url_annotation,
                     .log_redfish_traces =
                         planner_execution_flags.log_redfish_traces,
                     .custom_service_root =
                         options.service_root_uri ==
                                 ecclesia::QueryEngine::ServiceRootType::kGoogle
                             ? "/google/v1"
                             : "",
                     .redfish_interface = redfish_interface});

            result_single = std::move(local_result);
            transport_status[label] =
                result_single.query_result.status().error_code();
            return StatusFromQueryResultStatus(
                result_single.query_result.status());
          },
          priority_label);

      ProcessMetrics(metrics, transport_status, result_single.query_result);
    }
    query_id_to_result.mutable_results()->insert(
        {result_single.query_result.query_id(), result_single.query_result});
  }
  return query_id_to_result;
}

}  // namespace ecclesia
