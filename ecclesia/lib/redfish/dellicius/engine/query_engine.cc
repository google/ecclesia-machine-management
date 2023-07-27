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

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/log/die_if_null.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ecclesia/lib/file/cc_embed_interface.h"
#include "ecclesia/lib/redfish/dellicius/engine/config.h"
#include "ecclesia/lib/redfish/dellicius/engine/factory.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/interface.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/query_planner.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/dellicius/utils/parsers.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/node_topology.h"
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
          BuildDefaultQueryPlanner(query, std::move(params), normalizer_.get());
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

  std::vector<DelliciusQueryResult> ExecuteQuery(
      QueryEngine::ServiceRootType service_root_uri,
      absl::Span<const absl::string_view> query_ids, QueryTracker *tracker) {
    std::vector<DelliciusQueryResult> response_entries;
    for (const absl::string_view query_id : query_ids) {
      auto it = id_to_query_plans_.find(query_id);
      if (it == id_to_query_plans_.end()) {
        LOG(ERROR) << "Query plan does not exist for id " << query_id;
        continue;
      }

      DelliciusQueryResult result_single;
      {
        auto query_timer = QueryTimestamp(&result_single, clock_);
        if (service_root_uri == QueryEngine::ServiceRootType::kGoogle) {
          result_single = it->second->Run(
              redfish_interface_->GetRoot(GetParams{}, ServiceRootUri::kGoogle),
              *clock_, tracker);
        } else {
          result_single =
              it->second->Run(redfish_interface_->GetRoot(), *clock_, tracker);
        }
      }

      response_entries.push_back(std::move(result_single));
    }
    return response_entries;
  }

  std::vector<DelliciusQueryResult> ExecuteQuery(
      QueryEngine::ServiceRootType service_root_uri,
      absl::Span<const absl::string_view> query_ids) override {
    return ExecuteQuery(service_root_uri, query_ids, nullptr);
  }

  std::vector<DelliciusQueryResult> ExecuteQuery(
      QueryEngine::ServiceRootType service_root_uri,
      absl::Span<const absl::string_view> query_ids,
      QueryTracker &tracker) override {
    return ExecuteQuery(service_root_uri, query_ids, &tracker);
  }
  std::vector<DelliciusQueryResult> ExecuteQueryWithMetrics(
      QueryEngine::ServiceRootType service_root_uri,
      absl::Span<const absl::string_view> query_ids,
      RedfishMetrics *transport_metrics) override {
    // Copies the new transport metrics over the previous metrics.
    if (metrical_transport_ != nullptr) {
      metrical_transport_->ResetTrackingMetricsProto(transport_metrics);
    }
    return ExecuteQuery(service_root_uri, query_ids, nullptr);
  }

  const NodeTopology &GetTopology() override {
    if (absl::StatusOr<const NodeTopology *> topology =
            normalizer_->GetNodeTopology();
        topology.ok()) {
      return **topology;
    }
    return default_topology_;
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
    std::unique_ptr<Normalizer> normalizer) {
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
        BuildDefaultQueryPlanner(query, std::move(params), normalizer.get());
    if (!query_planner.ok()) {
      return absl::InternalError(
          absl::StrCat("Cannot create query plan due to error: ",
                       query_planner.status().message()));
    }
    id_to_query_plans.insert({query.query_id(), *std::move(query_planner)});
  }
  return QueryEngine(std::make_unique<QueryEngineImpl>(
      std::move(id_to_query_plans), query_context.clock, std::move(normalizer),
      std::move(redfish_interface)));
}

absl::StatusOr<QueryEngine> CreateQueryEngine(const QueryContext &query_context,
                                              QueryEngineParams configuration) {
  // Build Redfish interface
  std::unique_ptr<RedfishInterface> redfish_interface = NewHttpInterface(
      std::move(configuration.transport),
      std::move(configuration.cache_factory), RedfishInterface::kTrusted);

  RedfishInterface *redfish_interface_ptr = redfish_interface.get();
  return CreateQueryEngine(
      query_context, std::move(redfish_interface),
      BuildLocalDevpathNormalizer(configuration.stable_id_type,
                                  redfish_interface_ptr));
}

}  // namespace ecclesia
