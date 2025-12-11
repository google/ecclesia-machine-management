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

#include <stdbool.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/log/die_if_null.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
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
#include "ecclesia/lib/time/proto.h"

namespace ecclesia {

// Encapsulates the associations between subquery ids and root subquery ids.
struct SubqueryAssociations {
  absl::flat_hash_map<std::string, std::vector<std::string>>
      subquery_id_to_root_ids;
  absl::flat_hash_map<std::string, std::vector<std::string>>
      root_id_to_subquery_ids;
  absl::flat_hash_map<std::string, DelliciusQuery::Subquery>
      subquery_id_to_subquery;

  explicit SubqueryAssociations(const DelliciusQuery &query) {
    for (const auto &subquery : query.subquery()) {
      subquery_id_to_subquery[subquery.subquery_id()] = subquery;
      std::vector<std::string> root_ids;
      for (const auto &root_id : subquery.root_subquery_ids()) {
        root_ids.push_back(root_id);
      }
      if (root_ids.empty()) {
        continue;
      }
      for (const auto &root_id : root_ids) {
        root_id_to_subquery_ids[root_id].push_back(subquery.subquery_id());
      }
      subquery_id_to_root_ids[subquery.subquery_id()] = std::move(root_ids);
    }
  }
};

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
                                     RedfishResponse redfish_response,
                                     bool is_query_cancelled = false);

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

// Helper function to get the URI of a child resource from the parent resource's
// Redfish response if the child resource is inaccessible.
absl::StatusOr<std::string> GetChildUriFromNode(
  const QueryExecutionContext &execution_context,
  const std::string &node_name);

// Helper function to get the URI of a child resource from the Members list of
// the parent's Redfish response if the child resource is inaccessible.
absl::StatusOr<std::string> GetChildUriFromIterable(
  const QueryExecutionContext &execution_context, int index);

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

// RAII style wrapper to timestamp query.
class RedpathQueryTimestamp {
 public:
  RedpathQueryTimestamp(QueryPlannerIntf::QueryExecutionResult *result,
                        const Clock *clock)
      : result_(*ABSL_DIE_IF_NULL(result)),
        clock_(*ABSL_DIE_IF_NULL(clock)),
        start_time_(clock_.Now()) {}

  ~RedpathQueryTimestamp() {
    auto set_time = [](absl::Time time, google::protobuf::Timestamp &field) {
      if (auto timestamp = AbslTimeToProtoTime(time); timestamp.ok()) {
        field = *std::move(timestamp);
      }
    };
    set_time(start_time_,
             *result_.query_result.mutable_stats()->mutable_start_time());
    set_time(clock_.Now(),
             *result_.query_result.mutable_stats()->mutable_end_time());
  }

 private:
  QueryPlannerIntf::QueryExecutionResult &result_;
  const Clock &clock_;
  absl::Time start_time_;
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
    std::vector<RedpathNormalizer *> additional_normalizers;
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
  absl::Status TryNormalize(absl::string_view subquery_id,
                            RedfishInterface* redfish_interface,
                            QueryExecutionContext* query_execution_context,
                            const RedpathNormalizerOptions& normalizer_options);

  // Final normalization with additional normalizers, running against the final
  // query result, after all the subqueries are executed.
  // The status of the query result will be updated if any of the additional
  // normalizers fail and this should be last step in the query execution, so we
  // don't need to check the status after this function call.
  void TryNormalizeOnFinalQueryResult(
      ecclesia::QueryResult &query_result,
      const RedpathNormalizerOptions &normalizer_options);

  // Executes a single RedPath expression.
  absl::StatusOr<std::vector<QueryExecutionContext>> ExecuteQueryExpression(
      QueryType query_type, const RedPathExpression &expression,
      QueryExecutionContext &current_execution_context,
      std::optional<TraceInfo> &trace_info,
      RedfishInterface *redfish_interface = nullptr);

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

  void SetQueryCancellationState(bool is_query_execution_cancelled) override {
    absl::MutexLock lock(&query_cancellation_state_mutex_);
    query_cancellation_state_ = is_query_execution_cancelled;
  }

 private:
  bool IsQueryExecutionCancelled() {
    absl::MutexLock lock(&query_cancellation_state_mutex_);
    return query_cancellation_state_;
  }

  const DelliciusQuery query_;
  const std::string plan_id_;
  // RedpathNormalizer is thread safe.
  RedpathNormalizer &normalizer_;
  const std::vector<RedpathNormalizer *> additional_normalizers_;
  const std::unique_ptr<RedPathTrieNode> redpath_trie_root_;
  const RedPathRules redpath_rules_;
  // Redfish interface is thread safe.
  RedfishInterface *redfish_interface_ = nullptr;
  const std::string service_root_;
  const SubqueryAssociations subquery_associations_;
  const Clock *clock_ = nullptr;
  CacheStats cache_stats_;
  std::unique_ptr<QueryTimeoutManager> timeout_manager_ = nullptr;
  const ExecutionMode execution_mode_;

  // Tracks the query cancellation.
  // This flag is set to true when query cancellation is initiated and is reset
  // when query cancellation is completed.
  absl::Mutex query_cancellation_state_mutex_;
  bool query_cancellation_state_
      ABSL_GUARDED_BY(query_cancellation_state_mutex_) = false;
};

absl::StatusOr<std::unique_ptr<QueryPlannerIntf>> BuildQueryPlanner(
    QueryPlanner::ImplOptions query_planner_options);
}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_REDPATH_ENGINE_QUERY_PLANNER_IMPL_H_
