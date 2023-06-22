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
#include "absl/log/log.h"
#include "absl/status/statusor.h"
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
#include "google/protobuf/text_format.h"

namespace ecclesia {

namespace {

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
      intf_ = NewHttpInterface(std::move(metrical_transport),
                               std::move(cache_factory),
                               RedfishInterface::kTrusted);
    } else {
      intf_ = NewHttpInterface(std::move(transport), std::move(cache_factory),
                               RedfishInterface::kTrusted);
    }

    if (config.flags.enable_devpath_extension) {
      topology_ = CreateTopologyFromRedfish(intf_.get());
      normalizer_ = BuildDefaultNormalizerWithLocalDevpath(topology_);
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
      RedPathRedfishQueryParams params{};
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

      absl::StatusOr<DelliciusQueryResult> result_single;
      if (service_root_uri == QueryEngine::ServiceRootType::kGoogle) {
        result_single = it->second->Run(
            intf_->GetRoot(GetParams{}, ServiceRootUri::kGoogle), *clock_,
            tracker);
      } else {
        result_single = it->second->Run(intf_->GetRoot(), *clock_, tracker);
      }

      if (!result_single.ok()) {
        LOG(ERROR) << "Query Failed for id: " << query_id
                   << " Reason: " << result_single.status();
        continue;
      }
      response_entries.push_back(std::move(*result_single));
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

  const NodeTopology &GetTopology() override { return topology_; }

 private:
  // Data normalizer to inject in QueryPlanner for normalizing redfish
  // response per a given property specification in dellicius subquery.
  absl::flat_hash_map<std::string, std::unique_ptr<QueryPlannerInterface>>
      id_to_query_plans_;
  const Clock *clock_;
  std::unique_ptr<Normalizer> normalizer_;
  std::unique_ptr<RedfishInterface> intf_;
  NodeTopology topology_;

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

}  // namespace ecclesia
