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
    R"(^([a-zA-Z#@][0-9a-zA-Z.\\]*)(?:(!=|>|<|=|>=|<=))([a-zA-Z0-9._#]+)$)"};

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
bool PredicateFilterByNodeComparison(const RedfishObject &redfish_object,
                                     absl::string_view predicate) {
  std::string node_name, op, test_value;
  bool ret = false;
  if (RE2::FullMatch(predicate, *kPredicateRegexRelationalOperator, &node_name,
                     &op, &test_value)) {
    double value;
    auto json_obj = ResolveNodeNameToJsonObj(redfish_object, node_name);
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
// Checks if given Redfish Resource contains predicate string.
bool PredicateFilterByNodeName(const RedfishObject &redfish_object,
                               absl::string_view predicate) {
  std::vector<std::string> node_names = SplitNodeNameForNestedNodes(predicate);
  if (node_names.empty()) {
    return false;
  }
  nlohmann::json leaf = redfish_object.GetContentAsJson();
  for (auto const &name : node_names) {
    if (!leaf.contains(name)) {
      return false;
    }
    leaf = leaf.at(name);
  }
  return true;
}

bool ApplyPredicateRule(const RedfishObject &redfish_object, size_t node_index,
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
      single_predicate_result =
          PredicateFilterByNodeComparison(redfish_object, expr);
    } else {
      // Filter node-set by NodeName
      single_predicate_result = PredicateFilterByNodeName(redfish_object, expr);
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

absl::StatusOr<SubqueryDataSet *> SubqueryHandle::Normalize(
    const RedfishObject &redfish_object, DelliciusQueryResult &result,
    SubqueryDataSet *parent_subquery_dataset) {
  ECCLESIA_ASSIGN_OR_RETURN(SubqueryDataSet subquery_dataset,
                            normalizer_->Normalize(redfish_object, subquery_));

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
    ContextNode &context_node, DelliciusQueryResult &result,
    QueryTracker *tracker) {
  // Return if the Context Node does not contain a valid RedfishObject.
  if (context_node.redfish_object == nullptr) {
    return;
  }

  // First, we will pull NodeName from each RedPath expression across subqueries
  // and then deduplicate them to get a map between NodeName and RedPaths that
  // have the NodeName in common.
  // Recall that in a RedPath "/Chassis[*]/Sensors[*]", RedPath steps are
  // "Chassis[*]" and "Sensors[*]" and NodeName expressions are "Chassis" and
  // "Sensors".
  NodeNameToRedPathContexts node_name_to_redpath_contexts =
      DeduplicateNodeNamesAcrossSubqueries(
          std::move(context_node.redpath_ctx_multiple));

  // Return if there is no redpath expression left to process across subqueries.
  if (node_name_to_redpath_contexts.empty()) {
    return;
  }

  // Set of Nodes that are obtained by executing RedPath expressions relative to
  // Redfish Object encapsulated in given 'context_node'.
  std::vector<ContextNode> context_nodes;

  // We will query each NodeName in NodeName to RedPath context map created
  // above and apply predicate expressions from each RedPath to filter the nodes
  // that produces next set of context nodes.
  for (auto &[node_name, redpath_ctx_multiple] :
       node_name_to_redpath_contexts) {
    std::string last_executed_redpath = context_node.last_executed_redpath;

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
    RedfishVariant node_set_as_variant = context_node.redfish_object->Get(
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
    auto apply_predicate_from_each_subquery = [&](std::unique_ptr<RedfishObject>
                                                      redfish_object,
                                                  size_t node_index,
                                                  size_t node_set_size) {
      if (redfish_object == nullptr) return;
      ContextNode new_context_node = {
          .redfish_object = std::move(redfish_object),
          .last_executed_redpath = last_executed_redpath};
      for (auto &redpath_ctx : redpath_ctx_list_mapped_to_node) {
        if (!ApplyPredicateRule(*new_context_node.redfish_object, node_index,
                                node_set_size,
                                redpath_ctx.redpath_steps_iterator)) {
          continue;
        }
        // The Redfish object meets the predicate criteria of the RedPath
        // expression. Now check whether the query needs to continue or we need
        // to prepare the response if we have reached end of RedPath expression.
        auto &subquery_handle = redpath_ctx.subquery_handle;
        bool is_end_of_redpath =
            subquery_handle->IsEndOfRedPath(redpath_ctx.redpath_steps_iterator);

        // If there aren't any child subqueries and all step expressions in the
        // current RedPath context have been processed, we can proceed to data
        // normalization.
        if (is_end_of_redpath && !subquery_handle->HasChildSubqueries()) {
          subquery_handle
              ->Normalize(*new_context_node.redfish_object, result,
                          redpath_ctx.root_redpath_dataset)
              .IgnoreError();
        } else {
          if (is_end_of_redpath) {
            absl::StatusOr<SubqueryDataSet *> last_normalized_dataset;
            if (last_normalized_dataset = subquery_handle->Normalize(
                    *new_context_node.redfish_object, result,
                    redpath_ctx.root_redpath_dataset);
                !last_normalized_dataset.ok()) {
              continue;
            }

            // Since this SubqueryHandle has linked child SubqueryHandles,
            // we will insert all the RedPath contexts in the list tracked for
            // the new context node.
            for (auto &child_subquery_handle :
                 subquery_handle->GetChildSubqueryHandles()) {
              if (child_subquery_handle == nullptr) continue;
              new_context_node.redpath_ctx_multiple.push_back(
                  {child_subquery_handle, *last_normalized_dataset,
                   child_subquery_handle->GetRedPathIterator()});
            }
          } else {
            new_context_node.redpath_ctx_multiple.push_back(redpath_ctx);
            // Load next step expression in the RedPath that will execute
            // relative to the new context node.
            ++new_context_node.redpath_ctx_multiple.back()
                  .redpath_steps_iterator;
          }
        }
      }
      context_nodes.push_back(std::move(new_context_node));
    };

    // Apply predicate expression rule on each Redfish Resource in collection.
    size_t node_count = 1;
    std::unique_ptr<RedfishIterable> iter = node_set_as_variant.AsIterable();
    if (iter != nullptr) {
      node_count = iter->Size();
      // As query planner is iterating over each resource in collection and
      // applying predicate expression from all subquery handles mapped to the
      // node, from tracker's perspective QueryPlanner is executing [*].
      absl::StrAppend(&last_executed_redpath, "[", kPredicateSelectAll, "]");
      if (tracker) {
        tracker->redpaths_queried.insert({last_executed_redpath, GetParams{}});
      }
      for (size_t index = 0; index < node_count; ++index) {
        RedfishVariant indexed_node = node_set_as_variant[index];
        if (!indexed_node.status().ok()) continue;
        apply_predicate_from_each_subquery(indexed_node.AsObject(), index,
                                           node_count);
      }
    } else {
      apply_predicate_from_each_subquery(node_set_as_variant.AsObject(), 0,
                                         node_count);
    }
  }
  // Now, for each new Context node obtained after applying Predicate expression
  // from all RedPath expressions, execute next RedPath Step expression from
  // every RedPath context mapped to the context node.
  for (auto &qec : context_nodes) {
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
    ContextNode context_node{.redfish_object = std::move(obj)};
    for (auto &subquery_handle : subquery_handles_) {
      if (subquery_handle && subquery_handle->IsRootSubquery()) {
        context_node.redpath_ctx_multiple.push_back(
            {subquery_handle.get(), nullptr,
             subquery_handle->GetRedPathIterator()});
      }
    }
    ExecuteRedPathStepFromEachSubquery(context_node, result, tracker);
  }
  timestamp = AbslTimeToProtoTime(clock.Now());
  if (timestamp.ok()) {
    *result.mutable_end_timestamp() = *std::move(timestamp);
  }
  return result;
}

}  // namespace ecclesia
