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

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/functional/function_ref.h"
#include "absl/log/check.h"
#include "absl/log/die_if_null.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ecclesia/lib/file/cc_embed_interface.h"
#include "ecclesia/lib/redfish/dellicius/engine/config.h"
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
#include "ecclesia/lib/redfish/node_topology.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/converter.h"
#include "ecclesia/lib/redfish/topology.h"
#include "ecclesia/lib/redfish/transport/http_redfish_intf.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/redfish/transport/metrical_transport.h"
#include "ecclesia/lib/redfish/transport/transport_metrics.pb.h"
#include "ecclesia/lib/time/clock.h"
#include "ecclesia/lib/time/proto.h"
#include "google/protobuf/text_format.h"

namespace ecclesia {

namespace {

std::unique_ptr<Normalizer> GetMachineDevpathNormalizer(
    QueryEngineParams::RedfishStableIdType stable_id_type,
    std::unique_ptr<IdAssigner> id_assigner,
    RedfishInterface *redfish_interface) {
  switch (stable_id_type) {
    case QueryEngineParams::RedfishStableIdType::kRedfishLocation:
      return BuildNormalizerWithMachineDevpath(std::move(id_assigner));
    case QueryEngineParams::RedfishStableIdType::kRedfishLocationDerived:
      return BuildNormalizerWithMachineDevpath(
          std::move(id_assigner), CreateTopologyFromRedfish(redfish_interface));
  }
}

// RAII style wrapper to timestamp query.
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

class QueryEngineImpl final : public QueryEngine::QueryEngineIntf {
 public:
  explicit QueryEngineImpl(const QueryEngineConfiguration &config,
                           std::unique_ptr<RedfishTransport> transport,
                           RedfishTransportCacheFactory cache_factory,
                           const Clock *clock)
      : clock_(clock) {
    if (config.flags.enable_transport_metrics) {
      std::unique_ptr<MetricalRedfishTransport> metrical_transport =
          std::make_unique<MetricalRedfishTransport>(std::move(transport),
                                                     clock_, nullptr);
      metrical_transport_ = metrical_transport.get();
      redfish_interface_ = NewHttpInterface(std::move(metrical_transport),
                                            std::move(cache_factory),
                                            RedfishInterface::kTrusted);
    } else {
      redfish_interface_ =
          NewHttpInterface(std::move(transport), std::move(cache_factory),
                           RedfishInterface::kTrusted);
    }

    if (config.flags.enable_devpath_extension) {
      normalizer_ = BuildDefaultNormalizerWithLocalDevpath(
          CreateTopologyFromRedfish(redfish_interface_.get()));
    } else {
      normalizer_ = BuildDefaultNormalizer();
    }

    // Parse query rules from embedded proto messages
    absl::flat_hash_map<std::string, RedPathRedfishQueryParams>
        query_id_to_rules =
            ParseQueryRulesFromEmbeddedFiles(config.query_rules);
    // Parse queries from embedded proto messages
    for (const EmbeddedFile &query_file : config.query_files) {
      DelliciusQuery query;
      if (!google::protobuf::TextFormat::ParseFromString(std::string(query_file.data),
                                               &query)) {
        LOG(ERROR) << "Cannot get RedPath query from embedded file "
                   << query_file.name;
        continue;
      }

      // Build a query plan if none exists for the query id
      if (id_to_query_plans_.contains(query.query_id())) continue;
      RedPathRedfishQueryParams params;
      if (auto iter = query_id_to_rules.find(query.query_id());
          iter != query_id_to_rules.end()) {
        params = std::move(iter->second);
      }

      absl::StatusOr<std::unique_ptr<QueryPlannerInterface>> query_planner =
          BuildDefaultQueryPlanner(query, std::move(params), normalizer_.get(),
                                  redfish_interface_.get());
      if (!query_planner.ok()) continue;
      id_to_query_plans_.emplace(query.query_id(), *std::move(query_planner));
    }
  }

  // Constructs QueryEngine to execute queries in the map |id_to_query_plans|.
  // When a valid |metrical_transport| instance is provided,  QueryEngine is
  // constructed to trace each query and return associated metrics in the
  // response.
  QueryEngineImpl(
      absl::flat_hash_map<std::string, std::unique_ptr<QueryPlannerInterface>>
          id_to_query_plans,
      const Clock *clock, std::unique_ptr<Normalizer> normalizer,
      std::unique_ptr<RedfishInterface> redfish_interface,
      MetricalRedfishTransport *metrical_transport = nullptr)
      : id_to_query_plans_(std::move(id_to_query_plans)),
        clock_(clock),
        normalizer_(std::move(normalizer)),
        redfish_interface_(std::move(redfish_interface)),
        metrical_transport_(metrical_transport) {}

  // If a metrical transport is in use, the metrics for all the queries will be
  // aggregated in its RedfishMetrics object. Invoked from
  // ExecuteQueryWithAggregatedMetrics. In the case a normal redfish transport
  // is in use, executes the query normally.
  std::vector<DelliciusQueryResult> ExecuteQueryForAggregatedMetrics(
      QueryEngine::ServiceRootType service_root_uri,
      absl::Span<const absl::string_view> query_ids,
      const QueryVariableSet &query_arguments, QueryTracker *tracker) {
    std::vector<DelliciusQueryResult> response_entries;

    for (const absl::string_view query_id : query_ids) {
      auto it = id_to_query_plans_.find(query_id);
      if (it == id_to_query_plans_.end()) {
        LOG(ERROR) << "Query plan does not exist for id " << query_id;
        continue;
      }
      QueryVariables vars = QueryVariables();
      auto it_vars = query_arguments.find(query_id);
      if (it_vars != query_arguments.end()) vars = query_arguments.at(query_id);

      DelliciusQueryResult result_single;
      {
        auto query_timer = QueryTimestamp(&result_single, clock_);
        if (service_root_uri == QueryEngine::ServiceRootType::kGoogle) {
          result_single = it->second->Run(
              redfish_interface_->GetRoot(GetParams{}, ServiceRootUri::kGoogle),
              *clock_, tracker, vars);
        } else {
          result_single = it->second->Run(redfish_interface_->GetRoot(),
                                          *clock_, tracker, vars);
        }
      }
      response_entries.push_back(std::move(result_single));
    }

    return response_entries;
  }

  // Executes queries and provides transport metrics for each
  // DelliciusQueryResult, and populates the redfish_metrics field in each
  // result.
  std::vector<DelliciusQueryResult> ExecuteQueryWithMetricsPerResult(
      QueryEngine::ServiceRootType service_root_uri,
      absl::Span<const absl::string_view> query_ids,
      const QueryVariableSet &query_arguments, QueryTracker *tracker) {
    std::vector<DelliciusQueryResult> response_entries;
    RedfishMetrics *metrics = nullptr;
    // In the case no RedfishMetrics Object is provided, we have to construct
    // one to use for populating the DelliciusQueryResult.
    RedfishMetrics local_metrics;
    if (metrical_transport_ != nullptr) {
      metrics = &local_metrics;
      metrical_transport_->ResetTrackingMetricsProto(metrics);
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

      DelliciusQueryResult result_single;
      {
        auto query_timer = QueryTimestamp(&result_single, clock_);
        if (service_root_uri == QueryEngine::ServiceRootType::kGoogle) {
          result_single = it->second->Run(
              redfish_interface_->GetRoot(GetParams{}, ServiceRootUri::kGoogle),
              *clock_, tracker, vars, metrics);
        } else {
          result_single = it->second->Run(redfish_interface_->GetRoot(),
                                          *clock_, tracker, vars, metrics);
        }
      }
      response_entries.push_back(std::move(result_single));
    }

    return response_entries;
  }

  // Executes Queries based on query_ids; auto-collects metrics into resultant
  // DelliciusQueryResults if QueryEngine was constructed with a metrical
  // transport.
  std::vector<DelliciusQueryResult> ExecuteQuery(
      QueryEngine::ServiceRootType service_root_uri,
      absl::Span<const absl::string_view> query_ids,
      const QueryVariableSet &query_arguments, QueryTracker *tracker) {
    // Execution for aggregated metrics is the same as no metrics desired.
    if (metrical_transport_ == nullptr) {
      return ExecuteQueryForAggregatedMetrics(service_root_uri, query_ids,
                                              query_arguments, tracker);
    }
    return ExecuteQueryWithMetricsPerResult(service_root_uri, query_ids,
                                            query_arguments, tracker);
  }

  // Main method for ExecuteQuery with callback with entry point into the
  // QueryPlanner.
  void ExecuteQuery(
      QueryEngine::ServiceRootType service_root_uri,
      absl::Span<const absl::string_view> query_ids,
      const QueryVariableSet &query_arguments,
      absl::FunctionRef<bool(const DelliciusQueryResult &result)> callback,
      QueryTracker *tracker) {
    RedfishMetrics metrics;
    if (metrical_transport_ != nullptr) {
      metrical_transport_->ResetTrackingMetricsProto(&metrics);
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

      if (service_root_uri == QueryEngine::ServiceRootType::kGoogle) {
        it->second->Run(
            redfish_interface_->GetRoot(GetParams{}, ServiceRootUri::kGoogle),
            *clock_, tracker, vars, callback);
      } else {
        it->second->Run(redfish_interface_->GetRoot(), *clock_, tracker, vars,
                        callback);
      }
    }
  }

  // ExecuteQuery Methods involving calling user supplied callback when
  // the SubqueryOutput is too large.
  void ExecuteQuery(QueryEngine::ServiceRootType service_root_uri,
                    absl::Span<const absl::string_view> query_ids,
                    const QueryVariableSet &query_arguments,
                    absl::FunctionRef<bool(const DelliciusQueryResult &result)>
                        callback) override {
    ExecuteQuery(service_root_uri, query_ids, query_arguments, callback,
                 nullptr);
  }

  void ExecuteQuery(
      QueryEngine::ServiceRootType service_root_uri,
      absl::Span<const absl::string_view> query_ids,
      const QueryVariableSet &query_arguments,
      absl::FunctionRef<bool(const DelliciusQueryResult &result)> callback,
      QueryTracker &tracker) override {
    ExecuteQuery(service_root_uri, query_ids, query_arguments, callback,
                 &tracker);
  }

  // ExecuteQuery methods returning results to user.
  std::vector<DelliciusQueryResult> ExecuteQuery(
      QueryEngine::ServiceRootType service_root_uri,
      absl::Span<const absl::string_view> query_ids,
      const QueryVariableSet &query_arguments = {}) override {
    return ExecuteQuery(service_root_uri, query_ids, query_arguments, nullptr);
  }

  std::vector<DelliciusQueryResult> ExecuteQuery(
      QueryEngine::ServiceRootType service_root_uri,
      absl::Span<const absl::string_view> query_ids,
      const QueryVariableSet &query_arguments, QueryTracker &tracker) override {
    return ExecuteQuery(service_root_uri, query_ids, query_arguments, &tracker);
  }

  // Deprecated API that writes metrics to user supplied RedfishMetrics object.
  std::vector<DelliciusQueryResult> ExecuteQueryWithAggregatedMetrics(
      QueryEngine::ServiceRootType service_root_uri,
      absl::Span<const absl::string_view> query_ids,
      RedfishMetrics *transport_metrics,
      const QueryVariableSet &query_arguments = {}) override {
    // Copies the new transport metrics over the previous metrics.
    if (metrical_transport_ != nullptr) {
      metrical_transport_->ResetTrackingMetricsProto(transport_metrics);
    }
    return ExecuteQueryForAggregatedMetrics(service_root_uri, query_ids,
                                            query_arguments, nullptr);
  }

  // Translates vector of  DelliciusQueryResult to new QueryResult format.
  static std::vector<QueryResult> TranslateLegacyResults(
      const std::vector<DelliciusQueryResult> &legacy_results) {
    std::vector<QueryResult> translated_results;
    std::transform(legacy_results.begin(), legacy_results.end(),
                   std::back_inserter(translated_results),
                   [](const DelliciusQueryResult &result) {
                     return ToQueryResult(result);
                   });
    return translated_results;
  }

  // Executes Redpath query and returns results in updated QueryResult format.
  std::vector<QueryResult> ExecuteRedpathQuery(
      QueryEngine::ServiceRootType service_root_uri,
      absl::Span<const absl::string_view> query_ids,
      const QueryVariableSet &query_arguments = {}) override {
    return TranslateLegacyResults(
        ExecuteQuery(service_root_uri, query_ids, query_arguments));
  }

  // Executes Redpath query and returns results in updated QueryResult format,
  // with a tracker.
  std::vector<QueryResult> ExecuteRedpathQuery(
      QueryEngine::ServiceRootType service_root_uri,
      absl::Span<const absl::string_view> query_ids,
      const QueryVariableSet &query_arguments, QueryTracker &tracker) override {
    return TranslateLegacyResults(
        ExecuteQuery(service_root_uri, query_ids, query_arguments, tracker));
  }

  const NodeTopology &GetTopology() override {
    if (absl::StatusOr<const NodeTopology *> topology =
            normalizer_->GetNodeTopology();
        topology.ok()) {
      return **topology;
    }
    return default_topology_;
  }

  std::vector<std::string> GetQueryIds() const override {
    std::vector<std::string> keys;
    keys.reserve(id_to_query_plans_.size());
    for (const auto &[query_id, plan] : id_to_query_plans_) {
      keys.push_back(query_id);
    }
    return keys;
  }

  absl::StatusOr<RedfishInterface *> GetRedfishInterface(
      RedfishInterfacePasskey unused_passkey) override {
    if (redfish_interface_ == nullptr) {
      return absl::InternalError(
          "QueryEngine contains uninitialized RedfishInterface");
    }
    return redfish_interface_.get();
  }

 private:
  absl::flat_hash_map<std::string, std::unique_ptr<QueryPlannerInterface>>
      id_to_query_plans_;
  const Clock *clock_;
  std::unique_ptr<Normalizer> normalizer_;
  std::unique_ptr<RedfishInterface> redfish_interface_;
  // empty topology to be returned if no normalizers are found with real
  // topology.
  NodeTopology default_topology_;

  // Used during query metrics collection.
  MetricalRedfishTransport *metrical_transport_ = nullptr;
};

}  // namespace

QueryEngine::QueryEngine(const QueryEngineConfiguration &config,
                         std::unique_ptr<RedfishTransport> transport,
                         RedfishTransportCacheFactory cache_factory,
                         const Clock *clock)
    : engine_impl_(std::make_unique<QueryEngineImpl>(
          config, std::move(transport), std::move(cache_factory), clock)) {}

absl::StatusOr<QueryEngine> CreateQueryEngine(
    const QueryContext &query_context,
    std::unique_ptr<RedfishInterface> redfish_interface,
    std::unique_ptr<Normalizer> normalizer,
    MetricalRedfishTransport *metrical_transport) {
  // Parse query rules from embedded proto messages
  absl::flat_hash_map<std::string, RedPathRedfishQueryParams>
      query_id_to_rules = ParseQueryRulesFromEmbeddedFiles(
          {query_context.query_rules.begin(), query_context.query_rules.end()});

  // Parse queries from embedded proto messages
  absl::flat_hash_map<std::string, std::unique_ptr<QueryPlannerInterface>>
      id_to_query_plans;
  for (const EmbeddedFile &query_file : query_context.query_files) {
    DelliciusQuery query;
    if (!google::protobuf::TextFormat::ParseFromString(std::string(query_file.data),
                                             &query)) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Cannot get RedPath query from embedded file ", query_file.name));
    }

    // Build a query plan if none exists for the query id
    if (id_to_query_plans.contains(query.query_id())) continue;
    RedPathRedfishQueryParams params;
    if (auto iter = query_id_to_rules.find(query.query_id());
        iter != query_id_to_rules.end()) {
      params = std::move(iter->second);
    }

    absl::StatusOr<std::unique_ptr<QueryPlannerInterface>> query_planner =
        BuildDefaultQueryPlanner(query, std::move(params), normalizer.get(),
                                redfish_interface.get());
    if (!query_planner.ok()) {
      return absl::InternalError(
          absl::StrCat("Cannot create query plan due to error: ",
                       query_planner.status().message()));
    }
    id_to_query_plans.insert({query.query_id(), *std::move(query_planner)});
  }
  return QueryEngine(std::make_unique<QueryEngineImpl>(
      std::move(id_to_query_plans), query_context.clock, std::move(normalizer),
      std::move(redfish_interface), metrical_transport));
}

absl::StatusOr<QueryEngine> CreateQueryEngine(const QueryContext &query_context,
                                              QueryEngineParams configuration) {
  std::unique_ptr<RedfishInterface> redfish_interface;
  MetricalRedfishTransport *metrical_transport_ptr = nullptr;
  // Build Redfish interface and metrical transport if desired.
  if (configuration.feature_flags.enable_redfish_metrics) {
    std::unique_ptr<MetricalRedfishTransport> metrical_transport =
        std::make_unique<MetricalRedfishTransport>(
            std::move(configuration.transport), ecclesia::Clock::RealClock(),
            nullptr);
    metrical_transport_ptr = metrical_transport.get();
    redfish_interface = NewHttpInterface(std::move(metrical_transport),
                                         std::move(configuration.cache_factory),
                                         RedfishInterface::kTrusted);
  } else {
    redfish_interface = NewHttpInterface(std::move(configuration.transport),
                                         std::move(configuration.cache_factory),
                                         RedfishInterface::kTrusted);
  }
  RedfishInterface *redfish_interface_ptr = redfish_interface.get();
  return CreateQueryEngine(
      query_context, std::move(redfish_interface),
      BuildLocalDevpathNormalizer(configuration.stable_id_type,
                                  redfish_interface_ptr),
      metrical_transport_ptr);
}

absl::StatusOr<QueryEngine> CreateQueryEngine(
    const QueryContext &query_context, QueryEngineParams engine_params,
    std::unique_ptr<IdAssigner> id_assigner) {
  std::unique_ptr<RedfishInterface> redfish_interface;
  MetricalRedfishTransport *metrical_transport_ptr = nullptr;
  if (engine_params.feature_flags.enable_redfish_metrics) {
    std::unique_ptr<MetricalRedfishTransport> metrical_transport =
        std::make_unique<MetricalRedfishTransport>(
            std::move(engine_params.transport), ecclesia::Clock::RealClock(),
            nullptr);
    metrical_transport_ptr = metrical_transport.get();
    redfish_interface = NewHttpInterface(std::move(metrical_transport),
                                         std::move(engine_params.cache_factory),
                                         RedfishInterface::kTrusted);
  } else {
    redfish_interface = NewHttpInterface(std::move(engine_params.transport),
                                         std::move(engine_params.cache_factory),
                                         RedfishInterface::kTrusted);
  }

  if (redfish_interface == nullptr)
    return absl::InternalError("Can't create redfish interface");
  std::unique_ptr<Normalizer> normalizer = GetMachineDevpathNormalizer(
      engine_params.stable_id_type, std::move(id_assigner),
      redfish_interface.get());

  return CreateQueryEngine(query_context, std::move(redfish_interface),
                           std::move(normalizer), metrical_transport_ptr);
}

}  // namespace ecclesia
