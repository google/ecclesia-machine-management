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
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/interface.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/time/proto.h"
#include "re2/re2.h"

namespace ecclesia {

namespace {

using SubqueryHandle = QueryPlanner::SubqueryHandle;
using RedPathStep = std::pair<std::string, std::string>;

// Regex definitions to identify LocationStep expression and extract NodeName,
// Predicate and Filter Expressions.
// Note: Regex patterns here are not meant to detect malformed RedPaths.
//
// Pattern for location step: NodeName[Predicate]
constexpr LazyRE2 kLocationStepRegex = {
    "^([a-zA-Z#@][0-9a-zA-Z.]+)(?:\\[(.*?)\\]|)$"};
// Pattern for predicate formatted with relational operators:
constexpr LazyRE2 kPredicateRegexRelationalOperator = {
    "^([a-zA-Z#@][0-9a-zA-Z.]*)(?:(!=|>|<|=|>=|<=))([a-zA-Z0-9._#]+)$"};

constexpr absl::string_view kPredicateSelectAll = "*";
constexpr absl::string_view kPredicateSelectLastIndex = "last()";
constexpr absl::string_view kBinaryOperandTrue = "true";
constexpr absl::string_view kBinaryOperandFalse = "false";
// Supported relational operators
constexpr std::array<const char *, 6> kRelationsOperators = {
    "<", ">", "!=", ">=", "<=", "="};

// Creates RedPathStep objects from the given RedPath string.
absl::StatusOr<std::vector<RedPathStep>> RedPathToSteps(
    absl::string_view redpath) {
  std::vector<RedPathStep> steps;
  for (absl::string_view step_expression :
       absl::StrSplit(redpath, '/', absl::SkipEmpty())) {
    std::string node_name, predicate;
    if (!RE2::FullMatch(step_expression, *kLocationStepRegex, &node_name,
                        &predicate)) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Cannot parse Step expression %s in RedPath %s",
                          step_expression, redpath));
    }
    steps.push_back({node_name, predicate});
  }
  return steps;
}

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

// Gets all the node names in the given expression.
// parent.child.grandchild -> {parent, child, grandchild}
std::vector<std::string> GetNodeNamesFromChain(absl::string_view node_name) {
  std::vector<std::string> nodes;
  if (auto pos = node_name.find("@odata."); pos != std::string::npos) {
    std::string odata_str = std::string(node_name.substr(pos));
    if (absl::StrContains(odata_str, "@odata.id")) return nodes;
    nodes = absl::StrSplit(node_name.substr(0, pos), '.', absl::SkipEmpty());
    nodes.push_back(std::move(odata_str));
  } else {
    nodes = absl::StrSplit(node_name, '.', absl::SkipEmpty());
  }
  return nodes;
}

// Helper function to resolve node_name for nested nodes if any and return json
// object to be evaluated for required property.
absl::StatusOr<nlohmann::json> GetJsonObjectFromNodeName(
    const RedfishVariant &variant, absl::string_view node_name) {
  std::vector<std::string> node_names = GetNodeNamesFromChain(node_name);
  if (node_names.empty()) {
    return absl::InternalError("Cannot parse any node from given NodeName");
  }
  nlohmann::json json_obj = variant.AsObject()->GetContentAsJson();
  // If given expression has multiple nodes, we need to return the json object
  // associated with the leaf node.
  for (auto const &name : node_names) {
    if (!json_obj.contains(name)) {
      return absl::InternalError(
          absl::StrFormat("Node %s not found in json object", name));
    }
    json_obj = json_obj.at(name);
  }
  return json_obj;
}

// Handler for predicate expressions containing relational operators.
bool PredicateFilterByNodeComparison(const RedfishVariant &variant,
                                     absl::string_view predicate) {
  std::string node_name, op, test_value;
  bool ret = false;
  if (RE2::FullMatch(predicate, *kPredicateRegexRelationalOperator, &node_name,
                     &op, &test_value)) {
    double value;
    auto json_obj = GetJsonObjectFromNodeName(variant, node_name);
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
  std::vector<std::string> node_names = GetNodeNamesFromChain(predicate);
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

using NodeNameToSubqueryHandles =
    absl::flat_hash_map<std::string, std::vector<SubqueryHandle>>;

// Deduplicates the next NodeName expression in the RedPath of each subquery
// and returns NodeName to SubqueryHandle map. This is to ensure Redfish Request
// is sent out once but the dataset obtained can be processed per Subquery
// using the mapped SubqueryHandle objects.
NodeNameToSubqueryHandles DeduplicateNodeNamesAcrossSubqueries(
    const std::vector<SubqueryHandle> &subquery_handles) {
  NodeNameToSubqueryHandles node_to_subquery;
  for (const auto &subquery_handle : subquery_handles) {
    // Pair resource name and those subqueries that have this resource as next
    // element in their respective RedPaths
    auto node_name = subquery_handle.GetNodeNameFromRedPathStep();
    if (node_name != std::nullopt) {
      node_to_subquery[*node_name].push_back(subquery_handle);
    }
  }
  return node_to_subquery;
}

}  // namespace

SubqueryHandle::SubqueryHandle(const DelliciusQuery::Subquery &subquery,
                               Normalizer *normalizer)
    : subquery_(subquery), normalizer_(normalizer) {
  is_redpath_valid_ = false;
  auto steps = RedPathToSteps(subquery.redpath());
  if (!steps.ok()) {
    LOG(ERROR) << steps.status();
    return;
  }
  steps_in_redpath_ = *std::move(steps);
  // An iterator is configured to traverse the Step expressions in subquery's
  // RedPath as redfish requests are dispatched.
  iter_ = steps_in_redpath_.begin();
  is_redpath_valid_ = true;
}

std::optional<std::string> SubqueryHandle::GetNodeNameFromRedPathStep() const {
  if (is_redpath_valid_) return iter_->first;
  return std::nullopt;
}

// Normalize redfish response per the property requirements in
// subquery.
void SubqueryHandle::PrepareSubqueryOutput(const RedfishVariant &node,
                                           DelliciusQueryResult &result) {
  absl::StatusOr<SubqueryDataSet> normalized_data =
      normalizer_->Normalize(node, subquery_);
  if (normalized_data.ok()) {
    // Populate the response data in subquery output for the given id.
    auto *subquery_output = result.mutable_subquery_output_by_id();
    (*subquery_output)[subquery_.subquery_id()].mutable_data_sets()->Add(
        std::move(normalized_data.value()));
  }
}

// Normalizes dataset from filtered node if there are no more RedPathSteps left
// else updates URI to Context node mapping with current subquery.
bool SubqueryHandle::LoadNextRedPathStep() {
  // If iterator reached end of RedPath
  if (!steps_in_redpath_.empty() &&
      iter_->first == steps_in_redpath_.back().first) {
    return false;
  }
  ++iter_;
  return true;
}

bool SubqueryHandle::ApplyPredicateRule(const RedfishVariant &node,
                                        size_t node_index,
                                        size_t node_set_size) {
  std::string_view predicate = iter_->second;
  size_t num;
  // If '[last()]' predicate expression, check if current node at last index.
  if ((predicate == kPredicateSelectLastIndex &&
       node_index == node_set_size - 1) ||
      // If '[Index]' predicate expression, check if current node at given index
      (absl::SimpleAtoi(predicate, &num) && num == node_index) ||
      // If '[*]' predicate expression or empty predicate, no filter required.
      (predicate.empty() || predicate == kPredicateSelectAll)) {
    return true;
  }

  // Boolean tracking the status of filter operation using predicate expression
  // rule.
  bool is_filter_success = false;
  if (std::any_of(
          kRelationsOperators.begin(), kRelationsOperators.end(),
          [&](const char *op) { return absl::StrContains(predicate, op); })) {
    // Look for predicate expression containing relational operators.
    is_filter_success = PredicateFilterByNodeComparison(node, predicate);
  } else {
    // Filter node-set by NodeName
    is_filter_success = PredicateFilterByNodeName(node, predicate);
  }
  return is_filter_success;
}

void QueryPlanner::ExecuteRedPathStepFromEachSubquery(
    QueryExecutionContext &execution_context, DelliciusQueryResult &result,
    QueryTracker *tracker) {
  NodeNameToSubqueryHandles node_name_to_subquery =
      DeduplicateNodeNamesAcrossSubqueries(execution_context.subquery_handles);
  // Return if the Context Node is invalid or there is no redpath expression
  // left to process across subqueries.
  if (execution_context.redfish_object == nullptr ||
      node_name_to_subquery.empty()) {
    return;
  }

  // Maps RedPath of context node to the execution context. This mapping is used
  // to aggregate SubqueryHandle objects operating on same context node along
  // with any metadata applicable to the context node.
  absl::flat_hash_map<std::string, QueryExecutionContext>
      redpath_to_execution_ctx;

  // Track last executed RedPath per indexed node. This differs from
  // last_executed_path which is either a singleton resource or collection
  std::string last_executed_index_redpath;

  // We will query NodeName from each NodeName to Subquery Handles pair and
  // apply predicate expressions from each SubqueryHandle to filter the nodes
  // to produce next set of context nodes where each context node will have a
  // similar NodeName to Subquery handles pairing.
  for (auto &[node_name, handles] : node_name_to_subquery) {
    std::string last_executed_redpath = execution_context.last_executed_redpath;
    // Reference to allow capture in the PredicateRunner.
    std::vector<SubqueryHandle> &subquery_handles_mapped_to_node = handles;
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
      for (auto subquery_handle : subquery_handles_mapped_to_node) {
        // On successfully refining the node-set using predicate,
        // either prepare subquery response or continue query with
        // subordinate resources of the refined node-set.
        if (!subquery_handle.ApplyPredicateRule(node, node_index,
                                                node_set_size)) {
          continue;
        }
        // If we have reached end of RedPath, populate subquery response data.
        if (!subquery_handle.LoadNextRedPathStep()) {
          subquery_handle.PrepareSubqueryOutput(node, result);
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
          redpath_to_execution_ctx[last_executed_index_redpath]
              .subquery_handles.push_back(std::move(subquery_handle));
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
  // next RedPath Step expression from the mapped subqueries.
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
    QueryExecutionContext execution_context{
        .redfish_object = std::move(obj),
        .subquery_handles = subquery_handles_};
    ExecuteRedPathStepFromEachSubquery(execution_context, result, tracker);
  }
  timestamp = AbslTimeToProtoTime(clock.Now());
  if (timestamp.ok()) {
    *result.mutable_end_timestamp() = *std::move(timestamp);
  }
  return result;
}

QueryPlanner::QueryPlanner(const DelliciusQuery &query,
                           RedPathRedfishQueryParams query_params,
                           Normalizer *normalizer)
    : plan_id_(query.query_id()), query_params_(std::move(query_params)) {
  // Create SubqueryHandle for each Subquery in a RedPath Query.
  for (const auto &subquery : query.subquery()) {
    auto subquery_handle = SubqueryHandle(subquery, normalizer);
    // If SubqueryHandle instantiation fails for some reason, QueryPlanner will
    // not be able to fetch the first NodeName in Subquery's RedPath.
    // This however is not a fatal error and the QueryPlanner will continue
    // to create SubqueryHandles for remaining Subqueries.
    if (subquery_handle.GetNodeNameFromRedPathStep() == std::nullopt) continue;
    subquery_handles_.push_back(std::move(subquery_handle));
  }
}

}  // namespace ecclesia
