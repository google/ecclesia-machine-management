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

#ifndef ECCLESIA_LIB_REDFISH_REDPATH_ENGINE_QUERY_PLANNER_IMPL_H_
#define ECCLESIA_LIB_REDFISH_REDPATH_ENGINE_QUERY_PLANNER_IMPL_H_

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/log/die_if_null.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/redfish/redpath/engine/normalizer.h"
#include "ecclesia/lib/redfish/redpath/engine/query_planner.h"
#include "ecclesia/lib/redfish/redpath/engine/redpath_trie.h"
#include "ecclesia/lib/redfish/timing/query_timeout_manager.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/time/clock.h"

namespace ecclesia {

using SubqueryIdToSubquery =
    absl::flat_hash_map<std::string, DelliciusQuery::Subquery>;

// Describes the Redfish resource relative to which RedPath expressions execute.
struct RedfishResponse {
  // A singleton Redfish resource.
  std::unique_ptr<RedfishObject> redfish_object = nullptr;
  // Redfish collection or any iterable Redfish object - collection of primitive
  // or complex types.
  std::unique_ptr<RedfishIterable> redfish_iterable = nullptr;
  std::unique_ptr<RedfishTransport::bytes> redfish_raw_bytes = nullptr;
};

// Tracks RedPath prefixes executed along with Redfish QueryParameters used
// in QueryExecution.
struct RedPathPrefixTracker {
  std::string last_redpath_prefix;
};

// Describes the context used to execute a RedPath expression.
// It contains the Redfish resource as RedfishIterable or RedfishObject and
// any context necessary to execute RedPath expression relative to the resource.
// Therefore, each Redfish resource will be assigned an execution context if
// a RedPath expression needs to execute relative that Redfish resource.
struct QueryExecutionContext {
  // QueryResult obtained to store the result of the execution.
  QueryResult &result;
  absl::flat_hash_map<std::string, QueryValue *> subquery_id_to_subquery_result;
  // RedPath Trie Node that containing next set of RedPath expressions to
  // execute.
  const RedPathTrieNode *redpath_trie_node = nullptr;
  // QueryVariables used to execute a RedPath expression.
  const QueryVariables &query_variables;
  // Tracks RedPath prefixes executed.
  RedPathPrefixTracker redpath_prefix_tracker;
  // Tracks RedPath prefixes and the Params
  QueryPlannerIntf::RedpathQueryTracker *redpath_query_tracker = nullptr;
  // Redfish resource relative to which RedPath expressions execute.
  RedfishResponse redfish_response;

  // Redfish URIs to subscribe to.
  // Always empty unless subscription is requested.
  std::vector<std::string> uris_to_subscribe;

  QueryExecutionContext FromExisting(const std::string &new_redpath_prefix,
                                     const GetParams &get_params_for_redpath,
                                     RedfishResponse redfish_response);

  QueryExecutionContext(
      QueryResult *result_in,
      const absl::flat_hash_map<std::string, QueryValue *>
          &subquery_id_to_subquery_result_in,
      const QueryVariables *query_variables_in,
      RedPathPrefixTracker redpath_prefix_tracker_in,
      QueryPlannerIntf::RedpathQueryTracker *redpath_query_tracker_in,
      RedfishResponse redfish_object_and_iterable_in = {
          /*redfish_object=*/nullptr, /*redfish_iterable=*/nullptr})
      : result(*ABSL_DIE_IF_NULL(result_in)),
        subquery_id_to_subquery_result(subquery_id_to_subquery_result_in),
        query_variables(*ABSL_DIE_IF_NULL(query_variables_in)),
        redpath_prefix_tracker(std::move(redpath_prefix_tracker_in)),
        redpath_query_tracker(redpath_query_tracker_in),
        redfish_response(std::move(redfish_object_and_iterable_in)) {}
};

// Encapsulates information relevant per redfish object that is queried during
// the overall query execution process; used to tag redfish jsons when logging.
struct TraceInfo {
  std::string redpath_prefix;
  std::string redpath_query;
  std::string query_id;
  std::string redpath_node;
};

// Holds cache hit and miss counters.
struct CacheStats {
  std::atomic<int64_t> cache_miss{0};
  std::atomic<int64_t> cache_hit{0};

  void Reset() {
    cache_miss = 0;
    cache_hit = 0;
  }
};

// QueryPlannerImpl encapsulates the logic to interpret subqueries, deduplicate
// RedPath path expressions, dispatch an optimum number of redfish resource
// requests, and return normalized response data per given property requirement.
//
// QueryPlannerImpl is a thread safe implementation.
class QueryPlanner final : public QueryPlannerIntf {
 public:
  enum class ExecutionMode : std::uint8_t {
    kFailOnFirstError,
    kContinueOnSubqueryErrors
  };

  struct ImplOptions {
    const DelliciusQuery *query = nullptr;
    RedpathNormalizer *normalizer = nullptr;
    RedfishInterface *redfish_interface = nullptr;
    std::unique_ptr<RedPathTrieNode> redpath_trie_node = nullptr;
    RedPathRules redpath_rules;
    const Clock *clock = nullptr;
    const std::optional<absl::Duration> query_timeout = std::nullopt;
    ExecutionMode execution_mode = ExecutionMode::kFailOnFirstError;
  };

  explicit QueryPlanner(ImplOptions options_in);

  GetParams GetQueryParamsForRedPath(absl::string_view redpath_prefix);

  // Normalization of Redfish Object into query result is a best effort process
  // which does not error out if requested properties are not found in Redfish
  // Object.
  absl::Status TryNormalize(
      absl::string_view subquery_id,
      QueryExecutionContext *query_execution_context,
      const RedpathNormalizerOptions &normalizer_options) const;

  // Executes a single RedPath expression.
  absl::StatusOr<std::vector<QueryExecutionContext>> ExecuteQueryExpression(
      QueryType query_type, const RedPathExpression &expression,
      QueryExecutionContext &current_execution_context,
      std::optional<TraceInfo> &trace_info);

  void PopulateSubscriptionContext(
      const std::vector<QueryExecutionContext> &execution_contexts,
      QueryExecutionContext &current_execution_context,
      const RedPathExpression &expression,
      const QueryPlannerIntf::QueryExecutionOptions &query_execution_options,
      std::unique_ptr<QueryPlannerIntf::SubscriptionContext>
          &subscription_context);

  // Executes Query Plan per given `query_execution_options`.
  QueryExecutionResult Run(
      QueryExecutionOptions query_execution_options) override;

  // Resumes execution of a RedPath expression on receiving event.
  QueryResult Resume(QueryResumeOptions query_resume_options) override;

 private:
  const DelliciusQuery &query_;
  const std::string plan_id_;
  // RedpathNormalizer is thread safe.
  RedpathNormalizer &normalizer_;
  const std::unique_ptr<RedPathTrieNode> redpath_trie_node_;
  const RedPathRules redpath_rules_;
  // Redfish interface is thread safe.
  RedfishInterface &redfish_interface_;
  const std::string service_root_;
  const SubqueryIdToSubquery subquery_id_to_subquery_;
  const absl::flat_hash_map<std::string, std::vector<std::string>>
      subquery_id_to_root_ids_;
  const Clock *clock_ = nullptr;
  CacheStats cache_stats_;
  std::unique_ptr<QueryTimeoutManager> timeout_manager_ = nullptr;
  const ExecutionMode execution_mode_;
};

absl::StatusOr<std::unique_ptr<QueryPlannerIntf>> BuildQueryPlanner(
    QueryPlanner::ImplOptions query_planner_options);
}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_REDPATH_ENGINE_QUERY_PLANNER_IMPL_H_
