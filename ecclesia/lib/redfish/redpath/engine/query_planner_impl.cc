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

#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/die_if_null.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
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
#include "ecclesia/lib/redfish/transport/metrical_transport.h"
#include "ecclesia/lib/status/macros.h"
#include "ecclesia/lib/time/clock.h"
#include "ecclesia/lib/time/proto.h"
#include "single_include/nlohmann/json.hpp"
#include "re2/re2.h"

namespace ecclesia {

namespace {

using RedPathRedfishQueryParams =
    absl::flat_hash_map<std::string /* RedPath */, GetParams>;

// Pattern for location step: NodeName[Predicate]
constexpr LazyRE2 kLocationStepRegex = {
    "^([a-zA-Z#@][0-9a-zA-Z.]+|)(?:\\[(.*?)\\]|)$"};
constexpr absl::string_view kPredicateSelectAll = "*";

// All RedPath expressions execute relative to service root identified by '/'.
constexpr absl::string_view kServiceRootNode = "/";
constexpr absl::string_view kDefaultRedfishServiceRoot = "/redfish/v1";


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
    RedPathTrieNode *next_trie_node) {
  std::vector<std::string> predicates;
  for (const auto &[node_exp, node_ptr] :
       next_trie_node->expression_to_trie_node) {
    if (node_exp.type == RedPathExpression::Type::kPredicate) {
      // If some of the predicates are invalid for $filter, ie
      // "[*]" or "[Property]" the entire filter generation will fail, which is
      // intended behavior.
      predicates.push_back(node_exp.expression);
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
absl::StatusOr<std::unique_ptr<RedfishObject>> GetRedfishObjectWithFreshness(
    const GetParams &params, const RedfishVariant &variant,
    const std::optional<TraceInfo> &trace_info,
    std::atomic<int64_t> *cache_miss) {
  std::unique_ptr<RedfishObject> redfish_object = variant.AsObject();
  if (!redfish_object) return nullptr;
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
  if (!refetch_obj.ok()) return nullptr;
  return std::move(refetch_obj.value());
}

SubqueryIdToSubquery GetSubqueryIdToSubquery(const DelliciusQuery &query) {
  SubqueryIdToSubquery id_to_subquery;
  for (const auto &subquery : query.subquery()) {
    id_to_subquery[subquery.subquery_id()] = subquery;
  }
  return id_to_subquery;
}

absl::StatusOr<bool> ExecutePredicateExpression(
    const PredicateOptions &predicate_options,
    QueryExecutionContext &current_execution_context,
    const std::unique_ptr<RedfishObject> &redfish_object) {
  ECCLESIA_ASSIGN_OR_RETURN(
      std::string new_predicate,
      SubstituteVariables(predicate_options.predicate,
                          current_execution_context.query_variables));
  ECCLESIA_ASSIGN_OR_RETURN(
      bool predicate_rule_result,
      ApplyPredicateRule(redfish_object->GetContentAsJson(),
                         {.predicate = new_predicate,
                          .node_index = predicate_options.node_index,
                          .node_set_size = predicate_options.node_set_size}));

  return predicate_rule_result;
}

// Populates the result with the subquery error status that occurs.
void PopulateSubqueryErrorStatus(const absl::Status &node_status,
                                 QueryExecutionContext &execution_context,
                                 const RedPathExpression &expression) {
  ErrorCode error_code = ecclesia::ErrorCode::ERROR_INTERNAL;
  absl::StatusCode code = node_status.code();
  std::string node_name = expression.expression;
  std::string last_executed_redpath =
      execution_context.redpath_prefix_tracker.last_redpath_prefix;
  std::string error_message = absl::StrCat(
      "Cannot resolve NodeName ", node_name,
      " to valid Redfish object at path ", last_executed_redpath,
      ". Redfish Request failed with error: ", node_status.message());
  if (code == absl::StatusCode::kUnavailable) {
    error_code = ecclesia::ErrorCode::ERROR_UNAVAILABLE;
  }
  if (code == absl::StatusCode::kUnauthenticated) {
    error_code = ecclesia::ErrorCode::ERROR_UNAUTHENTICATED;
  }
  if (code == absl::StatusCode::kDeadlineExceeded) {
    error_code = ecclesia::ErrorCode::ERROR_QUERY_TIMEOUT;
  }
  execution_context.result.mutable_status()->add_errors(error_message);
  execution_context.result.mutable_status()->set_error_code(error_code);
}

}  // namespace

QueryPlanner::QueryPlanner(ImplOptions options_in)
    : query_(*ABSL_DIE_IF_NULL(options_in.query)),
      plan_id_(options_in.query->query_id()),
      normalizer_(*ABSL_DIE_IF_NULL(options_in.normalizer)),
      redpath_trie_node_(std::move(options_in.redpath_trie_node)),
      redpath_rules_(
          {.redpath_to_query_params = CombineQueryParams(
               query_,
               std::move(options_in.redpath_rules.redpath_to_query_params),
               options_in.subquery_sequences),
           .redpaths_to_subscribe =
               std::move(options_in.redpath_rules.redpaths_to_subscribe)}),
      redfish_interface_(*ABSL_DIE_IF_NULL(options_in.redfish_interface)),
      service_root_(query_.has_service_root()
                        ? query_.service_root()
                        : std::string(kDefaultRedfishServiceRoot)),
      subquery_id_to_subquery_(GetSubqueryIdToSubquery(query_)),
      subquery_sequences_(std::move(options_in.subquery_sequences)),
      clock_(options_in.clock),
      timeout_manager_(
          options_in.query_timeout.has_value()
              ? std::make_unique<QueryTimeoutManager>(
                    clock_ == nullptr ? *ecclesia::Clock::RealClock() : *clock_,
                    *options_in.query_timeout)
              : nullptr) {}

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

absl::Status QueryPlanner::TryNormalize(
    absl::string_view subquery_id,
    QueryExecutionContext *query_execution_context,
    const RedpathNormalizerOptions &normalizer_options) const {
  auto find_subquery = subquery_id_to_subquery_.find(subquery_id);
  if (find_subquery == subquery_id_to_subquery_.end()) {
    return absl::NotFoundError(
        absl::StrCat("Subquery for subquery id ", subquery_id,
                     " not found in query execution context."));
  }

  // Normalize Redfish Object into query result.
  const std::unique_ptr<RedfishObject> &redfish_object =
      query_execution_context->redfish_object_and_iterable.redfish_object;
  // adding url annotations feature.
  absl::StatusOr<QueryResultData> query_result_data = normalizer_.Normalize(
      *redfish_object, find_subquery->second, normalizer_options);

  if (!query_result_data.ok()) {
    if (query_result_data.status().code() != absl::StatusCode::kNotFound) {
      return query_result_data.status();
    }

    // Not finding a property is not a halting error.
    DLOG(INFO) << "Cannot find queried properties in Redfish Object.\n"
               << "===Redfish Object===\n"
               << redfish_object->GetContentAsJson().dump(1)
               << "\n===Subquery===\n"
               << find_subquery->second.DebugString()
               << "\nError: " << query_result_data.status();
    return absl::OkStatus();
  }

  QueryResultData *new_parent_result = nullptr;

  // When there is no parent subquery dataset.
  // This is typically the case when we are normalizing results for a root
  // subquery.
  if (query_execution_context->parent_subquery_result == nullptr) {
    auto [subquery_output, _] =
        query_execution_context->result.mutable_data()
            ->mutable_fields()
            ->insert({std::string(subquery_id), QueryValue()});
    new_parent_result = subquery_output->second.mutable_list_value()
                            ->add_values()
                            ->mutable_subquery_value();

    *new_parent_result = std::move(*query_result_data);
    query_execution_context->parent_subquery_result = new_parent_result;
    return query_result_data.status();
  }

  auto *child_subquery_output_by_id =
      query_execution_context->parent_subquery_result->mutable_fields();

  // Add normalized subquery dataset to parent subquery dataset.
  // When the current subquery contains an output to populate in parent dataset.
  auto find_parent_subquery_output =
      child_subquery_output_by_id->find(subquery_id);
  if (find_parent_subquery_output != child_subquery_output_by_id->end()) {
    new_parent_result =
        find_parent_subquery_output->second.mutable_list_value()
            ->add_values()
            ->mutable_subquery_value();

    *new_parent_result = std::move(*query_result_data);
    query_execution_context->parent_subquery_result = new_parent_result;
    return query_result_data.status();
  }

  // When the current subquery does not have an output to populate in parent
  // dataset.
  QueryValue *new_parent_subquery_result =
      &(*child_subquery_output_by_id)[subquery_id];
  new_parent_result = new_parent_subquery_result->mutable_list_value()
                          ->add_values()
                          ->mutable_subquery_value();

  *new_parent_result = std::move(*query_result_data);
  query_execution_context->parent_subquery_result = new_parent_result;
  return query_result_data.status();
}

absl::StatusOr<std::vector<QueryExecutionContext>>
QueryPlanner::ExecuteQueryExpression(
    const RedPathExpression &expression,
    QueryExecutionContext &current_execution_context,
    RedPathTrieNode *next_trie_node, std::optional<TraceInfo> &trace_info) {
  std::vector<QueryExecutionContext> execution_contexts;

  const std::unique_ptr<RedfishObject> &current_redfish_obj =
      current_execution_context.redfish_object_and_iterable.redfish_object;
  const std::unique_ptr<RedfishIterable> &current_redfish_iterable =
      current_execution_context.redfish_object_and_iterable.redfish_iterable;

  // Run query expression relative to Iterable resource
  if (current_redfish_iterable) {
    RedPathPrefixTracker &redpath_prefix_tracker =
        current_execution_context.redpath_prefix_tracker;
    std::string new_redpath_prefix = AddExpressionToRedPath(
        redpath_prefix_tracker.last_redpath_prefix, expression);

    // Check if subscription is required.
    bool subscription_required =
        redpath_rules_.redpaths_to_subscribe.contains(new_redpath_prefix);
    std::vector<std::string> uris_to_subscribe;

    // Get query parameters for the RedPath expression we are about to execute.
    GetParams get_params_for_redpath =
        GetQueryParamsForRedPath(new_redpath_prefix);

    // Iterate over resources in collection.
    size_t node_count = current_redfish_iterable->Size();
    for (int node_index = 0; node_index < node_count; ++node_index) {
      RedfishVariant indexed_node = (*current_redfish_iterable)[node_index];
      if (indexed_node.IsFresh() == CacheState::kIsFresh) {
        cache_miss_ = cache_miss_ + 1;
      } else if (indexed_node.IsFresh() == CacheState::kIsCached) {
        cache_hit_ = cache_hit_ + 1;
      }
      ECCLESIA_RETURN_IF_ERROR(indexed_node.status());
      if (trace_info.has_value()) {
        trace_info->redpath_prefix = new_redpath_prefix;
        trace_info->redpath_query = get_params_for_redpath.ToString();
        trace_info->redpath_node = next_trie_node->ToString();
      }
      ECCLESIA_ASSIGN_OR_RETURN(
          std::unique_ptr<RedfishObject> indexed_node_as_object,
          GetRedfishObjectWithFreshness(get_params_for_redpath, indexed_node,
                                        trace_info, &cache_miss_));

      // We don't create new execution context when a subscription is required.
      // Instead, we store each URI of the resource collection in the given
      // execution context which will then be used to create event subscription.
      if (subscription_required) {
        std::optional<std::string> odata_id =
            indexed_node_as_object->GetNodeValue<PropertyOdataId>();
        if (odata_id.has_value()) {
          uris_to_subscribe.push_back(odata_id.value());
        }
        continue;
      }

      ECCLESIA_ASSIGN_OR_RETURN(
          bool predicate_rule_result,
          ExecutePredicateExpression({.predicate = expression.expression,
                                      .node_index = node_index,
                                      .node_set_size = node_count},
                                     current_execution_context,
                                     indexed_node_as_object));

      if (predicate_rule_result) {
        if (current_execution_context.redpath_query_tracker != nullptr) {
          (current_execution_context.redpath_query_tracker
               ->executed_redpath_prefixes_and_params)
              .insert({new_redpath_prefix, get_params_for_redpath});
        }

        RedPathPrefixTracker new_redpath_prefix_tracker =
            current_execution_context.redpath_prefix_tracker;
        new_redpath_prefix_tracker.last_redpath_prefix = new_redpath_prefix;

        QueryExecutionContext new_execution_context(
            &current_execution_context.result,
            current_execution_context.parent_subquery_result, next_trie_node,
            &current_execution_context.query_variables,
            std::move(new_redpath_prefix_tracker),
            current_execution_context.redpath_query_tracker,
            {std::move(indexed_node_as_object), nullptr});
        execution_contexts.push_back(std::move(new_execution_context));
      }
    }

    if (subscription_required) {
      current_execution_context.uris_to_subscribe =
          std::move(uris_to_subscribe);
      return execution_contexts;
    }
  } else if (current_redfish_obj) {
    // Construct RedPath prefix to lookup associated query parameters
    RedPathPrefixTracker &redpath_prefix_tracker =
        current_execution_context.redpath_prefix_tracker;
    std::string new_redpath_prefix = AddExpressionToRedPath(
        redpath_prefix_tracker.last_redpath_prefix, expression);
    // Get query parameters for the RedPath expression we are about to
    // execute.
    GetParams get_params_for_redpath =
        GetQueryParamsForRedPath(new_redpath_prefix);

    // Halt execution and capture URI if RedPath expression requires
    // event subscription.
    if (redpath_rules_.redpaths_to_subscribe.contains(new_redpath_prefix)) {
      nlohmann::json json = current_redfish_obj->GetContentAsJson();
      auto find_node_name = json.find(expression.expression);
      if (find_node_name == json.end()) {
        // It is not an error if a property to subscribe is not in a resource.
        // Eventually, if no properties are subscribed to, the subscription
        // query will fail. So there is no need to fail prematurely here.
        DLOG(INFO) << "Cannot find navigational property in Redfish Object for "
                   << "subscription.";
        return execution_contexts;
      }

      auto find_navigational_property =
          find_node_name->find(PropertyOdataId::Name);
      if (find_navigational_property == find_node_name->end()) {
        return absl::InternalError(
            "Cannot find navigational property in Redfish Object for "
            "subscription.");
      }
      // Append query parameters to navigational property.
      std::string params = get_params_for_redpath.ToString();
      std::string navigational_property =
          absl::StrCat(find_navigational_property->get<std::string>(),
                       params.empty() ? "" : absl::StrCat("?", params));
      // Add URI to subscription.
      current_execution_context.uris_to_subscribe.push_back(
          std::move(navigational_property));
      return execution_contexts;
    }

    RedfishVariant redfish_variant(absl::OkStatus());
    if (expression.type == RedPathExpression::Type::kNodeName) {
      if (get_params_for_redpath.filter.has_value()) {
        // Since filter is enabled all predicates that rely on the redfish data
        // returned from this call need to be added to the $filter parameter
        // that is sent to the Redfish agent.
        absl::StatusOr<std::string> filter_string =
            GetFilterStringFromNextNode(next_trie_node);
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
          redfish_interface_.CachedGetUri(node_name, get_params_for_redpath);

    } else if (expression.type ==
               RedPathExpression::Type::kNodeNameUriPointer) {
      get_params_for_redpath = GetQueryParamsForRedPath(expression.expression);
      redfish_variant = redfish_interface_.CachedGetUri(expression.expression,
                                                        get_params_for_redpath);
    }
    if (redfish_variant.IsFresh() == CacheState::kIsFresh) {
      cache_miss_ = cache_miss_ + 1;
    } else if (redfish_variant.IsFresh() == CacheState::kIsCached) {
      cache_hit_ = cache_hit_ + 1;
    }
    // If a timeout occurred, it will be reported here.
    ECCLESIA_RETURN_IF_ERROR(redfish_variant.status());
    // Set timeout manager if it is available for the Variant so that requests
    // triggered by the variant can count against the query level timeout.
    if (timeout_manager_ != nullptr) {
      redfish_variant.SetTimeoutManager(timeout_manager_.get());
    }
    std::unique_ptr<RedfishObject> redfish_object = nullptr;
    std::unique_ptr<RedfishIterable> redfish_iterable = nullptr;
    if (redfish_iterable = redfish_variant.AsIterable(); !redfish_iterable) {
      redfish_object = redfish_variant.AsObject();

      if (redfish_object == nullptr) {
        DLOG(INFO) << "Cannot query NodeName " << expression.expression
                   << " in Redfish Object:\n"
                   << current_redfish_obj->GetContentAsJson().dump(1)
                   << "\nStatus: " << redfish_variant.status();
        return execution_contexts;
      }

      if (trace_info.has_value()) {
        trace_info->redpath_prefix = new_redpath_prefix;
        trace_info->redpath_query = get_params_for_redpath.ToString();
        trace_info->redpath_node = next_trie_node->ToString();
      }

      // Get fresh Redfish Object if user has requested in the query rule.
      ECCLESIA_ASSIGN_OR_RETURN(
          redfish_object,
          GetRedfishObjectWithFreshness(get_params_for_redpath, redfish_variant,
                                        trace_info, &cache_miss_));
    }

    // Add the last executed RedPath to the record.
    if (current_execution_context.redpath_query_tracker != nullptr) {
      (current_execution_context.redpath_query_tracker
           ->executed_redpath_prefixes_and_params)
          .insert({new_redpath_prefix, get_params_for_redpath});
    }

    RedPathPrefixTracker new_redpath_prefix_tracker =
        current_execution_context.redpath_prefix_tracker;
    new_redpath_prefix_tracker.last_redpath_prefix = new_redpath_prefix;
    QueryExecutionContext new_execution_context(
        &current_execution_context.result,
        current_execution_context.parent_subquery_result, next_trie_node,
        &current_execution_context.query_variables,
        std::move(new_redpath_prefix_tracker),
        current_execution_context.redpath_query_tracker,
        {std::move(redfish_object), std::move(redfish_iterable)});

    execution_contexts.push_back(std::move(new_execution_context));
  }
  return execution_contexts;
}

QueryResult QueryPlanner::Resume(QueryResumeOptions query_resume_options) {
  const RedfishVariant &redfish_variant = query_resume_options.redfish_variant;
  const RedPathTrieNode *next_trie_node = query_resume_options.trie_node;
  std::unique_ptr<RedfishObject> redfish_object = redfish_variant.AsObject();
  std::unique_ptr<RedfishIterable> redfish_iterable =
      redfish_variant.AsIterable();

  QueryResult result;
  result.set_query_id(plan_id_);

  // Initialize query execution context to execute next RedPath
  // expression.
  QueryExecutionContext execution_context(
      &result, nullptr, next_trie_node, &query_resume_options.variables,
      RedPathPrefixTracker(), query_resume_options.redpath_query_tracker,
      {std::move(redfish_object), std::move(redfish_iterable)});

  std::optional<TraceInfo> trace_info = std::nullopt;
  if (query_resume_options.log_redfish_traces) {
    trace_info = {
        .query_id = plan_id_,
    };
  }

  // Populate subquery data using current node before processing next
  // expression in the trie.
  if (!execution_context.redpath_trie_node.subquery_id.empty() &&
      execution_context.redfish_object_and_iterable.redfish_object != nullptr) {
    absl::Status normalize_status = TryNormalize(
        execution_context.redpath_trie_node.subquery_id, &execution_context,
        {.enable_url_annotation = query_resume_options.enable_url_annotation});
    if (!normalize_status.ok()) {
      result.mutable_status()->add_errors(absl::StrCat(
          "Unable to normalize resumed query: ", normalize_status.message()));
      result.mutable_status()->set_error_code(
          ecclesia::ErrorCode::ERROR_INTERNAL);
      return result;
    }
  }

  // Begin BFS traversal of the trie.
  std::queue<QueryExecutionContext> node_queue;
  node_queue.push(std::move(execution_context));
  while (!node_queue.empty()) {
    QueryExecutionContext current_execution_context =
        std::move(node_queue.front());
    node_queue.pop();
    const RedPathTrieNode &current_redpath_trie_node =
        current_execution_context.redpath_trie_node;
    for (const auto &[expression, trie_node] :
         current_redpath_trie_node.expression_to_trie_node) {
      // Get subquery id from next trie node. A subquery id would exist only
      // if the node marks the end of a RedPath expression else it would be
      // empty.
      absl::string_view subquery_id = trie_node->subquery_id;
      absl::StatusOr<std::vector<QueryExecutionContext>> execution_contexts =
          ExecuteQueryExpression(expression, current_execution_context,
                                 trie_node.get(), trace_info);
      if (!execution_contexts.ok()) {
        if (execution_contexts.status().code() != absl::StatusCode::kNotFound) {
          PopulateSubqueryErrorStatus(execution_contexts.status(),
                                      current_execution_context, expression);
        }
        continue;
      }

      for (auto &new_execution_context : *execution_contexts) {
        // Populate subquery data before processing next expression
        const std::unique_ptr<RedfishObject> &object =
            new_execution_context.redfish_object_and_iterable.redfish_object;
        if (!subquery_id.empty() && object != nullptr) {
          absl::Status normalize_status =
              TryNormalize(subquery_id, &new_execution_context,
                           {.enable_url_annotation =
                                query_resume_options.enable_url_annotation});
          if (!normalize_status.ok()) {
            result.mutable_status()->add_errors(absl::StrCat(
                "Unable to normalize: ", normalize_status.message()));
            result.mutable_status()->set_error_code(
                ecclesia::ErrorCode::ERROR_INTERNAL);
            return result;
          }
        }
        node_queue.push(std::move(new_execution_context));
      }
    }
  }
  return result;
}

void QueryPlanner::PopulateSubscriptionContext(
    const std::vector<QueryExecutionContext> &execution_contexts,
    QueryExecutionContext &current_execution_context,
    const RedPathExpression &expression, RedPathTrieNode *next_trie_node,
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

  if (next_trie_node != nullptr &&
      expression.type == RedPathExpression::Type::kPredicate) {
    subscription.predicate = expression.expression;
  }

  subscription_context->subscription_configs.push_back(std::move(subscription));
  subscription_context->redpath_to_trie_node.insert(
      {redpath_subscribed, next_trie_node});
  subscription_context->query_variables = query_execution_options.variables;
  subscription_context->log_redfish_traces =
      query_execution_options.log_redfish_traces;
}

QueryPlanner::QueryExecutionResult QueryPlanner::Run(
    QueryExecutionOptions query_execution_options) {
  QueryResult result;
  if (timeout_manager_ != nullptr && !timeout_manager_->StartTiming().ok()) {
    result.mutable_status()->add_errors(
        "Timed out before query execution could start");
    result.mutable_status()->set_error_code(
        ecclesia::ErrorCode::ERROR_QUERY_TIMEOUT);
    return QueryExecutionResult{.query_result = std::move(result)};
  }
  // Each metrical_transport object has a thread local RedfishMetrics object.
  const RedfishMetrics *metrics = MetricalRedfishTransport::GetConstMetrics();

  // Get Query Parameters to use for service root
  GetParams get_params = GetQueryParamsForRedPath(kServiceRootNode);

  result.set_query_id(plan_id_);
  // Get service root
  RedfishVariant variant = redfish_interface_.GetRoot(
      GetParams{.timeout_manager = get_params.timeout_manager}, service_root_);
  if (variant.IsFresh() == CacheState::kIsFresh) {
    cache_miss_ = cache_miss_ + 1;
  } else if (variant.IsFresh() == CacheState::kIsCached) {
    cache_hit_ = cache_hit_ + 1;
  }
  if (variant.status().code() == absl::StatusCode::kDeadlineExceeded) {
    result.mutable_status()->add_errors(
        "Timed out while querying service root");
    result.mutable_status()->set_error_code(
        ecclesia::ErrorCode::ERROR_QUERY_TIMEOUT);
    return QueryExecutionResult{.query_result = std::move(result)};
  }
  absl::StatusOr<std::unique_ptr<RedfishObject>> service_root_object =
      GetRedfishObjectWithFreshness(get_params, variant, std::nullopt,
                                    &cache_miss_);


  auto set_time = [](absl::Time time, google::protobuf::Timestamp &field) {
    if (auto timestamp = AbslTimeToProtoTime(time); timestamp.ok()) {
      field = *std::move(timestamp);
    }
  };
  if (clock_ != nullptr) {
    set_time(clock_->Now(), *result.mutable_stats()->mutable_start_time());
  }
  // If service root is unreachable populate the error and return the result.
  if (!service_root_object.ok() || *service_root_object == nullptr) {
    result.mutable_status()->add_errors(
        absl::StrCat("Unable to query service root: ",
                     service_root_object.status().message()));
    result.mutable_status()->set_error_code(
        service_root_object.status().code() ==
                absl::StatusCode::kDeadlineExceeded
            ? ecclesia::ErrorCode::ERROR_QUERY_TIMEOUT
            : ecclesia::ErrorCode::ERROR_SERVICE_ROOT_UNREACHABLE);
    return QueryExecutionResult{.query_result = std::move(result)};
  }
  // Initialize query execution context to execute next RedPath expression.
  QueryExecutionContext query_execution_context(
      &result, nullptr, redpath_trie_node_.get(),
      &query_execution_options.variables, RedPathPrefixTracker(),
      query_execution_options.redpath_query_tracker,
      {std::move(*service_root_object), nullptr});

  // Populate subquery data from root node.
  if (!query_execution_context.redpath_trie_node.subquery_id.empty()) {
    absl::Status normalize_status =
        TryNormalize(query_execution_context.redpath_trie_node.subquery_id,
                     &query_execution_context,
                     {.enable_url_annotation =
                          query_execution_options.enable_url_annotation});
    if (!normalize_status.ok()) {
      result.mutable_status()->add_errors(
          absl::StrCat("Unable to query data from service root: ",
                       normalize_status.message()));
      result.mutable_status()->set_error_code(
          ecclesia::ErrorCode::ERROR_INTERNAL);
      return QueryExecutionResult{.query_result = std::move(result)};
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
        current_execution_context.redpath_trie_node;

    for (const auto &[expression, next_trie_node] :
         current_redpath_trie_node.expression_to_trie_node) {
      // Get subquery id from next trie node. A subquery id would exist only
      // if the node marks the end of a RedPath expression else it would be
      // empty.
      absl::string_view subquery_id = next_trie_node->subquery_id;

      absl::StatusOr<std::vector<QueryExecutionContext>> execution_contexts =
          ExecuteQueryExpression(expression, current_execution_context,
                                 next_trie_node.get(), trace_info);
      // Exit query execution and populate error if querying fails. If the error
      // is due to the resource not being found continue as this is allowed by
      // query engine.
      if (!execution_contexts.ok()) {
        if (execution_contexts.status().code() == absl::StatusCode::kNotFound) {
          continue;
        }
        PopulateSubqueryErrorStatus(execution_contexts.status(),
                                    current_execution_context, expression);
        return QueryExecutionResult{
            .query_result = std::move(current_execution_context.result)};
      }

      // Handle subscription request.
      PopulateSubscriptionContext(
          *execution_contexts, current_execution_context, expression,
          next_trie_node.get(), query_execution_options, subscription_context);

      for (auto &execution_context : *execution_contexts) {
        // Populate subquery data before processing next expression
        const std::unique_ptr<RedfishObject> &redfish_object =
            execution_context.redfish_object_and_iterable.redfish_object;

        if (!subquery_id.empty() && redfish_object != nullptr) {
          absl::Status normalize_status =
              TryNormalize(subquery_id, &execution_context,
                           {.enable_url_annotation =
                                query_execution_options.enable_url_annotation});
          if (!normalize_status.ok()) {
            result.mutable_status()->add_errors(absl::StrCat(
                "Unable to normalize: ", normalize_status.message()));
            result.mutable_status()->set_error_code(
                ecclesia::ErrorCode::ERROR_INTERNAL);
            return QueryExecutionResult{.query_result = std::move(result)};
          }
        }
        node_queue.push(std::move(execution_context));
      }
    }
  }
  if (metrics != nullptr && !metrics->uri_to_metrics_map().empty() &&
      result.IsInitialized()) {
    *result.mutable_stats()->mutable_redfish_metrics() = *metrics;
    uint64_t request_count = 0;
    for (const auto &uri_x_metric : metrics->uri_to_metrics_map()) {
      for (const auto &[request_type, metadata] :
           uri_x_metric.second.request_type_to_metadata()) {
        request_count += metadata.request_count();
      }
    }

    result.mutable_stats()->set_num_requests(
        static_cast<int64_t>(request_count));
  }

  result.mutable_stats()->set_payload_size(
      static_cast<int64_t>(result.ByteSizeLong()));

  if (clock_ != nullptr) {
    set_time(clock_->Now(), *result.mutable_stats()->mutable_end_time());
  }
  QueryExecutionResult execution_result;

  execution_result.subscription_context = std::move(subscription_context);
  MetricalRedfishTransport::ResetMetrics();

  result.mutable_stats()->set_num_cache_misses(cache_miss_);
  cache_miss_ = 0;

  result.mutable_stats()->set_num_cache_hits(cache_hit_);
  cache_hit_ = 0;

  execution_result.query_result = result;

  return execution_result;
}

// Builds query plan for given query and returns QueryPlanner instance to the
// caller which can be used to execute the QueryPlan.
absl::StatusOr<std::unique_ptr<QueryPlannerIntf>> BuildQueryPlanner(
    QueryPlannerIntf::QueryPlannerOptions query_planner_options) {
  RedPathTrieBuilder redpath_trie_builder(&query_planner_options.query);
  ECCLESIA_ASSIGN_OR_RETURN(std::unique_ptr<RedPathTrieNode> redpath_trie,
                            redpath_trie_builder.CreateRedPathTrie());
  ECCLESIA_ASSIGN_OR_RETURN(
      const absl::flat_hash_set<std::vector<std::string>> *subquery_sequences,
      redpath_trie_builder.GetSubquerySequences());
  CHECK(subquery_sequences != nullptr);

  // Create QueryPlanner
  return std::make_unique<QueryPlanner>(QueryPlanner::ImplOptions{
      .query = &query_planner_options.query,
      .normalizer = query_planner_options.normalizer,
      .redfish_interface = query_planner_options.redfish_interface,
      .redpath_trie_node = std::move(redpath_trie),
      .redpath_rules = std::move(query_planner_options.redpath_rules),
      .subquery_sequences = *subquery_sequences,
      .clock = query_planner_options.clock,
      .query_timeout = query_planner_options.query_timeout});
}

}  // namespace ecclesia
