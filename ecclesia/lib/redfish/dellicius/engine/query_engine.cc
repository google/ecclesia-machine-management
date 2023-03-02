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
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ecclesia/lib/file/cc_embed_interface.h"
#include "ecclesia/lib/redfish/dellicius/engine/config.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/factory.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/interface.h"
#include "ecclesia/lib/redfish/dellicius/engine/query_rules.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/dellicius/utils/parsers.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/node_topology.h"
#include "ecclesia/lib/redfish/topology.h"
#include "ecclesia/lib/time/clock.h"
#include "google/protobuf/text_format.h"

namespace ecclesia {

namespace {

class QueryEngineImpl final : public QueryEngine::QueryEngineIntf {
 public:
  QueryEngineImpl(const QueryEngineConfiguration &config, const Clock *clock,
                  std::unique_ptr<RedfishInterface> intf)
      : clock_(clock), intf_(std::move(intf)) {
    if (config.flags.enable_devpath_extension) {
      topology_ = CreateTopologyFromRedfish(intf_.get());
      normalizer_ = BuildDefaultNormalizerWithDevpath(topology_);
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
      absl::StatusOr<QueryPlannerInterface> query_planner;
      if (auto iter = query_id_to_rules.find(query.query_id());
          iter != query_id_to_rules.end()) {
        query_planner = BuildQueryPlanner(query, std::move(iter->second),
                                          normalizer_.get());
      } else {
        query_planner = BuildQueryPlanner(query, RedPathRedfishQueryParams{},
                                          normalizer_.get());
      }
      if (!query_planner.ok()) continue;
      id_to_query_plans_.emplace(query.query_id(), std::move(*query_planner));
    }
  }

  std::vector<DelliciusQueryResult> ExecuteQuery(
      absl::Span<const absl::string_view> query_ids, QueryTracker *tracker) {
    std::vector<DelliciusQueryResult> response_entries;
    for (const absl::string_view query_id : query_ids) {
      auto it = id_to_query_plans_.find(query_id);
      if (it == id_to_query_plans_.end()) {
        LOG(ERROR) << "Query plan does not exist for id " << query_id;
        continue;
      }

      absl::StatusOr<DelliciusQueryResult> result_single =
          it->second.Run(intf_->GetRoot(), *clock_, tracker);
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
      absl::Span<const absl::string_view> query_ids) override {
    return ExecuteQuery(query_ids, nullptr);
  }

  std::vector<DelliciusQueryResult> ExecuteQuery(
      absl::Span<const absl::string_view> query_ids,
      QueryTracker &tracker) override {
    return ExecuteQuery(query_ids, &tracker);
  }

 private:
  // Data normalizer to inject in QueryPlanner for normalizing redfish
  // response per a given property specification in dellicius subquery.
  absl::flat_hash_map<std::string, QueryPlannerInterface> id_to_query_plans_;
  const Clock *clock_;
  std::unique_ptr<Normalizer> normalizer_;
  std::unique_ptr<RedfishInterface> intf_;
  NodeTopology topology_;
};

}  // namespace

QueryEngine::QueryEngine(const QueryEngineConfiguration &config,
                         const Clock *clock,
                         std::unique_ptr<RedfishInterface> intf)
    : engine_impl_(
          std::make_unique<QueryEngineImpl>(config, clock, std::move(intf))) {}

}  // namespace ecclesia
