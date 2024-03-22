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
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_predicates/filter.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_predicates/predicates.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/converter.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/redfish/redpath/engine/query_planner.h"
#include "ecclesia/lib/redfish/redpath/engine/redpath_trie.h"
#include "ecclesia/lib/status/macros.h"
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
    absl::flat_hash_map<std::string, const DelliciusQuery::Subquery *>;
using SubqueryOutputById = absl::flat_hash_map<std::string, SubqueryOutput>;

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
    id_to_subquery[subquery.subquery_id()] = &subquery;
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
  RedPathTrieNode *redpath_trie_node;
  // QueryVariables used to execute a RedPath expression.
  QueryVariables *query_variables;
  // Tracks RedPath prefixes executed along with Redfish QueryParameters.
  RedPathPrefixTracker redpath_prefix_tracker;

  // Redfish resources relative to which RedPath expressions execute
  // A singleton Redfish resource.
  std::unique_ptr<RedfishObject> redfish_object;
  // Redfish collection or any iterable Redfish object - collection of primitive
  // or complex types.
  std::unique_ptr<RedfishIterable> redfish_iterable;

  QueryExecutionContext(
      SubqueryOutputById *subquery_output_by_id_in,
      RedPathTrieNode *redpath_trie_node_in, QueryVariables *query_variables_in,
      RedPathPrefixTracker redpath_prefix_tracker_in,
      std::unique_ptr<RedfishObject> redfish_object_in = nullptr,
      std::unique_ptr<RedfishIterable> redfish_iterable_in = nullptr)
      : subquery_output_by_id(ABSL_DIE_IF_NULL(subquery_output_by_id_in)),
        redpath_trie_node(ABSL_DIE_IF_NULL(redpath_trie_node_in)),
        query_variables(ABSL_DIE_IF_NULL(query_variables_in)),
        redpath_prefix_tracker(std::move(redpath_prefix_tracker_in)),
        redfish_object(std::move(redfish_object_in)),
        redfish_iterable(std::move(redfish_iterable_in)) {}
};

// QueryPlannerImpl encapsulates the logic to interpret subqueries, deduplicate
// RedPath path expressions, dispatch an optimum number of redfish resource
// requests, and return normalized response data per given property requirement.
//
// QueryPlannerImpl is a thread safe implementation.
class QueryPlanner final : public QueryPlannerIntf {
 public:
  struct QueryPlannerOptions {
    const DelliciusQuery *query = nullptr;
    Normalizer *normalizer = nullptr;
    RedfishInterface *redfish_interface = nullptr;
    std::unique_ptr<RedPathTrieNode> redpath_trie_node = nullptr;
    RedPathRedfishQueryParams redpath_to_query_params;
    absl::flat_hash_set<std::vector<std::string>> subquery_sequences;
  };

  explicit QueryPlanner(QueryPlannerOptions options_in)
      : query_(ABSL_DIE_IF_NULL(options_in.query)),
        plan_id_(options_in.query->query_id()),
        normalizer_(ABSL_DIE_IF_NULL(options_in.normalizer)),
        redpath_trie_node_(std::move(options_in.redpath_trie_node)),
        redpath_to_query_params_(CombineQueryParams(
            *query_, std::move(options_in.redpath_to_query_params),
            options_in.subquery_sequences)),
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

  // Executes Query Plan per given `query_execution_options`.
  absl::StatusOr<QueryResult> Run(
      QueryExecutionOptions query_execution_options) override;

 private:
  const DelliciusQuery *query_;
  const std::string plan_id_;
  // Normalizer is thread safe.
  Normalizer *normalizer_;
  const std::unique_ptr<RedPathTrieNode> redpath_trie_node_;
  const RedPathRedfishQueryParams redpath_to_query_params_;
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
  if (auto iter = redpath_to_query_params_.find(redpath_prefix);
      iter != redpath_to_query_params_.end()) {
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
  absl::StatusOr<SubqueryDataSet> subquery_dataset = normalizer_->Normalize(
      *query_execution_context->redfish_object, *find_subquery->second);
  if (!subquery_dataset.ok()) {
    DLOG(INFO)
        << "Cannot find queried properties in Redfish Object.\n"
        << "===Redfish Object===\n"
        << query_execution_context->redfish_object->GetContentAsJson().dump(1)
        << "\n===Subquery===\n"
        << find_subquery->second << "\nError: " << subquery_dataset.status();
    return;
  }
  auto find_subquery_output =
      query_execution_context->subquery_output_by_id->find(subquery_id);
  CHECK(find_subquery_output !=
        query_execution_context->subquery_output_by_id->end());
  find_subquery_output->second.mutable_data_sets()->Add(
      std::move(*subquery_dataset));
}

absl::StatusOr<std::vector<QueryExecutionContext>>
QueryPlanner::ExecuteQueryExpression(
    const RedPathExpression &expression,
    QueryExecutionContext &current_execution_context,
    RedPathTrieNode *next_trie_node) {
  std::vector<QueryExecutionContext> execution_contexts;

  // Run query expression relative to Iterable resource
  if (current_execution_context.redfish_iterable) {
    RedPathPrefixTracker &redpath_prefix_tracker =
        current_execution_context.redpath_prefix_tracker;
    redpath_prefix_tracker.last_redpath_prefix =
        absl::StrCat(redpath_prefix_tracker.last_redpath_prefix, "[",
                     kPredicateSelectAll, "]");
    GetParams get_params_for_redpath =
        GetQueryParamsForRedPath(redpath_prefix_tracker.last_redpath_prefix);
    std::vector<std::unique_ptr<RedfishObject>> redfish_objects;
    size_t node_count = current_execution_context.redfish_iterable->Size();
    for (int node_index = 0; node_index < node_count; ++node_index) {
      RedfishVariant indexed_node =
          (*current_execution_context.redfish_iterable)[node_index];
      ECCLESIA_RETURN_IF_ERROR(indexed_node.status());
      ECCLESIA_ASSIGN_OR_RETURN(
          std::unique_ptr<RedfishObject> indexed_node_as_object,
          GetRedfishObjectWithFreshness(get_params_for_redpath, indexed_node));

      std::string new_predicate = expression.expression;
      for (const auto &value :
           current_execution_context.query_variables->values()) {
        if (value.name().empty()) continue;
        std::string variable_name = absl::StrCat("$", value.name());
        if (absl::StrContains(new_predicate, variable_name)) {
          new_predicate = absl::StrReplaceAll(new_predicate,
                                              {{variable_name, value.value()}});
        }
      }
      ECCLESIA_ASSIGN_OR_RETURN(
          bool predicate_rule_result,
          ApplyPredicateRule(indexed_node_as_object->GetContentAsJson(),
                             {.predicate = new_predicate,
                              .node_index = node_index,
                              .node_set_size = node_count}));

      if (predicate_rule_result) {
        QueryExecutionContext new_execution_context(
            current_execution_context.subquery_output_by_id, next_trie_node,
            current_execution_context.query_variables, redpath_prefix_tracker,
            std::move(indexed_node_as_object));
        execution_contexts.push_back(std::move(new_execution_context));
      }
    }
  } else if (current_execution_context.redfish_object) {
    // Construct RedPath prefix to lookup associated query parameters
    RedPathPrefixTracker &redpath_prefix_tracker =
        current_execution_context.redpath_prefix_tracker;
    redpath_prefix_tracker.last_redpath_prefix = absl::StrCat(
        redpath_prefix_tracker.last_redpath_prefix, "/", expression.expression);

    // Get query parameters for the RedPath expression we are about to execute.
    GetParams get_params_for_redpath =
        GetQueryParamsForRedPath(redpath_prefix_tracker.last_redpath_prefix);

    if (get_params_for_redpath.filter.has_value()) {
      // Since filter is enabled all predicates that rely on the redfish data
      // returned from this call need to be added to the $filter parameter that
      // is sent to the Redfish agent.
      absl::StatusOr<std::string> filter_string =
          GetFilterStringFromNextNode(next_trie_node);
      if (filter_string.ok()) {
        get_params_for_redpath.filter->SetFilterString(filter_string.value());
      }
    }
    RedfishVariant redfish_variant =
        current_execution_context.redfish_object->Get(expression.expression,
                                                      get_params_for_redpath);
    ECCLESIA_RETURN_IF_ERROR(redfish_variant.status());

    std::unique_ptr<RedfishObject> redfish_object = nullptr;
    std::unique_ptr<RedfishIterable> redfish_iterable = nullptr;
    if (redfish_iterable = redfish_variant.AsIterable(); !redfish_iterable) {
      redfish_object = redfish_variant.AsObject();
      if (redfish_object == nullptr) {
        DLOG(INFO) << "Cannot query NodeName " << expression.expression
                   << " in Redfish Object:\n"
                   << current_execution_context.redfish_object
                          ->GetContentAsJson()
                          .dump(1)
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
          .insert({redpath_prefix_tracker.last_redpath_prefix,
                   get_params_for_redpath});
    }

    QueryExecutionContext new_execution_context(
        current_execution_context.subquery_output_by_id, next_trie_node,
        current_execution_context.query_variables,
        current_execution_context.redpath_prefix_tracker,
        std::move(redfish_object), std::move(redfish_iterable));

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

absl::StatusOr<QueryResult> QueryPlanner::Run(
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
      std::move(service_root_object));

  std::queue<QueryExecutionContext> node_queue;
  node_queue.push(std::move(query_execution_context));

  // Below BFS traversal of RedPath prefix tree follows the following pattern:
  //  ExecutionContext of the root node is used to execute the RedPath
  //  expressions of child nodes. In other words, the result of querying RedPath
  //  expression of root node forms the context node relative to which
  //  expressions in child nodes are executed.
  while (!node_queue.empty()) {
    QueryExecutionContext current_execution_context =
        std::move(node_queue.front());
    node_queue.pop();

    RedPathTrieNode *current_redpath_trie_node =
        current_execution_context.redpath_trie_node;
    std::string redpath_trie_str;
    current_redpath_trie_node->ToString(redpath_trie_str);
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
      for (auto &execution_context : execution_contexts) {
        // Populate subquery data before processing next expression
        if (!subquery_id.empty() &&
            execution_context.redfish_object != nullptr) {
          TryNormalize(subquery_id, &execution_context);
        }
        node_queue.push(std::move(execution_context));
      }
    }
  }

  DelliciusQueryResult result;
  result.set_query_id(plan_id_);
  JoinSubqueryOutput(std::move(subquery_output_by_id), result);
  return ToQueryResult(result);
}

}  // namespace

// Builds query plan for given query and returns QueryPlanner instance to the
// caller which can be used to execute the QueryPlan.
absl::StatusOr<std::unique_ptr<QueryPlannerIntf>> BuildQueryPlanner(
    const DelliciusQuery &query,
    RedPathRedfishQueryParams redpath_to_query_params, Normalizer *normalizer,
    RedfishInterface *redfish_interface) {
  RedPathTrieBuilder redpath_trie_builder(&query);
  ECCLESIA_ASSIGN_OR_RETURN(std::unique_ptr<RedPathTrieNode> redpath_trie,
                            redpath_trie_builder.CreateRedPathTrie());
  ECCLESIA_ASSIGN_OR_RETURN(
      const absl::flat_hash_set<std::vector<std::string>> *subquery_sequences,
      redpath_trie_builder.GetSubquerySequences());
  CHECK(subquery_sequences != nullptr);

  return std::make_unique<QueryPlanner>(QueryPlanner::QueryPlannerOptions{
      .query = &query,
      .normalizer = normalizer,
      .redfish_interface = redfish_interface,
      .redpath_trie_node = std::move(redpath_trie),
      .redpath_to_query_params = std::move(redpath_to_query_params),
      .subquery_sequences = *subquery_sequences});
}

}  // namespace ecclesia
