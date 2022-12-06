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
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/time/clock.h"
#include "google/protobuf/text_format.h"

namespace ecclesia {

namespace {

using ExpandConfiguration = RedPathPrefixWithQueryParams::ExpandConfiguration;

absl::flat_hash_map<std::string, RedPathRedfishQueryParams>
GetQueryRulesFromEmbeddedFiles(const QueryEngineConfiguration &config) {
  absl::flat_hash_map<std::string, RedPathRedfishQueryParams>
      parsed_query_rules;
  // Extract query rules from embedded query files.
  for (const EmbeddedFile &query_rules_file : config.query_rules) {
    QueryRules query_rules;
    // Parse query rules into embedded file object.
    if (!google::protobuf::TextFormat::ParseFromString(std::string(query_rules_file.data),
                                             &query_rules))
      continue;
    // Extract RedPath prefix to query params map for each query id.
    for (const auto &[query_id, prefix_set_with_query_params] :
         query_rules.query_id_to_params_rule()) {
      RedPathRedfishQueryParams redpath_prefix_to_params;
      // Iterate over each pair of RedPath prefix and Redfish query parameter
      // configuration and build the prefix to param mapping in memory.
      for (const auto &redpath_prefix_with_query_params :
           prefix_set_with_query_params.redpath_prefix_with_params()) {
        RedfishQueryParamExpand::ExpandType expand_type;
        ExpandConfiguration::ExpandType expand_type_in_rule =
            redpath_prefix_with_query_params.expand_configuration().type();
        if (expand_type_in_rule == ExpandConfiguration::BOTH) {
          expand_type = RedfishQueryParamExpand::kBoth;
        } else if (expand_type_in_rule == ExpandConfiguration::NO_LINKS) {
          expand_type = RedfishQueryParamExpand::kNotLinks;
        } else if (expand_type_in_rule == ExpandConfiguration::ONLY_LINKS) {
          expand_type = RedfishQueryParamExpand::kLinks;
        } else {
          break;
        }
        GetParams params{.expand = RedfishQueryParamExpand(
                             {.type = expand_type,
                              .levels = redpath_prefix_with_query_params
                                            .expand_configuration()
                                            .level()})};

        redpath_prefix_to_params[redpath_prefix_with_query_params.redpath()] =
            params;
      }
      parsed_query_rules[query_id] = redpath_prefix_to_params;
    }
  }
  return parsed_query_rules;
}

class QueryEngineImpl final : public QueryEngine::QueryEngineIntf {
 public:
  QueryEngineImpl(const QueryEngineConfiguration &config, const Clock *clock,
                  std::unique_ptr<RedfishInterface> intf)
      : clock_(clock), intf_(std::move(intf)) {
    if (config.flags.enable_devpath_extension) {
      normalizer_ = BuildDefaultNormalizerWithDevpath(intf_.get());
    } else {
      normalizer_ = BuildDefaultNormalizer();
    }

    // Parse query rules from embedded proto messages
    absl::flat_hash_map<std::string, RedPathRedfishQueryParams>
        query_id_to_rules = GetQueryRulesFromEmbeddedFiles(config);
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
      std::unique_ptr<QueryPlannerInterface> query_planner;
      if (auto iter = query_id_to_rules.find(query.query_id());
          iter != query_id_to_rules.end()) {
        query_planner = BuildQueryPlanner(query, std::move(iter->second),
                                          normalizer_.get());
      } else {
        query_planner = BuildQueryPlanner(query, RedPathRedfishQueryParams{},
                                          normalizer_.get());
      }
      id_to_query_plans_.emplace(query.query_id(), std::move(query_planner));
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
      if (it->second == nullptr) {
        LOG(ERROR) << "Query plan is null for id " << query_id;
        continue;
      }
      DelliciusQueryResult result_single =
          it->second->Run(intf_->GetRoot(), *clock_, tracker);
      response_entries.push_back(std::move(result_single));
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
  // Data normalizer to inject in QueryPlanner for normalizing redfish response
  // per a given property specification in dellicius subquery.
  absl::flat_hash_map<std::string, std::unique_ptr<QueryPlannerInterface>>
      id_to_query_plans_;
  const Clock *clock_;
  std::unique_ptr<Normalizer> normalizer_;
  std::unique_ptr<RedfishInterface> intf_;
};

}  // namespace

QueryEngine::QueryEngine(const QueryEngineConfiguration &config,
                         const Clock *clock,
                         std::unique_ptr<RedfishInterface> intf)
    : engine_impl_(
          std::make_unique<QueryEngineImpl>(config, clock, std::move(intf))) {}

}  // namespace ecclesia
