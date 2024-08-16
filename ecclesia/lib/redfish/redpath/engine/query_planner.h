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

#ifndef ECCLESIA_LIB_REDFISH_REDPATH_ENGINE_QUERY_PLANNER_H_
#define ECCLESIA_LIB_REDFISH_REDPATH_ENGINE_QUERY_PLANNER_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/time/time.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_variables.pb.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_engine/redpath_subscription.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/redfish/redpath/engine/normalizer.h"
#include "ecclesia/lib/redfish/redpath/engine/redpath_trie.h"
#include "ecclesia/lib/time/clock.h"

namespace ecclesia {

// Describes the type of query execution.
enum class QueryType : uint8_t {
  kPolling = 0,
  kSubscription,
};

struct RedPathRules {
  absl::flat_hash_map<std::string /* RedPath */, GetParams>
      redpath_to_query_params;
  absl::flat_hash_set<std::string /* RedPath */> redpaths_to_subscribe;
};

// Provides an interface for planning and executing a RedPath Query.
class QueryPlannerIntf {
 public:
  // Context provided by QueryPlanner to create an event subscription.
  struct SubscriptionContext {
    // RedPath expression and corresponding query planner trie node.
    // This mapping is used by query planner to resume query execution on an
    // event.
    absl::flat_hash_map<std::string, RedPathTrieNode *> redpath_to_trie_node;
    // Subscription configuration for each RedPath expression.
    std::vector<RedPathSubscription::Configuration> subscription_configs;
    // Copy of query variables to be used for templated queries.
    QueryVariables query_variables;
    bool log_redfish_traces = false;
  };

  // Tracks the execution of Redpath queries.
  struct RedpathQueryTracker {
    absl::flat_hash_map<std::string, GetParams>
        executed_redpath_prefixes_and_params;
  };

  // Configures a query execution.
  struct QueryExecutionOptions {
    QueryVariables &variables;
    bool enable_url_annotation = false;
    bool log_redfish_traces = false;
    RedpathQueryTracker *redpath_query_tracker = nullptr;
    QueryType query_type = QueryType::kPolling;
  };

  // Configures a query resume operation.
  struct QueryResumeOptions {
    const RedPathTrieNode *trie_node;
    const RedfishVariant &redfish_variant;
    const QueryVariables &variables;
    bool enable_url_annotation = false;
    bool log_redfish_traces = false;
    RedpathQueryTracker *redpath_query_tracker = nullptr;
  };

  // QueryPlan execution output.
  // Optionally contains a valid `subscription_context` when QueryPlan involves
  // subscribing to RedfishEvents.
  struct QueryExecutionResult {
    QueryResult query_result;
    std::unique_ptr<SubscriptionContext> subscription_context = nullptr;
  };

  virtual ~QueryPlannerIntf() = default;

  // Executes query plan based on `query_execution_options` and returns
  // `QueryExecutionResult`.
  // If query plan includes subscription, returns `RedPathSubscription` along
  // with a partial `QueryResult` in `QueryExecutionResult` for RedPath
  // expressions that are polled as part of subscription sequence.
  virtual QueryExecutionResult Run(
      QueryExecutionOptions query_execution_options) = 0;

  // Resumes a query execution after an event.
  virtual QueryResult Resume(QueryResumeOptions query_resume_options) = 0;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_REDPATH_ENGINE_QUERY_PLANNER_H_
