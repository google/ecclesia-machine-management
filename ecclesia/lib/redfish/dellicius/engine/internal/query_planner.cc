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
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/interface.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/time/proto.h"
#include "single_include/nlohmann/json.hpp"
#include "re2/re2.h"

namespace ecclesia {

namespace {

using SubqueryHandle = QueryPlanner::SubqueryHandle;
using RedPathStep = SubqueryHandle::RedPathStep;
using ContextNodeToSubqueryHandles =
    SubqueryHandle::ContextNodeToSubqueryHandles;
using FilterContext = SubqueryHandle::FilterContext;

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
      LOG(ERROR) << json_obj.status();
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
    std::vector<SubqueryHandle> &subquery_handles) {
  NodeNameToSubqueryHandles node_to_subquery;
  for (auto &subquery_handle : subquery_handles) {
    // Pair resource name and those subqueries that have this resource as next
    // element in their respective RedPaths
    auto node_name = subquery_handle.GetNodeNameFromRedPathStep();
    if (node_name != std::nullopt) {
      node_to_subquery[*node_name].push_back(subquery_handle);
    }
  }
  return node_to_subquery;
}

// A helper utility to iterate over a Collection and Singleton Resource.
// The callback apply_predicate_from_each_subquery is responsible for applying
// Predicate Expression rules from each subquery using Redfish Resource
// encapsulated in the variant as the context node. This callback has a
// signature:
//     void f(SubqueryHandle::FilterContext context)
//         args: FilterContext {
//                 RedfishVariant: Redfish Resource used as context node.
//                 node_index: The specific instance of a Resource in Collection
//                             or 0 if the resource is Singleton
//                 node_set_size: Resource Collection size; 1 if Singleton.
//               }
template <typename Callback>
void PredicateRunner(RedfishVariant &&var,
                     Callback &&apply_predicate_from_each_subquery) {
  auto iter = var.AsIterable();
  size_t node_index = 0, node_count = 1;
  if (iter != nullptr) {
    // Iterate over each Resource in Collection and invoke predicate handler
    node_count = iter->Size();
    for (size_t index = 0; index < node_count; ++index) {
      apply_predicate_from_each_subquery(
          {std::move(var[index]), index, node_count});
    }
  } else {
    apply_predicate_from_each_subquery(
        {std::move(var), node_index, node_count});
  }
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
  if (is_redpath_valid_) return iter_->node_name;
  return std::nullopt;
}

// Normalizes dataset from filtered node if there are no more RedPathSteps left
// else updates URI to Context node mapping with current subquery.
void SubqueryHandle::NormalizeOrContinueQuery(
    const RedfishVariant &node,
    ContextNodeToSubqueryHandles &context_node_to_sq,
    DelliciusQueryResult &result) {
  // If iterator reached end of RedPath
  if (!steps_in_redpath_.empty() &&
      iter_->node_name == steps_in_redpath_.back().node_name) {
    // Normalize redfish response per the property requirements in
    // subquery.
    absl::StatusOr<SubqueryDataSet> normalized_data =
        normalizer_->Normalize(node, subquery_);
    if (normalized_data.ok()) {
      // Populate the response data in subquery output for the given id.
      auto *subquery_output = result.mutable_subquery_output_by_id();
      (*subquery_output)[subquery_.subquery_id()].mutable_data_sets()->Add(
          std::move(normalized_data.value()));
    }
  } else if (std::unique_ptr<RedfishObject> obj = node.AsObject()) {
    // Update Context Node URI to SubqueryHandle map to share context node
    // across subqueries.
    std::optional<std::string> maybe_uri = obj->GetUriString();
    // Objects that don't have a URI cannot serve as context node.
    // To reference into such objects, a Query needs to use property path
    // instead of using RedPath NodeName expressions.
    if (!maybe_uri.has_value()) return;
    auto uri_to_subqueries_pair = context_node_to_sq.find(*maybe_uri);
    // Since iterator has not reached the end of RedPath, increment the iterator
    // to point to next RedPath Step expression.
    ++iter_;
    // If URI exists in the map, add redfish object and subquery handle.
    if (uri_to_subqueries_pair != context_node_to_sq.end()) {
      // Insert the Redfish Object to be used as context node.
      uri_to_subqueries_pair->second.first = std::move(obj);
      // Insert this SubqueryHandle.
      uri_to_subqueries_pair->second.second.push_back(*this);
    } else {
      // If URI of the new context node does not exist, insert a new URI and
      // SubqueryHandle pair.
      std::vector<SubqueryHandle> context_node_sq_handles = {*this};
      context_node_to_sq[*std::move(maybe_uri)] = {std::move(obj),
                                                   context_node_sq_handles};
    }
    // Decrement the iterator to allow the same subquery handle to be tested for
    // next context node in the iterable.
    --iter_;
  }
}

bool SubqueryHandle::ApplyPredicateRule(const FilterContext &filter_context) {
  std::string_view predicate = iter_->predicate;
  size_t num;
  // If '[last()]' predicate expression, check if current node at last index.
  if ((predicate == kPredicateSelectLastIndex &&
       filter_context.node_index == filter_context.node_set_size - 1) ||
      // If '[Index]' predicate expression, check if current node at given index
      (absl::SimpleAtoi(predicate, &num) && num == filter_context.node_index) ||
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
    is_filter_success =
        PredicateFilterByNodeComparison(filter_context.node, predicate);
  } else {
    // Filter node-set by NodeName
    is_filter_success =
        PredicateFilterByNodeName(filter_context.node, predicate);
  }
  return is_filter_success;
}

void QueryPlanner::ExecuteRedPathStepFromEachSubquery(
    const RedfishObject *redfish_object,
    std::vector<SubqueryHandle> &subquery_handles,
    DelliciusQueryResult &result) {
  ContextNodeToSubqueryHandles context_node_to_sq;
  NodeNameToSubqueryHandles node_name_to_subquery =
      DeduplicateNodeNamesAcrossSubqueries(subquery_handles);
  if (node_name_to_subquery.empty()) return;
  for (auto &[node_name, handles] : node_name_to_subquery) {
    // Reference to allow capture in the PredicateRunner.
    std::vector<SubqueryHandle> &subquery_handles_mapped_to_node = handles;
    // Dispatch Redfish Request for the Redfish Resource associated with the
    // NodeName expression.
    auto node_set = redfish_object->Get(node_name);
    // If NodeName does not resolve to a valid Redfish Resource, skip it!
    if (node_set.AsObject() == nullptr && node_set.AsIterable() == nullptr) {
      continue;
    }
    // At this point we have executed redfish request for a NodeName that
    // results in a node-set which can be a singleton Redfish resource or
    // a Collection.
    // As we are batch processing subqueries, it is possible that a node in
    // this node-set can satisfy predicates across subqueries. Example
    // /Chassis[SKU=1234] and /Chassis[Name=Foo] could be the same chassis
    // instance /redfish/v1/Chassis/1U.
    // So instead of applying predicate expression individually on the node-set
    // we can batch process predicate expressions so that we iterate the
    // node-set once.
    //
    // On successful filter, map the subquery with the URI. The mapped
    // subqueries can then use the Resource associated with the URI as context
    // node for executing next NodeName expression.
    // Example: {"/redfish/v1/Chassis/1U" : {SQ1, SQ4, SQ5}},
    //           "/redfish/v1/Chassis/4U" : {SQ1, SQ4, SQ9}}
    PredicateRunner(std::move(node_set), [&](FilterContext filter_context) {
      for (auto &subquery_handle : subquery_handles_mapped_to_node) {
        if (subquery_handle.ApplyPredicateRule(filter_context)) {
          subquery_handle.NormalizeOrContinueQuery(filter_context.node,
                                                   context_node_to_sq, result);
        }
      }
    });
  }
  // Now, for each new context node obtained after applying Predicates, execute
  // next RedPath Step expression from the mapped subqueries.
  for (auto &[uri, redfish_obj_and_sq_handles] : context_node_to_sq) {
    ExecuteRedPathStepFromEachSubquery(redfish_obj_and_sq_handles.first.get(),
                                       redfish_obj_and_sq_handles.second,
                                       result);
  }
}

void QueryPlanner::Run(const RedfishVariant &variant, const Clock &clock,
                       DelliciusQueryResult &result) {
  auto timestamp = AbslTimeToProtoTime(clock.Now());
  if (timestamp.ok()) {
    *result.mutable_start_timestamp() = *std::move(timestamp);
  }
  result.set_query_id(plan_id_);
  if (auto obj = variant.AsObject()) {
    ExecuteRedPathStepFromEachSubquery(obj.get(), subquery_handles_, result);
  }
  timestamp = AbslTimeToProtoTime(clock.Now());
  if (timestamp.ok()) {
    *result.mutable_end_timestamp() = *std::move(timestamp);
  }
}

QueryPlanner::QueryPlanner(const DelliciusQuery &query, Normalizer *normalizer)
    : plan_id_(query.query_id()) {
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
