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
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/interface.h"
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
      normalizer_ = BuildDefaultNormalizerWithDevpath(intf_.get());
    } else {
      normalizer_ = BuildDefaultNormalizer();
    }
    // Parse queries from embedded proto messages
    for (const EmbeddedFile &query_file : config.query_files) {
      DelliciusQuery query;
      if (google::protobuf::TextFormat::ParseFromString(std::string(query_file.data),
                                              &query)) {
        // Builds a query plan
        std::unique_ptr<QueryPlannerInterface> query_planner =
            BuildQueryPlanner(query, normalizer_.get());
        id_to_query_plans_.emplace(query.query_id(), std::move(query_planner));
      }
    }
  }

  std::vector<DelliciusQueryResult> ExecuteQuery(
      absl::Span<const absl::string_view> query_ids) override {
    std::vector<DelliciusQueryResult> response_entries;
    // them sequentially.
    for (const absl::string_view query_id : query_ids) {
      auto it = id_to_query_plans_.find(query_id);
      if (it == id_to_query_plans_.end()) {
        LOG(ERROR) << "Query plan does not exist for id " << query_id;
        continue;
      }
      if (it->second == nullptr) {
        LOG(ERROR) << "Query plan is null for id " << query_id;
      }
      DelliciusQueryResult result_single;
      it->second->Run(intf_->GetRoot(), *clock_, result_single);
      response_entries.push_back(std::move(result_single));
    }
    return response_entries;
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
