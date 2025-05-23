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

#include "ecclesia/lib/redfish/redpath/engine/query_planner_impl.h"

#include <time.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/die_if_null.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_errors.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_variables.pb.h"
#include "ecclesia/lib/redfish/dellicius/utils/path_util.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/property_definitions.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_engine/redpath_subscription.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_predicates/filter.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_predicates/predicates.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_predicates/variable_substitution.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/redfish/redpath/engine/normalizer.h"
#include "ecclesia/lib/redfish/redpath/engine/query_planner.h"
#include "ecclesia/lib/redfish/redpath/engine/redpath_trie.h"
#include "ecclesia/lib/redfish/timing/query_timeout_manager.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/redfish/transport/metrical_transport.h"
#include "ecclesia/lib/status/macros.h"
#include "ecclesia/lib/time/clock.h"
#include "single_include/nlohmann/json.hpp"
#include "re2/re2.h"

namespace ecclesia {

namespace {

#define TRACE(trace_info, prefix, params, node) \
  if (trace_info.has_value()) {                 \
    trace_info->redpath_prefix = prefix;        \
    trace_info->redpath_query = params;         \
    trace_info->redpath_node = node;            \
  }

using RedPathRedfishQueryParams =
    absl::flat_hash_map<std::string /* RedPath */, GetParams>;

// Pattern for location step: NodeName[Predicate]
constexpr LazyRE2 kLocationStepRegex = {
    "^([a-zA-Z#@][0-9a-zA-Z.]+|)(?:\\[(.*?)\\]|)$"};
constexpr absl::string_view kPredicateSelectAll = "*";

// All RedPath expressions execute relative to service root identified by '/'.
constexpr absl::string_view kServiceRootNode = "/";
constexpr absl::string_view kDefaultRedfishServiceRoot = "/redfish/v1";

// Returns navigational property from given redfish object
absl::StatusOr<std::string> GetNavigationalPropertyToSubscribe(
    const std::unique_ptr<RedfishObject> &current_redfish_obj,
    const RedPathExpression &expression,
    const GetParams &get_params_for_redpath) {
  nlohmann::json json = current_redfish_obj->GetContentAsJson();
  auto find_node_name = json.find(expression.expression);
  if (find_node_name == json.end()) {
    return absl::InternalError(absl::StrCat(
        "Cannot find ",
        (expression.type == RedPathExpression::Type::kPredicate)
            ? absl::StrCat("predicate [", expression.expression, "]")
            : absl::StrCat("node name ", expression.expression),
        " in Redfish Object for subscription."));
  }

  auto find_navigational_property = find_node_name->find(PropertyOdataId::Name);
  if (find_navigational_property == find_node_name->end()) {
    return absl::InternalError(absl::StrCat(
        "Cannot find navigational property: ", PropertyOdataId::Name,
        " in Redfish Object for subscription."));
  }
  // Append query parameters to navigational property.
  std::string params = get_params_for_redpath.ToString();
  std::string navigational_property =
      absl::StrCat(find_navigational_property->get<std::string>(),
                   params.empty() ? "" : absl::StrCat("?", params));
  return navigational_property;
}

// Joins next RedPath expression to given RedPath prefix.
// If ignore predicate is true, inserts a predicate `[*]` in place of given
// predicate expression.
std::string AddExpressionToRedPath(absl::string_view redpath_prefix,
                                   const RedPathExpression &expression,
                                   bool ignore_predicate = true) {
  if (expression.type == RedPathExpression::Type::kPredicate) {
    return absl::StrCat(
        redpath_prefix, "[",
        ignore_predicate ? kPredicateSelectAll : expression.expression, "]");
  }
  return absl::StrCat(redpath_prefix, "/", expression.expression);
}

// Generate a $filter string based on the predicates listed as children of this
// node.
absl::StatusOr<std::string> GetFilterStringFromNextNode(
    RedPathTrieNode *next_trie_node,
    const QueryExecutionContext &execution_context) {
  std::vector<std::string> predicates;
  for (const auto &expression : next_trie_node->child_expressions) {
    if (expression.type == RedPathExpression::Type::kPredicate) {
      // If some of the predicates are invalid for $filter, ie
      // "[*]" or "[Property]" the entire filter generation will fail, which is
      // intended behavior.
      ECCLESIA_ASSIGN_OR_RETURN(
          std::string new_predicate,
          SubstituteVariables(expression.expression,
                              execution_context.query_variables));
      predicates.push_back(std::move(new_predicate));
    }
  }
  return BuildFilterFromRedpathPredicateList(predicates);
}

// Returns true if child RedPath is in expand path of parent RedPath.
bool IsInExpandPath(absl::string_view child_redpath,
                    absl::string_view parent_redpath,
                    size_t parent_expand_levels) {
  size_t expand_levels = 0;
  // Get the diff expression between the 2 RedPaths
  // Example diff for paths /Chassis[*] and /Chassis[*]/Sensors[*] would be
  // /Sensors[*]
  absl::string_view diff = child_redpath.substr(parent_redpath.length());
  std::vector<absl::string_view> step_expressions =
      absl::StrSplit(diff, '/', absl::SkipEmpty());
  // Now count the possible expand levels in the diff expression.
  for (absl::string_view step_expression : step_expressions) {
    std::string node_name;
    std::string predicate;
    if (RE2::FullMatch(step_expression, *kLocationStepRegex, &node_name,
                       &predicate)) {
      if (!node_name.empty()) {
        ++expand_levels;
      }
      if (!predicate.empty()) {
        ++expand_levels;
      }
    }
  }
  return expand_levels <= parent_expand_levels;
}

// Returns combined GetParams{} for RedPath expressions in the query.
// There are 2 places where we get parameters associated with RedPath prefix:
// 1) Expand configuration from embedded query_rule file
// 2) Freshness configuration in the Query itself
// In this function, we merge Freshness requirement with expand configuration
// for the redpath prefix.
RedPathRedfishQueryParams CombineQueryParams(
    const DelliciusQuery &query,
    RedPathRedfishQueryParams redpath_to_query_params,
    const absl::flat_hash_set<std::vector<std::string>>
        &all_joined_subqueries) {
  absl::flat_hash_map<std::string, DelliciusQuery::Subquery> subquery_map;
  for (const auto &subquery : query.subquery()) {
    subquery_map[subquery.subquery_id()] = subquery;
  }

  // For each RedPath prefix in joined subquery, get freshness requirement from
  // corresponding subquery.
  for (const auto &subquery_id_list : all_joined_subqueries) {
    std::string redpath_prefix;
    for (const auto &subquery_id : subquery_id_list) {
      auto iter = subquery_map.find(subquery_id);
      CHECK(iter != subquery_map.end())
          << "Subquery not found for id: " << subquery_id;
      std::string redpath_str = iter->second.redpath();

      // Convert all predicates in RedPath to [*].
      // This is done because engine fetches all members in a collection before
      // applying a predicate expression to filter, which internally is [*]
      // operation.
      RE2::GlobalReplace(&redpath_str, "\\[(.*?)\\]", "[*]");
      if (!absl::StartsWith(redpath_str, "/")) {
        absl::StrAppend(&redpath_prefix, "/", redpath_str);
      } else {
        absl::StrAppend(&redpath_prefix, redpath_str);
      }

      if (iter->second.freshness() != DelliciusQuery::Subquery::REQUIRED) {
        continue;
      }

      auto find_query_params = redpath_to_query_params.find(redpath_prefix);
      if (find_query_params != redpath_to_query_params.end()) {
        find_query_params->second.freshness = GetParams::Freshness::kRequired;
      } else {
        redpath_to_query_params.insert(
            {redpath_prefix,
             GetParams{.freshness = GetParams::Freshness::kRequired}});
      }
    }
  }

  // Now we adjust freshness configuration such that if a RedPath expression
  // has a freshness requirement but is in the expand path of parent RedPath
  // the freshness requirement bubbles up to the parent RedPath
  // Example: /Chassis[*]/Sensors, $expand=*($levels=1) will assume the
  // freshness setting for path /Chassis[*]/Sensors[*].
  absl::string_view last_redpath_with_expand;
  GetParams *last_params = nullptr;
  absl::btree_map<std::string, GetParams> redpaths_to_query_params_ordered{
      redpath_to_query_params.begin(), redpath_to_query_params.end()};
  for (auto &[redpath, params] : redpaths_to_query_params_ordered) {
    if (params.freshness == GetParams::Freshness::kRequired &&
        // Check if last RedPath is prefix of current RedPath.
        absl::StartsWith(redpath, last_redpath_with_expand) &&
        // Check whether last RedPath uses query parameters.
        last_params != nullptr &&
        // Check if the RedPath prefix has an expand configuration.
        last_params->expand.has_value() &&
        // Check if current RedPath is in expand path of last RedPath.
        IsInExpandPath(redpath, last_redpath_with_expand,
                       last_params->expand->levels()) &&
        // Make sure the last redpath expression is not already fetched fresh.
        last_params->freshness == GetParams::Freshness::kOptional) {
      last_params->freshness = GetParams::Freshness::kRequired;
    }

    if (params.expand.has_value() && params.expand->levels() > 0) {
      last_redpath_with_expand = redpath;
      last_params = &params;
    }
  }
  return {redpaths_to_query_params_ordered.begin(),
          redpaths_to_query_params_ordered.end()};
}

// Gets RedfishObject from given RedfishVariant.
// Issues fresh query if the object is served from cache and freshness is
// required.
absl::StatusOr< std::unique_ptr<RedfishObject>>
GetRedfishObjectWithFreshness(const GetParams &params, RedfishVariant &variant,
                              const std::optional<TraceInfo> &trace_info,
                              std::atomic<int64_t> *cache_miss,
                              bool is_query_execution_cancelled) {
  // Do not fetch the fresh payload if the query has been cancelled. Return a
  // cancelled error
  if (is_query_execution_cancelled) {
    return absl::CancelledError("Query has been cancelled.");
  }
  if (!variant.status().ok()) {
    return absl::FailedPreconditionError(absl::StrCat(
        "RedfishVariant object has status: ", variant.status().message()));
  }
  std::unique_ptr<RedfishObject> redfish_object = variant.AsObject();
  if (!redfish_object) {
    LOG(INFO) << "RedfishVariant object is null with OK status. "
              << "This is only acceptable in test cases using mockups.";
    return nullptr;
  }
  if (trace_info.has_value()) {
    LOG(INFO) << "Redfish Object Trace:" << "\nredpath_current: "
              << trace_info->redpath_prefix
              << "\nquery_string: " << trace_info->redpath_query
              << "\nquery_id: " << trace_info->query_id << '\n'
              << trace_info->redpath_node << "Query result:\n"
              << redfish_object->GetContentAsJson().dump(1);
  }
  if (params.freshness != GetParams::Freshness::kRequired) {
    return redfish_object;
  }
  absl::StatusOr<std::unique_ptr<RedfishObject>> refetch_obj =
      redfish_object->EnsureFreshPayload(
          {.timeout_manager = params.timeout_manager});
  // If the variant is not fresh, then we do an uncached get and hence we need
  // to increase cache miss counter; ow no-op.
  if (variant.IsFresh() == CacheState::kIsCached) {
    (*cache_miss) = (*cache_miss) + 1;
  }
  // EnsureFreshPayload may return NotFoundError, which applies to the URI, not
  // the resource. We convert it to InternalError to avoid ignoring it.
  if (!refetch_obj.ok()) {
    return absl::InternalError(refetch_obj.status().message());
  }
  return std::move(refetch_obj.value());
}

absl::StatusOr<bool> ExecutePredicateExpression(
    const PredicateOptions &predicate_options,
    QueryExecutionContext &current_execution_context,
    const std::unique_ptr<RedfishObject> &redfish_object) {
  absl::StatusOr<bool> predicate_rule_result =
      ApplyPredicateRule(redfish_object->GetContentAsJson(),
                         {.predicate = predicate_options.predicate,
                          .node_index = predicate_options.node_index,
                          .node_set_size = predicate_options.node_set_size});
  if (predicate_rule_result.ok()) {
    return predicate_rule_result.value();
  }
  absl::StatusOr<std::string> child_uri = GetChildUriFromIterable(
      current_execution_context, predicate_options.node_index);

  return absl::Status(
      predicate_rule_result.status().code(),
      absl::StrCat(
          "At resource URI: ",
          child_uri.ok() ? child_uri.value() : child_uri.status().message(),
          ": ", predicate_rule_result.status().message()));
}

// Populates the result with the subquery error status that occurs.
void PopulateSubqueryErrorStatus(const absl::Status &node_status,
                                 QueryExecutionContext &execution_context,
                                 const RedPathExpression &expression) {
  std::string failed_redpath = AddExpressionToRedPath(
      execution_context.redpath_prefix_tracker.last_redpath_prefix, expression);
  std::string error_message =
      absl::StrCat("Querying ",
                   (expression.type == RedPathExpression::Type::kPredicate)
                       ? absl::StrCat("predicate [", expression.expression, "]")
                       : absl::StrCat("node name ", expression.expression),
                   " from Redpath: ", failed_redpath,
                   " resulted in error: ", node_status.message());
  ErrorCode error_code = ecclesia::ErrorCode::ERROR_INTERNAL;
  absl::StatusCode code = node_status.code();
  if (code == absl::StatusCode::kUnavailable) {
    error_code = ecclesia::ErrorCode::ERROR_UNAVAILABLE;
  } else if (code == absl::StatusCode::kUnauthenticated) {
    error_code = ecclesia::ErrorCode::ERROR_UNAUTHENTICATED;
  } else if (code == absl::StatusCode::kDeadlineExceeded) {
    error_code = ecclesia::ErrorCode::ERROR_QUERY_TIMEOUT;
  } else if (code == absl::StatusCode::kCancelled) {
    error_code = ecclesia::ErrorCode::ERROR_CANCELLED;
  }
  execution_context.result.mutable_status()->add_errors(error_message);
  execution_context.result.mutable_status()->set_error_code(error_code);
}

// RAII object used to record stats for a query execution.
class QueryScopeStats {
 public:
  QueryScopeStats(QueryResult &result, const RedfishMetrics *metrics,
                  CacheStats *cache_stats)
      : result_(result),
        metrics_(metrics),
        cache_stats_(ABSL_DIE_IF_NULL(cache_stats)) {}
  ~QueryScopeStats() { Dump(); }

 private:
  // Updates the query result with query stats.
  void Dump() {
    if (metrics_ != nullptr && !metrics_->uri_to_metrics_map().empty() &&
        result_.IsInitialized()) {
      *result_.mutable_stats()->mutable_redfish_metrics() = *metrics_;
      uint64_t request_count = 0;
      for (const auto &uri_x_metric : metrics_->uri_to_metrics_map()) {
        for (const auto &[request_type, metadata] :
             uri_x_metric.second.request_type_to_metadata()) {
          request_count += metadata.request_count();
        }
      }
      result_.mutable_stats()->set_num_requests(
          static_cast<int64_t>(request_count));
    }
    result_.mutable_stats()->set_payload_size(
        static_cast<int64_t>(result_.ByteSizeLong()));
    result_.mutable_stats()->set_num_cache_misses(cache_stats_->cache_miss);
    result_.mutable_stats()->set_num_cache_hits(cache_stats_->cache_hit);
    cache_stats_->Reset();
    MetricalRedfishTransport::ResetMetrics();
  }

  QueryResult &result_;
  const RedfishMetrics *metrics_;
  CacheStats *cache_stats_;
};

absl::StatusOr<nlohmann::json> GetResponseJsonFromContext(
    const QueryExecutionContext &execution_context) {
  const auto &rf_object = execution_context.redfish_response.redfish_object;
  if (rf_object == nullptr) {
    return absl::InternalError("(RedfishObject is null)");
  }
  nlohmann::json response = rf_object->GetContentAsJson();
  if (response.is_discarded()) {
    return absl::InternalError(
        "(Cannot get content as Json from Parent RedfishObject)");
  }
  if (!response.contains(PropertyOdataId::Name)) {
    return absl::InternalError(
        "(Parent URI Not Available from RedfishObject: No @odata.id)");
  }
  return response;
}

}  // namespace

absl::StatusOr<std::string> GetChildUriFromNode(
    const QueryExecutionContext &execution_context,
    const std::string &node_name) {
  ECCLESIA_ASSIGN_OR_RETURN(const nlohmann::json response,
                            GetResponseJsonFromContext(execution_context));
  std::string parent_uri = response[PropertyOdataId::Name];

  if (!response.contains(node_name)) {
    return absl::InternalError(
        absl::StrCat("(RedfishObject does not contain: ", node_name,
                     " at URI: ", parent_uri, ")"));
  }
  if (!response[node_name].contains(PropertyOdataId::Name)) {
    return absl::InternalError(absl::StrCat(
        "(@odata.id missing from: ", node_name, " at URI: ", parent_uri, ")"));
  }
  return response[node_name][PropertyOdataId::Name];
}

absl::StatusOr<std::string> GetChildUriFromIterable(
    const QueryExecutionContext &execution_context, int index) {
  // For complex Redfish Objects, the navigational property may be absent.
  const auto &rf_iterable = execution_context.redfish_response.redfish_iterable;
  if (execution_context.redfish_response.redfish_object == nullptr &&
      rf_iterable != nullptr) {
    return absl::InternalError("(Queried resource is a ComplexType; no URI)");
  }
  ECCLESIA_ASSIGN_OR_RETURN(const nlohmann::json response,
                            GetResponseJsonFromContext(execution_context));
  std::string parent_uri = response[PropertyOdataId::Name];

  if (!response.contains(PropertyMembers::Name)) {
    return absl::InternalError(
        absl::StrCat("(RedfishObject is not an iterable: ", parent_uri,
                     " does not have Members[])"));
  }
  if (index >= rf_iterable->Size() || index < 0) {
    return absl::InternalError(
        absl::StrCat("(Index ", index, " out of bounds ",
                     "for RedfishIterable at URI: ", parent_uri, ")"));
  }
  if (!response[PropertyMembers::Name][index].contains(PropertyOdataId::Name)) {
    return absl::InternalError(
        absl::StrCat("(@odata.id Not Available in Members[] at index ", index,
                     " at URI: ", parent_uri, ")"));
  }
  return response[PropertyMembers::Name][index][PropertyOdataId::Name];
}

QueryExecutionContext QueryExecutionContext::FromExisting(
    const std::string &new_redpath_prefix,
    const GetParams &get_params_for_redpath,
    RedfishResponse redfish_response_in, bool is_query_cancelled) {
  if (redpath_query_tracker != nullptr) {
    redpath_query_tracker->executed_redpath_prefixes_and_params.insert(
        {new_redpath_prefix, get_params_for_redpath});
  }

  RedPathPrefixTracker new_redpath_prefix_tracker(redpath_prefix_tracker);
  new_redpath_prefix_tracker.last_redpath_prefix = new_redpath_prefix;

  if (is_query_cancelled &&
      result.status().error_code() != ecclesia::ErrorCode::ERROR_CANCELLED) {
    result.mutable_status()->add_errors("Query execution cancelled.");
    result.mutable_status()->set_error_code(
        ecclesia::ErrorCode::ERROR_CANCELLED);
  }
  return QueryExecutionContext(
      &result, subquery_id_to_subquery_result, &query_variables,
      std::move(new_redpath_prefix_tracker), redpath_query_tracker,
      std::move(redfish_response_in));
}

QueryPlanner::QueryPlanner(ImplOptions options_in)
    : query_(*ABSL_DIE_IF_NULL(options_in.query)),
      plan_id_(options_in.query->query_id()),
      normalizer_(*ABSL_DIE_IF_NULL(options_in.normalizer)),
      additional_normalizers_(options_in.additional_normalizers),
      redpath_trie_root_(std::move(options_in.redpath_trie_node)),
      redpath_rules_(std::move(options_in.redpath_rules)),
      redfish_interface_(options_in.redfish_interface),
      service_root_(query_.has_service_root()
                        ? query_.service_root()
                        : std::string(kDefaultRedfishServiceRoot)),
      subquery_associations_(query_),
      clock_(options_in.clock == nullptr ? ecclesia::Clock::RealClock()
                                         : options_in.clock),
      timeout_manager_(options_in.query_timeout.has_value()
                           ? std::make_unique<QueryTimeoutManager>(
                                 *clock_, *options_in.query_timeout)
                           : nullptr),
      execution_mode_(options_in.execution_mode) {}

GetParams QueryPlanner::GetQueryParamsForRedPath(
    absl::string_view redpath_prefix) {
  // Set default GetParams value as follows:
  //   Default freshness is Optional
  //   Default Redfish query parameter setting is no expand!
  auto params = GetParams{.freshness = GetParams::Freshness::kOptional,
                          .expand = std::nullopt,
                          .filter = std::nullopt};
  // Get RedPath specific configuration for expand and freshness
  if (auto iter = redpath_rules_.redpath_to_query_params.find(redpath_prefix);
      iter != redpath_rules_.redpath_to_query_params.end()) {
    params = iter->second;
  }
  // Set timeout manager if it is available.
  if (timeout_manager_ != nullptr) {
    params.timeout_manager = timeout_manager_.get();
  }
  return params;
}

// Tries to normalize the Redfish object based on the given subquery ID.
//
// This function retrieves the subquery definition from the
// `subquery_id_to_subquery_` map. If the subquery requires fetching raw data,
// it extracts the raw data from the `query_execution_context`. Otherwise, it
// uses the `normalizer_` to normalize the Redfish object based on the subquery
// definition and stores the result in the `query_execution_context`.
//
// If the subquery has a root subquery ID, the normalized result is nested under
// the root subquery result in the `query_execution_context`.
absl::Status QueryPlanner::TryNormalize(
    absl::string_view subquery_id,
    QueryExecutionContext *query_execution_context,
    const RedpathNormalizerOptions &normalizer_options) const {
  auto find_subquery =
      subquery_associations_.subquery_id_to_subquery.find(subquery_id);
  if (find_subquery == subquery_associations_.subquery_id_to_subquery.end()) {
    return absl::NotFoundError(
        absl::StrCat("Subquery for subquery id ", subquery_id,
                     " not found in query execution context."));
  }

  QueryValue normalized_query_value;
  // When we are required to populate raw data like cper binary.
  if (find_subquery->second.has_fetch_raw_data() &&
      query_execution_context->redfish_response.redfish_raw_bytes != nullptr) {
    const auto &fetch_raw_data = find_subquery->second.fetch_raw_data();
    std::string raw_str(
        query_execution_context->redfish_response.redfish_raw_bytes->begin(),
        query_execution_context->redfish_response.redfish_raw_bytes->end());
    if (fetch_raw_data.type() == DelliciusQuery::Subquery::RawData::BYTES) {
      normalized_query_value.mutable_raw_data()->set_raw_bytes_value(raw_str);
    } else if (fetch_raw_data.type() ==
               DelliciusQuery::Subquery::RawData::STRING) {
      // Encode bytes to a printable format.
      normalized_query_value.mutable_raw_data()->set_raw_string_value(
          absl::Base64Escape(raw_str));
    }
  } else {
    if (query_execution_context->redfish_response.redfish_object == nullptr) {
      return absl::InternalError(absl::StrCat(
          "Redfish object is null in query execution context: ",
          query_execution_context->redpath_prefix_tracker.last_redpath_prefix));
    }
    absl::StatusOr<QueryResultData> normalized_query_result =
        normalizer_.Normalize(
            *query_execution_context->redfish_response.redfish_object,
            find_subquery->second, normalizer_options);
    if (!normalized_query_result.ok()) {
      if (normalized_query_result.status().code() !=
          absl::StatusCode::kNotFound) {
        return normalized_query_result.status();
      }

      // There are no properties to normalize. If the subquery doesn't have
      // any properties to query or the subquery is not a root subquery, there
      // no reason to create an empty subquery result.
      // But Not finding a property is not an error either, so in this case
      // we just return with ok status without data.
      if (!find_subquery->second.properties().empty() ||
          !subquery_associations_.root_id_to_subquery_ids.contains(
              subquery_id)) {
        // Not finding a property is not a halting error.
        DLOG(INFO) << "Cannot find queried properties in Redfish Object.\n"
                   << "===Redfish Object===\n"
                   << query_execution_context->redfish_response.redfish_object
                          ->GetContentAsJson()
                          .dump(1)
                   << "\n===Subquery===\n"
                   << find_subquery->second.DebugString()
                   << "\nError: " << normalized_query_result.status();
        return absl::OkStatus();
      }

      // If the subquery is a root subquery, we need to create an empty subquery
      // result to allow child subqueries to be executed.
      normalized_query_result = QueryResultData();
    }

    // Add an empty subquery value to allow child subqueries to be executed.
    *normalized_query_value.mutable_subquery_value() =
        std::move(*normalized_query_result);
  }

  // If the current subquery has one or more root subquery ids, we will find
  // the result of the root subquery so that we can nest the result of the
  // current subquery in it.
  QueryValue *root_subquery_result = nullptr;
  auto subquery_id_to_root_iter =
      subquery_associations_.subquery_id_to_root_ids.find(subquery_id);
  if (subquery_id_to_root_iter !=
      subquery_associations_.subquery_id_to_root_ids.end()) {
    for (const std::string &root_id : subquery_id_to_root_iter->second) {
      auto find_parent_subquery_output =
          query_execution_context->subquery_id_to_subquery_result.find(root_id);
      if (find_parent_subquery_output ==
          query_execution_context->subquery_id_to_subquery_result.end()) {
        continue;
      }
      root_subquery_result = find_parent_subquery_output->second;
      break;
    }
  }

  if (subquery_id_to_root_iter ==
          subquery_associations_.subquery_id_to_root_ids.end() ||
      root_subquery_result == nullptr) {
    // There isn't a root subquery associated with current subquery or the
    // result of root subquery is not found.
    // This is typically the case when we are normalizing results for a root
    // subquery itself or dealing with a resume query operation on receiving
    // redfish event.
    auto [subquery_output, _] =
        query_execution_context->result.mutable_data()
            ->mutable_fields()
            ->insert({std::string(subquery_id), QueryValue()});
    QueryValue *subquery_value =
        subquery_output->second.mutable_list_value()->add_values();

    *subquery_value = std::move(normalized_query_value);
    query_execution_context->subquery_id_to_subquery_result[subquery_id] =
        subquery_value;
    return absl::OkStatus();
  }

  auto *values_in_root_subquery_result =
      root_subquery_result->mutable_subquery_value()->mutable_fields();

  // Add current subquery result to parent subquery result.
  auto subquery_value_iter = values_in_root_subquery_result->find(subquery_id);
  if (subquery_value_iter != values_in_root_subquery_result->end()) {
    QueryValue *new_query_result_data =
        subquery_value_iter->second.mutable_list_value()->add_values();

    *new_query_result_data = std::move(normalized_query_value);
    query_execution_context->subquery_id_to_subquery_result[subquery_id] =
        new_query_result_data;
    return absl::OkStatus();
  }

  // When the root subquery result does not have a subquery result linked with
  // current subquery id, we create a new subquery output in root subquery
  // result with current subquery id.
  QueryValue *subquery_value = &(*values_in_root_subquery_result)[subquery_id];
  QueryValue *new_query_result = subquery_value;
  // When we are dealing with raw data, we don't expect multiple values in
  // subquery. So add a list of values only when the query result doesn't have
  // raw data.
  if (!normalized_query_value.has_raw_data()) {
    new_query_result = subquery_value->mutable_list_value()->add_values();
  }
  *new_query_result = std::move(normalized_query_value);
  query_execution_context->subquery_id_to_subquery_result[subquery_id] =
      new_query_result;
  return absl::OkStatus();
}

void QueryPlanner::TryNormalizeOnFinalQueryResult(
    ecclesia::QueryResult &result,
    const RedpathNormalizerOptions &normalizer_options) {
  for (RedpathNormalizer *additional_normalizer : additional_normalizers_) {
    absl::Status normalize_status = additional_normalizer->Normalize(
        *result.mutable_data(), normalizer_options);
    if (!normalize_status.ok()) {
      result.mutable_status()->add_errors(
          absl::StrCat("Unable to normalize with additional normalizers: ",
                       normalize_status.message()));
      result.mutable_status()->set_error_code(
          ecclesia::ErrorCode::ERROR_INTERNAL);
      return;
    }
  }
}

absl::StatusOr<std::vector<QueryExecutionContext>>
QueryPlanner::ExecuteQueryExpression(
    QueryType query_type, const RedPathExpression &expression,
    QueryExecutionContext &current_execution_context,
    std::optional<TraceInfo> &trace_info,
    RedfishInterface *redfish_interface_inject) {
  if (redfish_interface_inject == nullptr && redfish_interface_ == nullptr) {
    return absl::InternalError("Redfish interface is null.");
  }

  RedfishInterface *redfish_interface = redfish_interface_inject == nullptr
                                            ? redfish_interface_
                                            : redfish_interface_inject;

  std::vector<QueryExecutionContext> execution_contexts;

  const std::unique_ptr<RedfishObject> &current_redfish_obj =
      current_execution_context.redfish_response.redfish_object;
  const std::unique_ptr<RedfishIterable> &current_redfish_iterable =
      current_execution_context.redfish_response.redfish_iterable;

  // Construct RedPath prefix to lookup associated query parameters
  RedPathPrefixTracker &redpath_prefix_tracker =
      current_execution_context.redpath_prefix_tracker;
  std::string new_redpath_prefix = AddExpressionToRedPath(
      redpath_prefix_tracker.last_redpath_prefix, expression);
  // Get query parameters for the RedPath expression we are about to
  // execute.
  GetParams get_params_for_redpath =
      GetQueryParamsForRedPath(new_redpath_prefix);

  // Run query expression relative to Iterable resource
  if (current_redfish_iterable &&
      expression.type == RedPathExpression::Type::kPredicate) {
    // Substitute variables in the predicate expression.
    ECCLESIA_ASSIGN_OR_RETURN(
        std::string new_predicate,
        SubstituteVariables(expression.expression,
                            current_execution_context.query_variables));

    // Iterate over resources in collection.
    size_t node_count = current_redfish_iterable->Size();
    for (int node_index = 0; node_index < node_count; ++node_index) {
      RedfishVariant indexed_node = (*current_redfish_iterable)[node_index];
      if (indexed_node.IsFresh() == CacheState::kIsFresh) {
        cache_stats_.cache_miss += 1;
      } else if (indexed_node.IsFresh() == CacheState::kIsCached) {
        cache_stats_.cache_hit += 1;
      }

      TRACE(trace_info, new_redpath_prefix, get_params_for_redpath.ToString(),
            expression.trie_node->ToString());

      // Get fresh Redfish Object if user has requested in the query rule.
      absl::StatusOr<std::unique_ptr<RedfishObject>> indexed_node_as_object =
          GetRedfishObjectWithFreshness(get_params_for_redpath, indexed_node,
                                        trace_info, &cache_stats_.cache_miss,
                                        IsQueryExecutionCancelled());
      if (!indexed_node.status().ok() || !indexed_node_as_object.ok()) {
        // If a query has been cancelled, add a new execution context with a
        // cancelled error for all the remaining indexed nodes.
        if (indexed_node_as_object.status().code() ==
            absl::StatusCode::kCancelled) {
          execution_contexts.push_back(current_execution_context.FromExisting(
              new_redpath_prefix, get_params_for_redpath, {nullptr, nullptr},
              true));
          continue;
        }

        absl::Status error_status = indexed_node.status().ok()
                                        ? indexed_node_as_object.status()
                                        : indexed_node.status();
        absl::StatusOr<std::string> child_uri =
            GetChildUriFromIterable(current_execution_context, node_index);

        error_status = absl::Status(
            error_status.code(),
            absl::StrCat("At resource URI: ",
                         child_uri.ok() ? child_uri.value()
                                        : child_uri.status().message(),
                         ": ", error_status.message()));
        if (execution_mode_ == QueryPlanner::ExecutionMode::kFailOnFirstError) {
          return error_status;
        }
        if (error_status.code() != absl::StatusCode::kNotFound) {
          PopulateSubqueryErrorStatus(error_status, current_execution_context,
                                      expression);
        }
        continue;
      }

      if (*indexed_node_as_object == nullptr) {
        continue;
      }

      // We don't create new execution context when a subscription is required.
      // Instead, we store each URI of the resource collection in the given
      // execution context which will then be used to create event subscription.
      if (query_type == QueryType::kSubscription &&
          redpath_rules_.redpaths_to_subscribe.contains(new_redpath_prefix)) {
        std::optional<std::string> odata_id =
            (*indexed_node_as_object)->GetNodeValue<PropertyOdataId>();
        if (odata_id.has_value()) {
          current_execution_context.uris_to_subscribe.push_back(
              odata_id.value());
        }
        continue;
      }
      ECCLESIA_ASSIGN_OR_RETURN(
          bool predicate_rule_result,
          ExecutePredicateExpression({.predicate = new_predicate,
                                      .node_index = node_index,
                                      .node_set_size = node_count},
                                     current_execution_context,
                                     *indexed_node_as_object));

      if (predicate_rule_result) {
        execution_contexts.push_back(current_execution_context.FromExisting(
            new_redpath_prefix, get_params_for_redpath,
            {std::move(*indexed_node_as_object), nullptr}));
      }
    }
  } else if (current_redfish_obj) {
    // Halt execution and capture URI if RedPath expression requires
    // event subscription.
    if (query_type == QueryType::kSubscription &&
        redpath_rules_.redpaths_to_subscribe.contains(new_redpath_prefix)) {
      absl::StatusOr<std::string> uri_to_subscribe =
          GetNavigationalPropertyToSubscribe(current_redfish_obj, expression,
                                             get_params_for_redpath);
      if (uri_to_subscribe.ok()) {
        current_execution_context.uris_to_subscribe.push_back(
            uri_to_subscribe.value());
      }
      return execution_contexts;
    }

    // If the query has been cancelled, return a cancelled error.
    // Do not fetch the fresh payload if the query has been cancelled.
    if (IsQueryExecutionCancelled()) {
      return absl::CancelledError("Query execution cancelled for NodeName: " +
                                  expression.expression);
    }
    RedfishVariant redfish_variant(absl::OkStatus());
    if (expression.type == RedPathExpression::Type::kNodeName) {
      if (get_params_for_redpath.filter.has_value()) {
        // Since filter is enabled all predicates that rely on the redfish data
        // returned from this call need to be added to the $filter parameter
        // that is sent to the Redfish agent.
        absl::StatusOr<std::string> filter_string = GetFilterStringFromNextNode(
            expression.trie_node, current_execution_context);
        if (filter_string.ok()) {
          get_params_for_redpath.filter->SetFilterString(filter_string.value());
        }
      }
      redfish_variant = current_redfish_obj->Get(expression.expression,
                                                 get_params_for_redpath);
    } else if (expression.type ==
               RedPathExpression::Type::kNodeNameJsonPointer) {
      // resolve the nest node represented in the normalized_node_name
      absl::StatusOr<nlohmann::json> json_obj = ResolveRedPathNodeToJson(
          current_redfish_obj->GetContentAsJson(), expression.expression);
      if (!json_obj.ok()) {
        return execution_contexts;
      }
      std::string node_name = json_obj->get<std::string>();
      get_params_for_redpath = GetQueryParamsForRedPath(node_name);
      redfish_variant =
          redfish_interface->CachedGetUri(node_name, get_params_for_redpath);
    } else if (expression.type ==
               RedPathExpression::Type::kNodeNameUriPointer) {
      get_params_for_redpath = GetQueryParamsForRedPath(expression.expression);
      redfish_variant = redfish_interface->CachedGetUri(expression.expression,
                                                        get_params_for_redpath);
    } else if (expression.type == RedPathExpression::Type::kPredicate) {
      // We allow executing predicate on single redfish object.
      ECCLESIA_ASSIGN_OR_RETURN(
          bool predicate_rule_result,
          ExecutePredicateExpression({.predicate = expression.expression,
                                      .node_index = 0,
                                      .node_set_size = 1},
                                     current_execution_context,
                                     current_redfish_obj));
      ECCLESIA_ASSIGN_OR_RETURN(
          std::unique_ptr<RedfishObject> redfish_object,
          current_redfish_obj->EnsureFreshPayload(get_params_for_redpath));
      if (!predicate_rule_result || !redfish_object) {
        // It is not an error to execute predicate and not successfully filter
        // redfish object.
        return execution_contexts;
      }
      TRACE(trace_info, new_redpath_prefix, get_params_for_redpath.ToString(),
            expression.trie_node->ToString());
      execution_contexts.push_back(current_execution_context.FromExisting(
          new_redpath_prefix, get_params_for_redpath,
          {std::move(redfish_object), nullptr}));
      return execution_contexts;
    }

    if (redfish_variant.IsFresh() == CacheState::kIsFresh) {
      cache_stats_.cache_miss += 1;
    } else if (redfish_variant.IsFresh() == CacheState::kIsCached) {
      cache_stats_.cache_hit += 1;
    }
    // If a timeout occurred, it will be reported here.
    if (!redfish_variant.status().ok()) {
      absl::StatusOr<std::string> child_uri =
          GetChildUriFromNode(current_execution_context, expression.expression);

      return absl::Status(
          redfish_variant.status().code(),
          absl::StrCat(
              "At resource URI: ",
              child_uri.ok() ? child_uri.value() : child_uri.status().message(),
              ": ", redfish_variant.status().message()));
    }
    // Set timeout manager if it is available for the Variant so that requests
    // triggered by the variant can count against the query level timeout.
    if (timeout_manager_ != nullptr) {
      redfish_variant.SetTimeoutManager(timeout_manager_.get());
    }

    RedfishResponse redfish_response;
    redfish_response.redfish_iterable = redfish_variant.AsIterable();
    std::optional<RedfishTransport::bytes> raw_bytes = redfish_variant.AsRaw();
    if (raw_bytes.has_value()) {
      redfish_response.redfish_raw_bytes =
          std::make_unique<RedfishTransport::bytes>(
              std::move(raw_bytes.value()));
    }
    redfish_response.redfish_object = redfish_variant.AsObject();
    if (!redfish_response.redfish_iterable &&
        !redfish_response.redfish_raw_bytes) {
      if (!redfish_response.redfish_object) {
        DLOG(INFO) << "Cannot query NodeName " << expression.expression
                   << " in Redfish Object:\n"
                   << redfish_variant.DebugString()
                   << "\nStatus: " << redfish_variant.status();
        return execution_contexts;
      }
      TRACE(trace_info, new_redpath_prefix, get_params_for_redpath.ToString(),
            expression.trie_node->ToString());
      // Get fresh Redfish Object if user has requested in the query rule.
      absl::StatusOr<std::unique_ptr<RedfishObject>> rf_object_or_status =
          GetRedfishObjectWithFreshness(get_params_for_redpath, redfish_variant,
                                        trace_info, &cache_stats_.cache_miss,
                                        IsQueryExecutionCancelled());
      if (!rf_object_or_status.ok()) {
        absl::StatusOr<std::string> child_uri = GetChildUriFromNode(
            current_execution_context, expression.expression);

        return absl::Status(
            rf_object_or_status.status().code(),
            absl::StrCat("At resource URI: ",
                         child_uri.ok() ? child_uri.value()
                                        : child_uri.status().message(),
                         ": ", rf_object_or_status.status().message()));
      }
      redfish_response.redfish_object = std::move(rf_object_or_status.value());
    }
    execution_contexts.push_back(current_execution_context.FromExisting(
        new_redpath_prefix, get_params_for_redpath,
        std::move(redfish_response)));
  }
  return execution_contexts;
}

QueryResult QueryPlanner::Resume(QueryResumeOptions query_resume_options) {
  const RedfishVariant &redfish_variant = query_resume_options.redfish_variant;
  std::unique_ptr<RedfishObject> redfish_object = redfish_variant.AsObject();
  std::unique_ptr<RedfishIterable> redfish_iterable =
      redfish_variant.AsIterable();

  QueryResult result;
  result.set_query_id(plan_id_);

  // Initialize query execution context to execute next RedPath
  // expression.
  QueryExecutionContext execution_context(
      &result, {}, &query_resume_options.variables, RedPathPrefixTracker(),
      query_resume_options.redpath_query_tracker,
      {std::move(redfish_object), std::move(redfish_iterable)});
  execution_context.redpath_trie_node = query_resume_options.trie_node;

  std::optional<TraceInfo> trace_info = std::nullopt;
  if (query_resume_options.log_redfish_traces) {
    trace_info = {
        .query_id = plan_id_,
    };
  }

  // Populate subquery data using current node before processing next
  // expression in the trie.
  if (!execution_context.redpath_trie_node->subquery_id.empty() &&
      execution_context.redfish_response.redfish_object != nullptr) {
    absl::Status normalize_status = TryNormalize(
        execution_context.redpath_trie_node->subquery_id, &execution_context,
        {.enable_url_annotation = query_resume_options.enable_url_annotation});
    if (!normalize_status.ok()) {
      result.mutable_status()->add_errors(absl::StrCat(
          "Unable to normalize resumed query: ", normalize_status.message()));
      result.mutable_status()->set_error_code(
          ecclesia::ErrorCode::ERROR_INTERNAL);
      return result;
    }
  }

  if (query_resume_options.redfish_interface == nullptr &&
      redfish_interface_ == nullptr) {
    const absl::flat_hash_set<RedPathExpression> &expressions =
        execution_context.redpath_trie_node->child_expressions;
    if (expressions.empty()) {
      result.mutable_status()->add_errors(
          "Redfish interface is not set when "
          "resuming with no remaining expressions to query");
    } else {
      result.mutable_status()->add_errors(absl::StrCat(
          "Redfish interface is not set before querying ",
          expressions.begin()->type == RedPathExpression::Type::kPredicate
              ? absl::StrCat("predicate [", expressions.begin()->expression,
                             "]")
              : absl::StrCat("node ", expressions.begin()->expression)));
    }
    result.mutable_status()->set_error_code(
        ecclesia::ErrorCode::ERROR_INTERNAL);
    return result;
  }

  RedfishInterface *local_redfish_interface =
      query_resume_options.redfish_interface == nullptr
          ? redfish_interface_
          : query_resume_options.redfish_interface;

  // Begin BFS traversal of the trie.
  std::queue<QueryExecutionContext> node_queue;
  node_queue.push(std::move(execution_context));
  while (!node_queue.empty()) {
    QueryExecutionContext current_execution_context =
        std::move(node_queue.front());
    node_queue.pop();
    const RedPathTrieNode &current_redpath_trie_node =
        *current_execution_context.redpath_trie_node;
    for (const auto &expression : current_redpath_trie_node.child_expressions) {
      absl::StatusOr<std::vector<QueryExecutionContext>> execution_contexts =
          ExecuteQueryExpression(QueryType::kPolling, expression,
                                 current_execution_context, trace_info,
                                 local_redfish_interface);
      if (!execution_contexts.ok()) {
        if (execution_contexts.status().code() != absl::StatusCode::kNotFound) {
          PopulateSubqueryErrorStatus(execution_contexts.status(),
                                      current_execution_context, expression);
        }
        continue;
      }
      // Get subquery id from next trie node. A subquery id would exist only
      // if the node marks the end of a RedPath expression else it would be
      // empty.
      RedPathTrieNode *trie_node = expression.trie_node;
      absl::string_view subquery_id = trie_node->subquery_id;
      for (auto &new_execution_context : *execution_contexts) {
        // Populate subquery data before processing next expression
        const std::unique_ptr<RedfishObject> &object =
            new_execution_context.redfish_response.redfish_object;
        if (!subquery_id.empty() && object != nullptr) {
          absl::Status normalize_status =
              TryNormalize(subquery_id, &new_execution_context,
                           {.enable_url_annotation =
                                query_resume_options.enable_url_annotation});
          if (normalize_status.ok()) {
            continue;
          }
          result.mutable_status()->add_errors(
              absl::StrCat("Unable to normalize: ", normalize_status.message(),
                           " for subquery: ", subquery_id));
          result.mutable_status()->set_error_code(
              ecclesia::ErrorCode::ERROR_INTERNAL);
          return result;
        }
        new_execution_context.redpath_trie_node = trie_node;
        node_queue.push(std::move(new_execution_context));
      }
    }
  }
  // Final normalization with additional normalizers, running against the final
  // query result.
  TryNormalizeOnFinalQueryResult(
      result,
      {.enable_url_annotation = query_resume_options.enable_url_annotation});
  return result;
}

void QueryPlanner::PopulateSubscriptionContext(
    const std::vector<QueryExecutionContext> &execution_contexts,
    QueryExecutionContext &current_execution_context,
    const RedPathExpression &expression,
    const QueryPlannerIntf::QueryExecutionOptions &query_execution_options,
    std::unique_ptr<QueryPlannerIntf::SubscriptionContext>
        &subscription_context) {
  // If there are execution contexts, then query is supposed to continue.
  // We should not create a subscription in that case.
  if (!execution_contexts.empty() ||
      current_execution_context.uris_to_subscribe.empty()) {
    return;
  }

  // Populate subscription context.
  if (subscription_context == nullptr) {
    subscription_context =
        std::make_unique<QueryPlannerIntf::SubscriptionContext>();
  }
  RedPathSubscription::Configuration subscription;
  std::string redpath_subscribed = AddExpressionToRedPath(
      current_execution_context.redpath_prefix_tracker.last_redpath_prefix,
      expression);
  subscription.redpath = redpath_subscribed;
  subscription.query_id = plan_id_;
  // Populate uris.
  subscription.uris = {current_execution_context.uris_to_subscribe.begin(),
                       current_execution_context.uris_to_subscribe.end()};
  current_execution_context.uris_to_subscribe.clear();

  if (expression.type == RedPathExpression::Type::kPredicate) {
    subscription.predicate = expression.expression;
  }

  subscription_context->subscription_configs.push_back(std::move(subscription));
  subscription_context->redpath_to_trie_node.insert(
      {redpath_subscribed, expression.trie_node});
  subscription_context->query_variables = query_execution_options.variables;
  subscription_context->log_redfish_traces =
      query_execution_options.log_redfish_traces;
}

QueryPlanner::QueryExecutionResult QueryPlanner::Run(
    QueryExecutionOptions query_execution_options) {
  QueryExecutionResult query_execution_result;
  QueryResult &result = query_execution_result.query_result;
  result.set_query_id(plan_id_);
  QueryScopeStats scoped_stats(
      result, MetricalRedfishTransport::GetConstMetrics(), &cache_stats_);
  if (timeout_manager_ != nullptr && !timeout_manager_->StartTiming().ok()) {
    result.mutable_status()->add_errors(
        "Timed out before query execution could start");
    result.mutable_status()->set_error_code(
        ecclesia::ErrorCode::ERROR_QUERY_TIMEOUT);
    return query_execution_result;
  }

  if (query_execution_options.redfish_interface == nullptr &&
      redfish_interface_ == nullptr) {
    result.mutable_status()->add_errors(
        "Redfish interface is not set before query execution");
    result.mutable_status()->set_error_code(
        ecclesia::ErrorCode::ERROR_INTERNAL);
    return query_execution_result;
  }

  RedfishInterface *local_redfish_interface =
      query_execution_options.redfish_interface == nullptr
          ? redfish_interface_
          : query_execution_options.redfish_interface;

  // If query execution has been cancelled, do not query service root.
  if (IsQueryExecutionCancelled()) {
    result.mutable_status()->add_errors(
        "Query execution cancelled before querying service root.");
    result.mutable_status()->set_error_code(
        ecclesia::ErrorCode::ERROR_CANCELLED);
    return query_execution_result;
  }

  // Query service root.
  // Get Query Parameters to use for service root
  GetParams get_params_service_root =
      GetQueryParamsForRedPath(kServiceRootNode);
  RedfishVariant variant(absl::OkStatus());
  if (!query_execution_options.custom_service_root.empty()) {
    variant = local_redfish_interface->GetRoot(
        get_params_service_root, query_execution_options.custom_service_root);
  } else {
    variant = local_redfish_interface->GetRoot(get_params_service_root,
                                               service_root_);
  }
  if (variant.IsFresh() == CacheState::kIsFresh) {
    cache_stats_.cache_miss += 1;
  } else if (variant.IsFresh() == CacheState::kIsCached) {
    cache_stats_.cache_hit += 1;
  }
  if (variant.status().code() == absl::StatusCode::kDeadlineExceeded) {
    result.mutable_status()->add_errors(
        "Timed out while querying service root");
    result.mutable_status()->set_error_code(
        ecclesia::ErrorCode::ERROR_QUERY_TIMEOUT);
    return query_execution_result;
  }

  absl::StatusOr<std::unique_ptr<RedfishObject>> service_root_object =
      GetRedfishObjectWithFreshness(get_params_service_root, variant,
                                    std::nullopt, &cache_stats_.cache_miss,
                                    IsQueryExecutionCancelled());

  // If service root is unreachable populate the error and return the result.
  if (!service_root_object.ok() || *service_root_object == nullptr) {
    result.mutable_status()->add_errors(absl::StrCat(
        "Attempting to reach service root ", get_params_service_root.uri_prefix,
        service_root_, " resulted in error: ",
        service_root_object.ok()
            ? "Getting object with freshness returned nullptr"
            : service_root_object.status().message()));
    result.mutable_status()->set_error_code(
        service_root_object.status().code() ==
                absl::StatusCode::kDeadlineExceeded
            ? ecclesia::ErrorCode::ERROR_QUERY_TIMEOUT
            : (service_root_object.status().code() ==
                       absl::StatusCode::kCancelled
                   ? ecclesia::ErrorCode::ERROR_CANCELLED
                   : ecclesia::ErrorCode::ERROR_SERVICE_ROOT_UNREACHABLE));
    return query_execution_result;
  }
  // Initialize query execution context to execute next RedPath expression.
  QueryExecutionContext query_execution_context(
      &result, {}, &query_execution_options.variables, RedPathPrefixTracker(),
      query_execution_options.redpath_query_tracker,
      {std::move(*service_root_object), nullptr});
  query_execution_context.redpath_trie_node = redpath_trie_root_.get();

  // Populate subquery data from root node.
  if (!query_execution_context.redpath_trie_node->subquery_id.empty()) {
    absl::Status normalize_status =
        TryNormalize(query_execution_context.redpath_trie_node->subquery_id,
                     &query_execution_context,
                     {.enable_url_annotation =
                          query_execution_options.enable_url_annotation});
    if (!normalize_status.ok()) {
      result.mutable_status()->add_errors(absl::StrCat(
          "Querying service root: ", get_params_service_root.uri_prefix,
          service_root_, " resulted in error: ", normalize_status.message()));
      result.mutable_status()->set_error_code(
          ecclesia::ErrorCode::ERROR_INTERNAL);
      return query_execution_result;
    }
  }

  std::queue<QueryExecutionContext> node_queue;
  node_queue.push(std::move(query_execution_context));

  std::optional<TraceInfo> trace_info = std::nullopt;
  if (query_execution_options.log_redfish_traces) {
    trace_info = {
        .query_id = plan_id_,
    };
  }

  // Create Subscription Context.
  // This context is used in asynchronous execution of RedPath query where
  // query execution blocks waiting for events and resumes from the previous
  // checkpoint based on the QueryPlanner state encapsulated in
  // `subscription_context`.
  std::unique_ptr<SubscriptionContext> subscription_context = nullptr;

  // Below BFS traversal of RedPath prefix tree follows the following pattern:
  //  ExecutionContext of the root node is used to execute the RedPath
  //  expressions of child nodes. In other words, the result of querying RedPath
  //  expression of root node forms the context node relative to which
  //  expressions in child nodes are executed.
  while (!node_queue.empty()) {
    QueryExecutionContext current_execution_context =
        std::move(node_queue.front());
    node_queue.pop();
    const RedPathTrieNode &current_redpath_trie_node =
        *current_execution_context.redpath_trie_node;

    for (const RedPathExpression &expression :
         current_redpath_trie_node.child_expressions) {
      // Get subquery id from next trie node. A subquery id would exist only
      // if the node marks the end of a RedPath expression else it would be
      // empty.
      absl::string_view subquery_id = expression.trie_node->subquery_id;

      absl::StatusOr<std::vector<QueryExecutionContext>> execution_contexts =
          ExecuteQueryExpression(query_execution_options.query_type, expression,
                                 current_execution_context, trace_info,
                                 local_redfish_interface);
      // Exit query execution and populate error if querying fails. If the error
      // is due to the resource not being found continue as this is allowed by
      // query engine.
      if (!execution_contexts.ok()) {
        if (execution_contexts.status().code() == absl::StatusCode::kNotFound) {
          continue;
        }
        PopulateSubqueryErrorStatus(execution_contexts.status(),
                                    current_execution_context, expression);

        if (execution_contexts.status().code() ==
            absl::StatusCode::kCancelled) {
          result.mutable_status()->add_errors(
              absl::StrCat("Halted query execution: ",
                           execution_contexts.status().message()));
          result.mutable_status()->set_error_code(
              ecclesia::ErrorCode::ERROR_CANCELLED);
          return query_execution_result;
        }

        if (execution_mode_ == ExecutionMode::kContinueOnSubqueryErrors) {
          continue;
        }

        // We are returning an error at this point, so end the timing session,
        // regardless of its status.
        if (timeout_manager_ != nullptr) {
          timeout_manager_->EndTiming().IgnoreError();
        }
        return query_execution_result;
      }

      // Handle subscription request.
      PopulateSubscriptionContext(
          *execution_contexts, current_execution_context, expression,
          query_execution_options, subscription_context);

      for (auto &execution_context : *execution_contexts) {
        // Populate subquery data before processing next expression
        if (!subquery_id.empty()) {
          absl::Status normalize_status =
              TryNormalize(subquery_id, &execution_context,
                           {.enable_url_annotation =
                                query_execution_options.enable_url_annotation});
          if (!normalize_status.ok()) {
            result.mutable_status()->add_errors(absl::StrCat(
                "Unable to normalize: ", normalize_status.message(),
                " for subquery: ", subquery_id));
            if (execution_context.result.has_status() &&
                execution_context.result.status().error_code() ==
                    ecclesia::ErrorCode::ERROR_CANCELLED) {
              result.mutable_status()->set_error_code(
                  execution_context.result.status().error_code());
              return query_execution_result;
            }
            result.mutable_status()->set_error_code(
                ecclesia::ErrorCode::ERROR_INTERNAL);
            return query_execution_result;
          }
        }
        execution_context.redpath_trie_node = expression.trie_node;
        node_queue.push(std::move(execution_context));
      }
    }
  }
  query_execution_result.subscription_context = std::move(subscription_context);

  // Final normalization with additional normalizers, running against the final
  // query result.
  TryNormalizeOnFinalQueryResult(
      result,
      {.enable_url_annotation = query_execution_options.enable_url_annotation});

  // Check if between the last RPC returning and now, the timeout has been
  // reached. If so, return a timeout error.
  if (timeout_manager_ != nullptr && !timeout_manager_->EndTiming().ok()) {
    result.mutable_status()->add_errors("Timed out while executing query");
    result.mutable_status()->set_error_code(
        ecclesia::ErrorCode::ERROR_QUERY_TIMEOUT);
  }
  return query_execution_result;
}

// Builds query plan for given query and returns QueryPlanner instance to the
// caller which can be used to execute the QueryPlan.
absl::StatusOr<std::unique_ptr<QueryPlannerIntf>> BuildQueryPlanner(
    QueryPlanner::ImplOptions query_planner_options) {
  RedPathTrieBuilder redpath_trie_builder(query_planner_options.query);
  ECCLESIA_ASSIGN_OR_RETURN(std::unique_ptr<RedPathTrieNode> redpath_trie,
                            redpath_trie_builder.CreateRedPathTrie());

  query_planner_options.redpath_trie_node = std::move(redpath_trie);
  query_planner_options.redpath_rules.redpath_to_query_params =
      CombineQueryParams(
          *query_planner_options.query,
          query_planner_options.redpath_rules.redpath_to_query_params,
          redpath_trie_builder.GetSubquerySequences());

  // Create QueryPlanner
  return std::make_unique<QueryPlanner>(std::move(query_planner_options));
}

}  // namespace ecclesia
