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

#include "ecclesia/lib/redfish/dellicius/engine/internal/query_planner.h"

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/interface.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/dellicius/utils/path_util.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/status/macros.h"
#include "ecclesia/lib/time/proto.h"
#include "re2/re2.h"

namespace ecclesia {

namespace {

using SubqueryHandle = QueryPlanner::SubqueryHandle;

// Pattern for predicate formatted with relational operators:
constexpr LazyRE2 kPredicateRegexRelationalOperator = {
    "^([a-zA-Z#@][0-9a-zA-Z.]*)(?:(!=|>|<|=|>=|<=))([a-zA-Z0-9._#]+)$"};

constexpr absl::string_view kPredicateSelectAll = "*";
constexpr absl::string_view kPredicateSelectLastIndex = "last()";
constexpr absl::string_view kBinaryOperandTrue = "true";
constexpr absl::string_view kBinaryOperandFalse = "false";
constexpr absl::string_view kLogicalOperatorAnd = "and";
constexpr absl::string_view kLogicalOperatorOr = "or";
// Supported relational operators
constexpr std::array<const char *, 6> kRelationsOperators = {
    "<", ">", "!=", ">=", "<=", "="};

template <typename F>
bool ApplyNumberComparisonFilter(const nlohmann::json &obj, F comparator) {
  double number;
  if (obj.is_number()) {
    number = obj.get<double>();
  } else if (!obj.is_string() ||
             !absl::SimpleAtod(obj.get<std::string>(), &number)) {
    return false;
  }
  return comparator(number);
}

// Helper function is used to ensure the obtained value equal or not equal to a
// non-number value.
template <typename F>
bool ApplyStringComparisonFilter(F filter_condition,
                                 absl::string_view inequality_string) {
  if (inequality_string == "!=") {
    return !filter_condition();
  }
  return filter_condition();
}

// Handler for predicate expressions containing relational operators.
bool PredicateFilterByNodeComparison(const RedfishVariant &variant,
                                     absl::string_view predicate) {
  std::string node_name, op, test_value;
  bool ret = false;
  if (RE2::FullMatch(predicate, *kPredicateRegexRelationalOperator, &node_name,
                     &op, &test_value)) {
    double value;
    auto json_obj = ResolveNodeNameToJsonObj(variant, node_name);
    if (!json_obj.ok()) {
      return false;
    }

    // Number comparison.
    if (absl::SimpleAtod(test_value, &value)) {
      const auto condition = [&op, value](double number) {
        if (op == ">=") return number >= value;
        if (op == ">") return number > value;
        if (op == "<=") return number <= value;
        if (op == "<") return number < value;
        if (op == "!=") return number != value;
        if (op == "=") return number == value;
        return false;
      };
      ret = ApplyNumberComparisonFilter(*json_obj, condition);
    } else if (test_value == kBinaryOperandFalse ||
               test_value == kBinaryOperandTrue) {
      // For the property value's type is boolean.
      bool bool_value = test_value != kBinaryOperandFalse;
      const auto condition = [json_obj, bool_value]() {
        return *json_obj == bool_value;
      };
      ret = ApplyStringComparisonFilter(condition, op);
    } else if (test_value == "null") {
      // For the property value is null.
      const auto condition = [json_obj]() { return json_obj->is_null(); };
      ret = ApplyStringComparisonFilter(condition, op);
    } else {
      // For the property value's type is string.
      const auto condition = [json_obj, test_value]() {
        return *json_obj == test_value;
      };
      ret = ApplyStringComparisonFilter(condition, op);
    }
  }
  return ret;
}

// Handler for '[nodename]'
// Checks if given Redfish Resource encapsulated in the Variant contains
// predicate string.
bool PredicateFilterByNodeName(const RedfishVariant &variant,
                               absl::string_view predicate) {
  std::vector<std::string> node_names = SplitNodeNameForNestedNodes(predicate);
  if (node_names.empty()) {
    return false;
  }
  nlohmann::json leaf = variant.AsObject()->GetContentAsJson();
  for (auto const &name : node_names) {
    if (!leaf.contains(name)) {
      return false;
    }
    leaf = leaf.at(name);
  }
  return true;
}

bool ApplyPredicateRule(const RedfishVariant &node, size_t node_index,
                        size_t node_set_size,
                        const SubqueryHandle::RedPathIterator &iter) {
  std::string_view predicate = iter->second;
  absl::string_view logical_operation = kLogicalOperatorAnd;
  bool is_filter_success = true;
  for (absl::string_view expr : absl::StrSplit(predicate, ' ')) {
    // If expression is a logical operator, capture it and move to next
    // expression
    if (expr == kLogicalOperatorAnd || expr == kLogicalOperatorOr) {
      // A binary operator is parsed only when last operator has been applied.
      // Since last operator has not been applied and we are seeing another
      // operator in the expression, it can be considered an invalid expression.
      if (!logical_operation.empty()) {
        LOG(ERROR) << "Invalid predicate expression " << predicate;
        return false;
      }
      logical_operation = expr;
      continue;
    }

    // There should always be a logical operation defined for the predicates.
    // Default logical operation is 'AND' between a predicate expression and
    // default boolean operand 'true'
    if (logical_operation.empty()) {
      LOG(ERROR) << "Invalid predicate expression " << predicate;
      return false;
    }

    size_t num;
    bool single_predicate_result = false;
    // If '[last()]' predicate expression, check if current node at last index.
    if ((expr == kPredicateSelectLastIndex &&
         node_index == node_set_size - 1) ||
        // If '[Index]' predicate expression, check if current node at given
        // index
        (absl::SimpleAtoi(expr, &num) && num == node_index) ||
        // If '[*]' predicate expression or empty predicate, no filter required.
        (expr.empty() || expr == kPredicateSelectAll)) {
      single_predicate_result = true;
    } else if (std::any_of(kRelationsOperators.begin(),
                           kRelationsOperators.end(), [&](const char *op) {
                             return absl::StrContains(expr, op);
                           })) {
      // Look for predicate expression containing relational operators.
      single_predicate_result = PredicateFilterByNodeComparison(node, expr);
    } else {
      // Filter node-set by NodeName
      single_predicate_result = PredicateFilterByNodeName(node, expr);
    }
    // Apply logical operation.
    if (logical_operation == kLogicalOperatorAnd) {
      is_filter_success &= single_predicate_result;
    } else {
      is_filter_success |= single_predicate_result;
    }
    // Reset logical operation
    logical_operation = "";
  }
  return is_filter_success;
}

using NodeNameToRedPathContexts =
    absl::flat_hash_map<std::string, std::vector<QueryPlanner::RedPathContext>>;

// Deduplicates the next NodeName expression in the RedPath of each subquery
// and returns NodeName to Subquery Iterators map. This is to ensure Redfish
// Request is sent out once but the dataset obtained can be processed per
// Subquery using the mapped RedPathContext objects.
NodeNameToRedPathContexts DeduplicateNodeNamesAcrossSubqueries(
    std::vector<QueryPlanner::RedPathContext> &&redpath_context_multiple) {
  NodeNameToRedPathContexts node_to_redpath_contexts;
  for (auto &&redpath_context : redpath_context_multiple) {
    // Pair resource name and those RedPaths that have this resource as next
    // NodeName.
    std::string node_name = redpath_context.redpath_steps_iterator->first;
    node_to_redpath_contexts[node_name].push_back(redpath_context);
  }
  return node_to_redpath_contexts;
}

}  // namespace

// Normalize Redfish response per the property requirements in subquery.
absl::StatusOr<SubqueryDataSet *> SubqueryHandle::Normalize(
    const RedfishVariant &node, DelliciusQueryResult &result,
    SubqueryDataSet *parent_subquery_dataset) {
  ECCLESIA_ASSIGN_OR_RETURN(SubqueryDataSet subquery_dataset,
                            normalizer_->Normalize(node, subquery_));

  // Insert normalized data in the parent Subquery dataset if provided.
  // Otherwise, add the dataset in the query result.
  SubqueryOutput *subquery_output = nullptr;
  if (parent_subquery_dataset == nullptr) {
    subquery_output =
        &(*result.mutable_subquery_output_by_id())[subquery_.subquery_id()];
  } else {
    subquery_output = &(
        *parent_subquery_dataset
             ->mutable_child_subquery_output_by_id())[subquery_.subquery_id()];
  }
  auto *dataset = subquery_output->mutable_data_sets()->Add();
  if (dataset != nullptr) {
    *dataset = std::move(subquery_dataset);
  }
  return dataset;
}

void QueryPlanner::ExecuteRedPathStepFromEachSubquery(
    QueryExecutionContext &execution_context, DelliciusQueryResult &result,
    QueryTracker *tracker) {
  NodeNameToRedPathContexts node_name_to_redpath_contexts =
      DeduplicateNodeNamesAcrossSubqueries(
          std::move(execution_context.redpath_ctx_multiple));

  // Return if the Context Node is invalid or there is no redpath expression
  // left to process across subqueries.
  if (execution_context.redfish_object == nullptr ||
      node_name_to_redpath_contexts.empty()) {
    return;
  }

  // Maps RedPath of context node to the execution context. This mapping is used
  // to aggregate Subqueries operating on same context node along with any
  // metadata applicable to the context node.
  absl::flat_hash_map<std::string, QueryExecutionContext>
      redpath_to_execution_ctx;

  // Track last executed RedPath per indexed node. This differs from
  // last_executed_path which is either a singleton resource or collection
  std::string last_executed_index_redpath;

  // We will query NodeName from each NodeName to RedPathContexts pair and
  // apply predicate expressions from each RedPath to filter the nodes
  // to produce next set of context nodes where each context node will have a
  // similar NodeName to RedPath Contexts pairing.
  for (auto &[node_name, redpath_ctx_multiple] :
       node_name_to_redpath_contexts) {
    std::string last_executed_redpath = execution_context.last_executed_redpath;

    // Reference to allow capture in the PredicateRunner.
    std::vector<RedPathContext> &redpath_ctx_list_mapped_to_node =
        redpath_ctx_multiple;

    // Append the last executed RedPath to construct the next RedPath to query.
    absl::StrAppend(&last_executed_redpath, "/", node_name);
    GetParams get_params_for_redpath{};
    if (auto iter = query_params_.find(last_executed_redpath);
        iter != query_params_.end()) {
      get_params_for_redpath = iter->second;
    }

    // Dispatch Redfish Request for the Redfish Resource associated with the
    // NodeName expression.
    RedfishVariant node_set_as_variant = execution_context.redfish_object->Get(
        node_name, get_params_for_redpath);

    // Add last executed RedPath to the record.
    if (tracker) {
      tracker->redpaths_queried.insert(
          {last_executed_redpath, get_params_for_redpath});
    }

    // If NodeName does not resolve to a valid Redfish Resource, skip it!
    if (!node_set_as_variant.status().ok()) {
      continue;
    }
    // At this point we have executed redfish request for a NodeName that
    // results in a node-set which can be a singleton Redfish resource or
    // a Collection.
    // As we are batch processing subqueries, it is possible that a node in
    // this node-set can satisfy predicates across subqueries. Example
    // /Chassis[SKU=1234] and /Chassis[Name=Foo] could be the same chassis
    // instance /Chassis[1].
    // So instead of iterating over a node-set to apply predicate for each
    // subquery, we can batch process predicate expressions such that we iterate
    // the node-set once and for each node we apply all the predicate
    // expressions.
    //
    // On successful filter, map the subquery with the RedPath. The mapped
    // subqueries then use the Resource associated with the RedPath as context
    // node for executing next NodeName expression.
    // Example: {"/Chassis[1]" : {SQ1, SQ4, SQ5}},
    //           "/Chassis[4]" : {SQ1, SQ4, SQ9}}
    auto apply_predicate_from_each_subquery = [&](RedfishVariant node,
                                                  size_t node_index,
                                                  size_t node_set_size) {
      for (auto &redpath_ctx : redpath_ctx_list_mapped_to_node) {
        // On successfully refining the node-set using predicate,
        // either prepare subquery response or continue query with
        // subordinate resources of the refined node-set.
        if (!ApplyPredicateRule(node, node_index, node_set_size,
                                redpath_ctx.redpath_steps_iterator)) {
          continue;
        }

        auto &subquery_handle = redpath_ctx.subquery_handle;
        bool is_end_of_redpath =
            subquery_handle->IsEndOfRedPath(redpath_ctx.redpath_steps_iterator);

        // If there aren't any child subqueries and all step expressions in the
        // current SubqueryHandle's RedPath have been processed, we can proceed
        // to data normalization.
        if (is_end_of_redpath && !subquery_handle->HasChildSubqueries()) {
          subquery_handle
              ->Normalize(node, result, redpath_ctx.root_redpath_dataset)
              .IgnoreError();
        } else {
          // Prepare for Querying the next step expression in RedPath. The
          // context node for the next query operation will be the refined
          // node-set obtained after applying predicate expression.
          std::unique_ptr<RedfishObject> obj = node.AsObject();
          // Redfish Object must be valid to serve as context node.
          if (obj == nullptr) {
            continue;
          }
          auto query_plan_entry =
              redpath_to_execution_ctx.find(last_executed_index_redpath);

          // We will construct a new execution context using the
          // filtered Redfish Resource as context node if none exists.
          if (query_plan_entry == redpath_to_execution_ctx.end()) {
            QueryExecutionContext new_execution_context;
            new_execution_context.redfish_object = std::move(obj);
            new_execution_context.last_executed_redpath = last_executed_redpath;
            redpath_to_execution_ctx.emplace(last_executed_index_redpath,
                                             std::move(new_execution_context));
          }

          // Add all the current subquery handle to the list of Subquery
          // Handles that share the same context node for their next
          // redpath expression
          std::vector<RedPathContext> &redpath_contexts =
              redpath_to_execution_ctx[last_executed_index_redpath]
                  .redpath_ctx_multiple;
          if (is_end_of_redpath) {
            // All RedPath step expressions of current SubqueryHandle have been
            // processed. We can normalize the data to prepare the subquery
            // response.
            absl::StatusOr<SubqueryDataSet *> last_normalized_dataset;
            if (last_normalized_dataset = subquery_handle->Normalize(
                    node, result, redpath_ctx.root_redpath_dataset);
                !last_normalized_dataset.ok()) {
              continue;
            }

            // Since this SubqueryHandle has linked child SubqueryHandles,
            // we will insert all the child Handles in the execution context
            // to be executed using the new context node.
            for (auto &child_subquery_handle :
                 subquery_handle->GetChildSubqueryHandles()) {
              if (child_subquery_handle == nullptr) continue;
              redpath_contexts.push_back(
                  {child_subquery_handle, *last_normalized_dataset,
                   child_subquery_handle->GetRedPathIterator()});
            }
          } else {
            redpath_contexts.push_back(redpath_ctx);
            // Load next step expression in RedPath that will query the redfish
            // object in the new execution context serving as new context
            // node.
            ++redpath_contexts.back().redpath_steps_iterator;
          }
        }
      }
    };

    // Apply predicate expression rule on each Redfish Resource in collection.
    size_t node_count = 1;
    std::unique_ptr<RedfishIterable> iter = node_set_as_variant.AsIterable();
    if (iter != nullptr) {
      node_count = iter->Size();
      // As query planner is iterating over each resource in collection and
      // applying predicate expression from all subquery handles mapped to the
      // node, from tracker's perspective QueryPlanner is executing [*]. Hence
      // RedPath expressions in tracker will differ from 'last_executed_redpath'
      // in execution context which creates context for indexed nodes as well.
      absl::StrAppend(&last_executed_redpath, "[", kPredicateSelectAll, "]");
      if (tracker) {
        tracker->redpaths_queried.insert({last_executed_redpath, GetParams{}});
      }
      for (size_t index = 0; index < node_count; ++index) {
        absl::StrAppend(&last_executed_index_redpath, "[", index, "]");
        apply_predicate_from_each_subquery(node_set_as_variant[index], index,
                                           node_count);
      }
    } else {
      absl::StrAppend(&last_executed_index_redpath, "[", 0, "]");
      apply_predicate_from_each_subquery(std::move(node_set_as_variant), 0,
                                         node_count);
    }
  }
  // Now, for each new context node obtained after applying Predicates, execute
  // next RedPath Step expression from the mapped RedPath Iterators in the
  // execution context.
  for (auto &[_, qec] : redpath_to_execution_ctx) {
    ExecuteRedPathStepFromEachSubquery(qec, result, tracker);
  }
}

DelliciusQueryResult QueryPlanner::Run(const RedfishVariant &variant,
                                       const Clock &clock,
                                       QueryTracker *tracker) {
  DelliciusQueryResult result;
  auto timestamp = AbslTimeToProtoTime(clock.Now());
  if (timestamp.ok()) {
    *result.mutable_start_timestamp() = *std::move(timestamp);
  }
  result.set_query_id(plan_id_);
  if (auto obj = variant.AsObject()) {
    QueryExecutionContext execution_context{.redfish_object = std::move(obj)};
    for (auto &subquery_handle : subquery_handles_) {
      if (subquery_handle && subquery_handle->IsRootSubquery()) {
        execution_context.redpath_ctx_multiple.push_back(
            {subquery_handle.get(), nullptr,
             subquery_handle->GetRedPathIterator()});
      }
    }
    ExecuteRedPathStepFromEachSubquery(execution_context, result, tracker);
  }
  timestamp = AbslTimeToProtoTime(clock.Now());
  if (timestamp.ok()) {
    *result.mutable_end_timestamp() = *std::move(timestamp);
  }
  return result;
}

}  // namespace ecclesia
