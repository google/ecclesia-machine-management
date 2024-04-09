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
#include <cstddef>
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
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/interface.h"
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
#include "ecclesia/lib/redfish/redpath/definitions/query_result/converter.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/redfish/redpath/engine/query_planner.h"
#include "ecclesia/lib/redfish/redpath/engine/redpath_trie.h"
#include "ecclesia/lib/status/macros.h"
#include "single_include/nlohmann/json.hpp"
#include "re2/re2.h"

namespace ecclesia {

namespace {

// Pattern for location step: NodeName[Predicate]
constexpr LazyRE2 kLocationStepRegex = {
    "^([a-zA-Z#@][0-9a-zA-Z.]+|)(?:\\[(.*?)\\]|)$"};
constexpr absl::string_view kPredicateSelectAll = "*";

// All RedPath expressions execute relative to service root identified by '/'.
constexpr absl::string_view kServiceRootNode = "/";
constexpr absl::string_view kDefaultRedfishServiceRoot = "/redfish/v1";

using SubqueryIdToSubquery =
    absl::flat_hash_map<std::string, DelliciusQuery::Subquery>;
using SubqueryOutputById = absl::flat_hash_map<std::string, SubqueryOutput>;

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
  // Now count the possible expand levels in the diff expresion.
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

// Returns SubqueryOutput for given subquery_id.
SubqueryOutput &GetSubqueryOutputForId(
    SubqueryOutputById &subquery_output_by_id, absl::string_view subquery_id) {
  auto find_subquery_output = subquery_output_by_id.find(subquery_id);
  CHECK(find_subquery_output != subquery_output_by_id.end())
      << "Subquery output not found for id: " << subquery_id;
  return find_subquery_output->second;
}

// Initializes SubqueryOutputById for each subquery.
SubqueryOutputById InitializeSubqueryOutput(
    const absl::flat_hash_set<std::vector<std::string>> &subquery_sequences) {
  SubqueryOutputById subquery_output_by_id;
  for (const auto &subquery_sequence : subquery_sequences) {
    for (absl::string_view subquery : subquery_sequence) {
      auto find_subquery_output = subquery_output_by_id.find(subquery);
      if (find_subquery_output == subquery_output_by_id.end()) {
        subquery_output_by_id[subquery] = SubqueryOutput();
      }
    }
  }
  return subquery_output_by_id;
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
    const GetParams &params, const RedfishVariant &variant) {
  std::unique_ptr<RedfishObject> redfish_object = variant.AsObject();
  if (!redfish_object) return nullptr;
  if (params.freshness != GetParams::Freshness::kRequired) {
    return redfish_object;
  }
  absl::StatusOr<std::unique_ptr<RedfishObject>> refetch_obj =
      redfish_object->EnsureFreshPayload();
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

// Tracks RedPath prefixes executed along with Redfish QueryParameters used
// in QueryExecution.
struct RedPathPrefixTracker {
  absl::flat_hash_map<std::string, GetParams>
      *executed_redpath_prefixes_and_params = nullptr;
  std::string last_redpath_prefix;
};

// Describes the Redfish resource relative to which RedPath expressions execute.
struct RedfishObjectAndIterable {
  // A singleton Redfish resource.
  std::unique_ptr<RedfishObject> redfish_object;
  // Redfish collection or any iterable Redfish object - collection of primitive
  // or complex types.
  std::unique_ptr<RedfishIterable> redfish_iterable;
};

// Describes the context used to execute a RedPath expression.
// It contains the Redfish resource as RedfishIterable or RedfishObject and
// any context necessary to execute RedPath expression relative to the resource.
// Therefore, each Redfish resource will be assigned an execution context if
// a RedPath expression needs to execute relative that Redfish resource.
struct QueryExecutionContext {
  // Maps subquery output to subquery id.
  SubqueryOutputById *subquery_output_by_id;
  // RedPath Trie Node that containing next set of RedPath expressions to
  // execute.
  const RedPathTrieNode *redpath_trie_node;
  // QueryVariables used to execute a RedPath expression.
  const QueryVariables *query_variables;
  // Tracks RedPath prefixes executed along with Redfish QueryParameters.
  RedPathPrefixTracker redpath_prefix_tracker;
  // Redfish resource relative to which RedPath expressions execute.
  RedfishObjectAndIterable redfish_object_and_iterable;

  // Redfish URIs to subscribe to.
  // Always empty unless subscription is requested.
  std::vector<std::string> uris_to_subscribe;

  QueryExecutionContext(
      SubqueryOutputById *subquery_output_by_id_in,
      const RedPathTrieNode *redpath_trie_node_in,
      const QueryVariables *query_variables_in,
      RedPathPrefixTracker redpath_prefix_tracker_in,
      RedfishObjectAndIterable redfish_object_and_iterable_in = {
          /*redfish_object=*/nullptr, /*redfish_iterable=*/nullptr})
      : subquery_output_by_id(ABSL_DIE_IF_NULL(subquery_output_by_id_in)),
        redpath_trie_node(ABSL_DIE_IF_NULL(redpath_trie_node_in)),
        query_variables(ABSL_DIE_IF_NULL(query_variables_in)),
        redpath_prefix_tracker(std::move(redpath_prefix_tracker_in)),
        redfish_object_and_iterable(std::move(redfish_object_and_iterable_in)) {
  }
};

// QueryPlannerImpl encapsulates the logic to interpret subqueries, deduplicate
// RedPath path expressions, dispatch an optimum number of redfish resource
// requests, and return normalized response data per given property requirement.
//
// QueryPlannerImpl is a thread safe implementation.
class QueryPlanner final : public QueryPlannerIntf {
 public:
  // Options for QueryPlanner. These are private query planner options
  // translated from QueryPlannerIntf::QueryPlannerOptions.
  struct Options {
    const DelliciusQuery *query = nullptr;
    Normalizer *normalizer = nullptr;
    RedfishInterface *redfish_interface = nullptr;
    std::unique_ptr<RedPathTrieNode> redpath_trie_node = nullptr;
    QueryPlannerOptions::RedPathRules redpath_rules;
    absl::flat_hash_set<std::vector<std::string>> subquery_sequences;
  };

  explicit QueryPlanner(Options options_in)
      : query_(ABSL_DIE_IF_NULL(options_in.query)),
        plan_id_(options_in.query->query_id()),
        normalizer_(ABSL_DIE_IF_NULL(options_in.normalizer)),
        redpath_trie_node_(std::move(options_in.redpath_trie_node)),
        redpath_rules_(
            {.redpath_to_query_params = CombineQueryParams(
                 *query_,
                 std::move(options_in.redpath_rules.redpath_to_query_params),
                 options_in.subquery_sequences),
             .redpaths_to_subscribe =
                 std::move(options_in.redpath_rules.redpaths_to_subscribe)}),
        redfish_interface_(options_in.redfish_interface),
        service_root_(query_->has_service_root()
                          ? query_->service_root()
                          : std::string(kDefaultRedfishServiceRoot)),
        subquery_id_to_subquery_(GetSubqueryIdToSubquery(*query_)),
        subquery_sequences_(std::move(options_in.subquery_sequences)) {}

  GetParams GetQueryParamsForRedPath(absl::string_view redpath_prefix);

  // Normalization of Redfish Object into query result is a best effort process
  // which does not error out if requested properties are not found in Redfish
  // Object.
  void TryNormalize(absl::string_view subquery_id,
                    QueryExecutionContext *query_execution_context) const;

  // Joins subquery output with parent subquery output.
  void JoinSubqueryOutput(SubqueryOutputById subquery_output_by_id,
                          DelliciusQueryResult &result);

  // Executes a single RedPath expression.
  absl::StatusOr<std::vector<QueryExecutionContext>> ExecuteQueryExpression(
      const RedPathExpression &expression,
      QueryExecutionContext &current_execution_context,
      RedPathTrieNode *next_trie_node);

  void PopulateSubscriptionContext(
      const std::vector<QueryExecutionContext> &execution_contexts,
      QueryExecutionContext &current_execution_context,
      const RedPathExpression &expression, RedPathTrieNode *next_trie_node,
      const QueryPlannerIntf::QueryExecutionOptions &query_execution_options,
      std::unique_ptr<QueryPlannerIntf::SubscriptionContext>
          &subscription_context);

  // Executes Query Plan per given `query_execution_options`.
  absl::StatusOr<QueryExecutionResult> Run(
      QueryExecutionOptions query_execution_options) override;

  // Resumes execution of a RedPath expression on receiving event.
  absl::StatusOr<QueryResult> Resume(
      QueryResumeOptions query_resume_options) override;

 private:
  const DelliciusQuery *query_;
  const std::string plan_id_;
  // Normalizer is thread safe.
  Normalizer *normalizer_;
  const std::unique_ptr<RedPathTrieNode> redpath_trie_node_;
  const QueryPlannerOptions::RedPathRules redpath_rules_;
  // Redfish interface is thread safe.
  RedfishInterface *redfish_interface_;
  const std::string service_root_;
  const SubqueryIdToSubquery subquery_id_to_subquery_;
  const absl::flat_hash_set<std::vector<std::string>> subquery_sequences_;
};

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
  return params;
}

void QueryPlanner::TryNormalize(
    absl::string_view subquery_id,
    QueryExecutionContext *query_execution_context) const {
  auto find_subquery = subquery_id_to_subquery_.find(subquery_id);
  CHECK(find_subquery != subquery_id_to_subquery_.end());

  // Normalize Redfish Object into query result.
  const std::unique_ptr<RedfishObject> &redfish_object =
      query_execution_context->redfish_object_and_iterable.redfish_object;
  // annotations feature.
  absl::StatusOr<SubqueryDataSet> subquery_dataset =
      normalizer_->Normalize(*redfish_object, find_subquery->second, {});
  if (!subquery_dataset.ok()) {
    DLOG(INFO) << "Cannot find queried properties in Redfish Object.\n"
               << "===Redfish Object===\n"
               << redfish_object->GetContentAsJson().dump(1)
               << "\n===Subquery===\n"
               << find_subquery->second.DebugString()
               << "\nError: " << subquery_dataset.status();
    return;
  }
  auto find_subquery_output =
      query_execution_context->subquery_output_by_id->find(subquery_id);
  CHECK(find_subquery_output !=
        query_execution_context->subquery_output_by_id->end());
  find_subquery_output->second.mutable_data_sets()->Add(
      std::move(*subquery_dataset));
}

absl::StatusOr<bool> ExecutePredicateExpression(
    const PredicateOptions &predicate_options,
    QueryExecutionContext &current_execution_context,
    const std::unique_ptr<RedfishObject> &redfish_object) {
  std::string new_predicate = predicate_options.predicate;
  for (const auto &value :
       current_execution_context.query_variables->values()) {
    if (value.name().empty()) continue;
    std::string variable_name = absl::StrCat("$", value.name());
    if (absl::StrContains(new_predicate, variable_name)) {
      new_predicate =
          absl::StrReplaceAll(new_predicate, {{variable_name, value.value()}});
    }
  }

  ECCLESIA_ASSIGN_OR_RETURN(
      bool predicate_rule_result,
      ApplyPredicateRule(redfish_object->GetContentAsJson(),
                         {.predicate = new_predicate,
                          .node_index = predicate_options.node_index,
                          .node_set_size = predicate_options.node_set_size}));

  return predicate_rule_result;
}

absl::StatusOr<std::vector<QueryExecutionContext>>
QueryPlanner::ExecuteQueryExpression(
    const RedPathExpression &expression,
    QueryExecutionContext &current_execution_context,
    RedPathTrieNode *next_trie_node) {
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
      ECCLESIA_RETURN_IF_ERROR(indexed_node.status());
      ECCLESIA_ASSIGN_OR_RETURN(
          std::unique_ptr<RedfishObject> indexed_node_as_object,
          GetRedfishObjectWithFreshness(get_params_for_redpath, indexed_node));

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
        RedPathPrefixTracker new_redpath_prefix_tracker =
            current_execution_context.redpath_prefix_tracker;
        new_redpath_prefix_tracker.last_redpath_prefix = new_redpath_prefix;
        QueryExecutionContext new_execution_context(
            current_execution_context.subquery_output_by_id, next_trie_node,
            current_execution_context.query_variables,
            std::move(new_redpath_prefix_tracker),
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
        return absl::InternalError(
            "Cannot find navigational property in Redfish Object for "
            "subscription.");
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
          redfish_interface_->CachedGetUri(node_name, get_params_for_redpath);
    } else if (expression.type ==
               RedPathExpression::Type::kNodeNameUriPointer) {
      get_params_for_redpath = GetQueryParamsForRedPath(expression.expression);

      redfish_variant = redfish_interface_->CachedGetUri(
          expression.expression, get_params_for_redpath);
    }
    ECCLESIA_RETURN_IF_ERROR(redfish_variant.status());

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

      // Get fresh Redfish Object if user has requested in the query rule.
      ECCLESIA_ASSIGN_OR_RETURN(
          redfish_object, GetRedfishObjectWithFreshness(get_params_for_redpath,
                                                        redfish_variant));
    }

    // Add the last executed RedPath to the record.
    if (redpath_prefix_tracker.executed_redpath_prefixes_and_params !=
        nullptr) {
      (*redpath_prefix_tracker.executed_redpath_prefixes_and_params)
          .insert({new_redpath_prefix, get_params_for_redpath});
    }

    RedPathPrefixTracker new_redpath_prefix_tracker =
        current_execution_context.redpath_prefix_tracker;
    new_redpath_prefix_tracker.last_redpath_prefix = new_redpath_prefix;
    QueryExecutionContext new_execution_context(
        current_execution_context.subquery_output_by_id, next_trie_node,
        current_execution_context.query_variables,
        std::move(new_redpath_prefix_tracker),
        {std::move(redfish_object), std::move(redfish_iterable)});

    execution_contexts.push_back(std::move(new_execution_context));
  }
  return execution_contexts;
}

void QueryPlanner::JoinSubqueryOutput(SubqueryOutputById subquery_output_by_id,
                                      DelliciusQueryResult &result) {
  for (const auto &subquery_sequence : subquery_sequences_) {
    std::vector<std::string>::const_reverse_iterator iter =
        subquery_sequence.rbegin();
    std::string last_subquery_id = *iter++;

    for (; iter != subquery_sequence.rend(); ++iter) {
      SubqueryOutput &child_subquery_output =
          GetSubqueryOutputForId(subquery_output_by_id, last_subquery_id);
      SubqueryOutput &parent_subquery_output =
          GetSubqueryOutputForId(subquery_output_by_id, *iter);

      // If parent subquery output has no datasets, create an empty dataset
      // to house the child subquery response.
      if (parent_subquery_output.data_sets().empty()) {
        parent_subquery_output.mutable_data_sets()->Add();
      }

      for (SubqueryDataSet &data_set :
           *parent_subquery_output.mutable_data_sets()) {
        data_set.mutable_child_subquery_output_by_id()->insert(
            {last_subquery_id, child_subquery_output});
      }
      last_subquery_id = *iter;
    }
  }

  // Populate result
  for (const auto &subquery_sequence : subquery_sequences_) {
    absl::string_view subquery_id = *subquery_sequence.begin();
    auto &subquery_output =
        GetSubqueryOutputForId(subquery_output_by_id, subquery_id);
    result.mutable_subquery_output_by_id()->insert(
        {std::string(subquery_id), subquery_output});
  }
}

absl::StatusOr<QueryResult> QueryPlanner::Resume(
    QueryResumeOptions query_resume_options) {
  const RedfishVariant &redfish_variant = query_resume_options.redfish_variant;
  const RedPathTrieNode *next_trie_node = query_resume_options.trie_node;
  std::unique_ptr<RedfishObject> redfish_object = redfish_variant.AsObject();
  std::unique_ptr<RedfishIterable> redfish_iterable =
      redfish_variant.AsIterable();

  // Initialize subquery output.
  SubqueryOutputById subquery_output_by_id =
      InitializeSubqueryOutput(subquery_sequences_);

  // Initialize query execution context to execute next RedPath
  // expression.
  QueryExecutionContext execution_context(
      &subquery_output_by_id, next_trie_node, &query_resume_options.variables,
      RedPathPrefixTracker(),
      {std::move(redfish_object), std::move(redfish_iterable)});

  // Populate subquery data using current node before processing next
  // expression in the trie.
  if (!execution_context.redpath_trie_node->subquery_id.empty() &&
      execution_context.redfish_object_and_iterable.redfish_object != nullptr) {
    TryNormalize(execution_context.redpath_trie_node->subquery_id,
                 &execution_context);
  }

  // Begin BFS traversal of the trie.
  std::queue<QueryExecutionContext> node_queue;
  node_queue.push(std::move(execution_context));
  while (!node_queue.empty()) {
    QueryExecutionContext current_execution_context =
        std::move(node_queue.front());
    node_queue.pop();
    const RedPathTrieNode *current_redpath_trie_node =
        current_execution_context.redpath_trie_node;
    for (const auto &[expression, trie_node] :
         current_redpath_trie_node->expression_to_trie_node) {
      // Get subquery id from next trie node. A subquery id would exist only
      // if the node marks the end of a RedPath expression else it would be
      // empty.
      absl::string_view subquery_id = trie_node->subquery_id;
      ECCLESIA_ASSIGN_OR_RETURN(
          std::vector<QueryExecutionContext> execution_contexts,
          ExecuteQueryExpression(expression, current_execution_context,
                                 trie_node.get()));

      for (auto &new_execution_context : execution_contexts) {
        // Populate subquery data before processing next expression
        const std::unique_ptr<RedfishObject> &object =
            new_execution_context.redfish_object_and_iterable.redfish_object;
        if (!subquery_id.empty() && object != nullptr) {
          TryNormalize(subquery_id, &new_execution_context);
        }
        node_queue.push(std::move(new_execution_context));
      }
    }
  }

  DelliciusQueryResult result;
  result.set_query_id(plan_id_);
  JoinSubqueryOutput(std::move(subquery_output_by_id), result);
  return ToQueryResult(result);
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
}

absl::StatusOr<QueryPlannerIntf::QueryExecutionResult> QueryPlanner::Run(
    QueryExecutionOptions query_execution_options) {
  // Get Query Parameters to use for service root
  GetParams get_params = GetQueryParamsForRedPath(kServiceRootNode);


  // Get service root
  RedfishVariant variant =
      redfish_interface_->GetRoot(GetParams{}, service_root_);
  ECCLESIA_ASSIGN_OR_RETURN(std::unique_ptr<RedfishObject> service_root_object,
                            GetRedfishObjectWithFreshness(get_params, variant));

  // Initialize subquery output.
  SubqueryOutputById subquery_output_by_id =
      InitializeSubqueryOutput(subquery_sequences_);

  // Initialize query execution context to execute next RedPath expression.
  QueryExecutionContext query_execution_context(
      &subquery_output_by_id, redpath_trie_node_.get(),
      &query_execution_options.variables, RedPathPrefixTracker(),
      {std::move(service_root_object), nullptr});

  std::queue<QueryExecutionContext> node_queue;
  node_queue.push(std::move(query_execution_context));

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

    const RedPathTrieNode *current_redpath_trie_node =
        current_execution_context.redpath_trie_node;

    for (const auto &[expression, next_trie_node] :
         current_redpath_trie_node->expression_to_trie_node) {
      // Get subquery id from next trie node. A subquery id would exist only
      // if the node marks the end of a RedPath expression else it would be
      // empty.
      absl::string_view subquery_id = next_trie_node->subquery_id;
      ECCLESIA_ASSIGN_OR_RETURN(
          std::vector<QueryExecutionContext> execution_contexts,
          ExecuteQueryExpression(expression, current_execution_context,
                                 next_trie_node.get()));

      // Handle subscription request.
      PopulateSubscriptionContext(
          execution_contexts, current_execution_context, expression,
          next_trie_node.get(), query_execution_options, subscription_context);

      for (auto &execution_context : execution_contexts) {
        // Populate subquery data before processing next expression
        const std::unique_ptr<RedfishObject> &redfish_object =
            execution_context.redfish_object_and_iterable.redfish_object;
        if (!subquery_id.empty() && redfish_object != nullptr) {
          TryNormalize(subquery_id, &execution_context);
        }
        node_queue.push(std::move(execution_context));
      }
    }
  }

  DelliciusQueryResult result;
  result.set_query_id(plan_id_);
  JoinSubqueryOutput(std::move(subquery_output_by_id), result);

  QueryExecutionResult execution_result;
  execution_result.query_result = ToQueryResult(result);
  execution_result.subscription_context = std::move(subscription_context);
  return execution_result;
}

}  // namespace

// Builds query plan for given query and returns QueryPlanner instance to the
// caller which can be used to execute the QueryPlan.
absl::StatusOr<std::unique_ptr<QueryPlannerIntf>> BuildQueryPlanner(
    QueryPlannerOptions query_planner_options) {
  RedPathTrieBuilder redpath_trie_builder(&query_planner_options.query);
  ECCLESIA_ASSIGN_OR_RETURN(std::unique_ptr<RedPathTrieNode> redpath_trie,
                            redpath_trie_builder.CreateRedPathTrie());
  ECCLESIA_ASSIGN_OR_RETURN(
      const absl::flat_hash_set<std::vector<std::string>> *subquery_sequences,
      redpath_trie_builder.GetSubquerySequences());
  CHECK(subquery_sequences != nullptr);

  // Create QueryPlanner
  return std::make_unique<QueryPlanner>(QueryPlanner::Options{
      .query = &query_planner_options.query,
      .normalizer = query_planner_options.normalizer,
      .redfish_interface = query_planner_options.redfish_interface,
      .redpath_trie_node = std::move(redpath_trie),
      .redpath_rules = std::move(query_planner_options.redpath_rules),
      .subquery_sequences = *subquery_sequences});
}

}  // namespace ecclesia
