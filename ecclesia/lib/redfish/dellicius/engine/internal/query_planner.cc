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

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "google/rpc/code.pb.h"
#include "google/rpc/status.pb.h"
#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/functional/function_ref.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ecclesia/lib/redfish/transport/transport_metrics.pb.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/interface.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_errors.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_variables.pb.h"
#include "ecclesia/lib/redfish/dellicius/utils/path_util.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/status/macros.h"
#include "ecclesia/lib/time/clock.h"
#include "ecclesia/lib/time/proto.h"
#include "re2/re2.h"

namespace ecclesia {

namespace {
using ecclesia::RedfishInterface;

// Pattern for predicate formatted with relational operators:
constexpr LazyRE2 kPredicateRegexRelationalOperator = {
    R"(^([a-zA-Z#@][0-9a-zA-Z.\\]*)(?:(!=|>|<|=|>=|<=))([a-zA-Z0-9._\-\:#\\ ]+)$)"};

// Pattern for location step: NodeName[Predicate]
constexpr LazyRE2 kLocationStepRegex = {
    "^([a-zA-Z#@][0-9a-zA-Z.]+|)(?:\\[(.*?)\\]|)$"};

// Pattern for Redfish standard (ISO 8601) datetime string.
constexpr LazyRE2 kRedfishDatetimeRegex = {
    R"(^(?:[1-9]\d{3}-(?:(?:0[1-9]|1[0-2])-(?:0[1-9]|1\d|2[0-8])|(?:0[13-9]|1[0-2])-(?:29|30)|(?:0[13578]|1[02])-31)|(?:[1-9]\d(?:0[48]|[2468][048]|[13579][26])|(?:[2468][048]|[13579][26])00)-02-29)T(?:[01]\d|2[0-3]):[0-5]\d:[0-5]\d(?:\.\d{1,9})?(?:Z|[+-][01]\d:[0-5]\d)?$)"};

// All RedPath expressions execute relative to service root identified by '/'.
constexpr absl::string_view kServiceRootNode = "/";

// Known predicate expressions.
constexpr absl::string_view kPredicateSelectAll = "*";
constexpr absl::string_view kPredicateSelectLastIndex = "last()";
constexpr absl::string_view kBinaryOperandTrue = "true";
constexpr absl::string_view kBinaryOperandFalse = "false";
constexpr absl::string_view kLogicalOperatorAnd = "and";
constexpr absl::string_view kLogicalOperatorOr = "or";
// Supported relational operators
constexpr std::array<const char *, 6> kRelationsOperators = {
    "<", ">", "!=", ">=", "<=", "="};

// Matchers for user supplied datetime formats in a predicate.
constexpr absl::string_view kRedfishDatetimePlusOffset =
    "%Y-%m-%dT%H:%M:%E6S%Ez";
constexpr absl::string_view kRedfishDatetimeNoOffset = "%Y-%m-%dT%H:%M:%E6S";

using RedPathStep = std::pair<std::string, std::string>;
using RedPathIterator =
    std::vector<std::pair<std::string, std::string>>::const_iterator;

// Fetch a Redfish resource using the redfish_interface
ecclesia::RedfishVariant FetchUri(RedfishInterface *redfish_interface,
                                  absl::string_view uri,
                                  ecclesia::GetParams params = {}) {
  if (redfish_interface == nullptr) {
    return RedfishVariant(absl::InternalError("Null redfish interface"));
  }

  return params.freshness == ecclesia::GetParams::Freshness::kRequired
           ? redfish_interface->UncachedGetUri(uri, std::move(params))
           : redfish_interface->CachedGetUri(uri, std::move(params));
}

// Set the Timestamp object from the given clock
void SetTime(const Clock &clock, google::protobuf::Timestamp &field) {
  auto time = clock.Now();
  if (auto timestamp = AbslTimeToProtoTime(time); timestamp.ok()) {
    field = *std::move(timestamp);
  }
}

// Creates RedPathStep objects from the given RedPath string.
absl::StatusOr<std::vector<RedPathStep>> RedPathToSteps(
    absl::string_view redpath) {
  std::vector<RedPathStep> steps;

  // When queried node is service root itself.
  if (redpath == kServiceRootNode) {
    steps.push_back({});
    return steps;
  }

  for (absl::string_view step_expression :
       absl::StrSplit(redpath, '/', absl::SkipEmpty())) {
    std::string node_name, predicate;
    if (!RE2::FullMatch(step_expression, *kLocationStepRegex, &node_name,
                        &predicate) ||
        node_name.empty()) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Cannot parse Step expression %s in RedPath %s",
                          step_expression, redpath));
    }
    steps.push_back({node_name, predicate});
  }
  return steps;
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
    std::string node_name, predicate;
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

GetParams GetQueryParamsForRedPath(
    const RedPathRedfishQueryParams &redpath_to_query_params,
    absl::string_view redpath_prefix) {
  // Set default GetParams value as follows:
  //   Default freshness is Optional
  //   Default Redfish query parameter setting is no expand!
  auto params = GetParams{.freshness = GetParams::Freshness::kOptional,
                          .expand = std::nullopt};

  // Get RedPath specific configuration for expand and freshness
  if (auto iter = redpath_to_query_params.find(redpath_prefix);
      iter != redpath_to_query_params.end()) {
    params = iter->second;
  }
  return params;
}

// Returns combined GetParams{} for RedPath expressions in the query.
// There are 2 places where we get parameters associated with RedPath prefix:
// 1) Expand configuration from embedded query_rule file
// 2) Freshness configuration in the Query itself
// In this function, we merge Freshness requirement with expand configuration
// for the redpath prefix.
RedPathRedfishQueryParams CombineQueryParams(
    const DelliciusQuery &query,
    RedPathRedfishQueryParams redpath_to_query_params) {
  for (const auto &subquery : query.subquery()) {
    if (subquery.freshness() != DelliciusQuery::Subquery::REQUIRED) {
      continue;
    }

    absl::string_view redpath_str = subquery.redpath();
    std::string redpath_formatted = std::string(redpath_str);
    if (!absl::StartsWith(redpath_str, "/")) {
      redpath_formatted = "/";
      absl::StrAppend(&redpath_formatted, redpath_str);
    }

    auto iter = redpath_to_query_params.find(redpath_formatted);
    if (iter != redpath_to_query_params.end()) {
      iter->second.freshness = GetParams::Freshness::kRequired;
    } else {
      redpath_to_query_params.insert(
          {redpath_formatted,
           GetParams{.freshness = GetParams::Freshness::kRequired}});
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

// Helper function is used to ensure the obtained value equal or not equal to
// a non-number value.
template <typename F>
bool ApplyStringComparisonFilter(F filter_condition,
                                 absl::string_view inequality_string) {
  if (inequality_string == "!=") {
    return !filter_condition();
  }
  return filter_condition();
}

// Helper function used to validates two timestamp strings are in line with the
// expected redfish standard and applies a given comparator.
bool ApplyDateTimeComparisonFilter(
    const std::function<bool(absl::Time, absl::Time)> &time_comparator,
    const std::string &lhs_time_str, absl::string_view test_value) {
  absl::Time test_time, lhs_time;
  // Parse the user supplied timestamp into the desired format.
  if (!absl::ParseTime(kRedfishDatetimeNoOffset, test_value, &test_time,
                       /*err=*/nullptr)) {
    LOG(ERROR) << "Failed to parse " << test_value
               << " into a valid time string, expected format is "
               << kRedfishDatetimeNoOffset;

    return false;
  }
  // Parse the timestamp from the Redfish property into the desired format.
  if (!absl::ParseTime(kRedfishDatetimePlusOffset, lhs_time_str, &lhs_time,
                       /*err=*/nullptr)) {
    LOG(ERROR) << "Failed to parse redfish property " << lhs_time_str
               << " into a valid time string, expected format is "
               << kRedfishDatetimePlusOffset;

    return false;
  }
  return time_comparator(lhs_time, test_time);
}

bool IsDateTimeString(absl::string_view test_value) {
  return RE2::FullMatch(test_value, *kRedfishDatetimeRegex);
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
    if (IsDateTimeString(test_value)) {
      const auto time_condition = [&op](absl::Time lhs_time,
                                        absl::Time test_time) {
        if (op == ">=") return lhs_time >= test_time;
        if (op == ">") return lhs_time > test_time;
        if (op == "<=") return lhs_time <= test_time;
        if (op == "<") return lhs_time < test_time;
        if (op == "!=") return lhs_time != test_time;
        if (op == "=") return lhs_time == test_time;
        return false;
      };
      return json_obj->is_string()
                 ? ApplyDateTimeComparisonFilter(
                       time_condition, json_obj->get<std::string>(), test_value)
                 : false;
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
      const auto condition = [json_obj, &test_value]() {
        absl::StrReplaceAll({{"\\", ""}}, &test_value);
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
                        size_t node_set_size, const RedPathIterator &iter) {
  absl::string_view predicate = iter->second;
  if (predicate.empty()) return false;

  absl::string_view logical_operation = kLogicalOperatorAnd;
  bool is_filter_success = true;
  std::vector<absl::string_view> expressions =
      SplitExprByDelimiterWithEscape(predicate, " ", '\\');
  for (std::string_view expr : expressions) {
    // If expression is a logical operator, capture it and move to next
    // expression
    if (expr == kLogicalOperatorAnd || expr == kLogicalOperatorOr) {
      // A binary operator is parsed only when last operator has been applied.
      // Since last operator has not been applied and we are seeing another
      // operator in the expression, it can be considered an invalid
      // expression.
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
    // If '[last()]' predicate expression, check if current node at last
    // index.
    if ((expr == kPredicateSelectLastIndex &&
         node_index == node_set_size - 1) ||
        // If '[Index]' predicate expression, check if current node at given
        // index
        (absl::SimpleAtoi(expr, &num) && num == node_index) ||
        // If '[*]' predicate expression, no filter required.
        (expr == kPredicateSelectAll)) {
      single_predicate_result = true;
    } else if (std::any_of(kRelationsOperators.begin(),
                           kRelationsOperators.end(), [&](const char *op) {
                             return absl::StrContains(expr, op);
                           })) {
      // Look for predicate expression containing relational operators.
      single_predicate_result =
          PredicateFilterByNodeComparison(redfish_object, expr);
    } else if (absl::StartsWith(expr, "!")) {
      // For predicate [!<NodeName>]
      single_predicate_result =
          !PredicateFilterByNodeName(redfish_object, expr.substr(1));
    } else {
      // For predicate[<NodeName>]
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

// If there is an unpopulated variable the predicate cannot be completely
// invalidated in case there are other conditions in the predicate with
// populated variables. To facilitate this we need to break down the predicate
// and remove only the condition with the unpopulated variable. The input
// predicate has already been substituted with the variable values provided. Any
// remaining variables (string prefixed with $) are to be removed.
//
// Example:
//
// [ReadingType=$Type and ReadingUnits=$Units and Reading>$Threshold]
//        Variable Map: Type = Temperature, Threshold = 50
//
//  After processing this predicate will resolve to:
//
// [ReadingType=Temperature and Reading>50]
//
std::string InvalidateUnpopulatedVariables(absl::string_view predicate) {
  std::vector<absl::string_view> expressions =
      SplitExprByDelimiterWithEscape(predicate, " ", '\\');
  // For a single expression, just return the select all token.
  if (expressions.size() == 1) {
    return std::string(kPredicateSelectAll);
  }
  std::vector<absl::string_view> new_expressions;
  bool skip_next = false;
  int index = 0;
  for (absl::string_view expr : expressions) {
    if (skip_next) {
      skip_next = false;
      index++;
      continue;
    }
    if (absl::StrContains(expr, '$')) {
      if (index == expressions.size() - 1) {
        // Since this is the last expression we need to remove the logical
        // operator seen before. If nothing has been added to the list of new
        // expressions, return select all. This means that no variables have
        // been populated.
        if (new_expressions.empty()) return std::string(kPredicateSelectAll);
        new_expressions.pop_back();
      } else {
        // the next element is the logical operator, skip it to remove from
        // predicate.
        skip_next = true;
      }
    } else {
      new_expressions.push_back(expr);
    }
    index++;
  }
  return absl::StrJoin(new_expressions, " ");
}

// Provides a subquery level abstraction to traverse RedPath step expressions
// and apply predicate expression rules to refine a given node-set or fetch a
// Redfish object directly using uri_reference_redpath.
class SubqueryHandle final {
 public:
  SubqueryHandle(const DelliciusQuery::Subquery &subquery,
                 std::vector<std::pair<std::string, std::string>> redpath_steps,
                 Normalizer *normalizer)
      : subquery_(subquery),
        normalizer_(normalizer),
        redpath_steps_(std::move(redpath_steps)) {}

  // Parses given 'redfish_object' for properties requested in the subquery
  // and prepares dataset to be appended in 'result'.
  // When subqueries are linked, the normalized dataset is added to the given
  // 'parent_subquery_dataset' instead of the 'result'
  absl::StatusOr<SubqueryDataSet *> Normalize(
      const RedfishObject &redfish_object, DelliciusQueryResult &result,
      SubqueryDataSet *parent_subquery_dataset,
      const std::function<bool(const DelliciusQueryResult &result)> &callback =
          nullptr);

  void AddChildSubqueryHandle(SubqueryHandle *child_subquery_handle) {
    child_subquery_handles_.push_back(child_subquery_handle);
  }

  std::vector<SubqueryHandle *> GetChildSubqueryHandles() const {
    return child_subquery_handles_;
  }

  // Returns true if the encapsulated subquery does not have a root_subquery_id
  // set.
  bool IsRootSubquery() const { return subquery_.root_subquery_ids().empty(); }

  bool IsChildSubquery() const { return !IsRootSubquery(); }

  bool HasChildSubqueries() const { return !child_subquery_handles_.empty(); }

  void SetParentSubqueryDataSet(SubqueryDataSet *parent_subquery_data_set) {
    parent_subquery_data_set_ = parent_subquery_data_set;
  }

  void SubstituteVariables(const QueryVariables &variables) {
    std::vector<std::pair<std::string, std::string>> new_redpath_steps;
    std::vector<std::pair<std::string, std::string>> replacements;
    // Build the list of replacements that will be passed into StrReplaceAll.
    for (const auto &value : variables.values()) {
      if (value.name().empty()) continue;
      std::string result;
      std::string variable_name = absl::StrCat("$", value.name());
      replacements.push_back(std::make_pair(variable_name, value.value()));
    }
    // Go through all of the redpath steps
    for (const auto &pair : redpath_steps_) {
      std::pair<std::string, std::string> new_pair = pair;
      new_pair.second = absl::StrReplaceAll(new_pair.second, replacements);
      // If after the variable replacement there is still an unfilled variable,
      // remove the predicate step. This will be equivalent to a match-all/*.
      if (absl::StrContains(new_pair.second, '$')) {
        LOG(WARNING) << "Unmatched variable within predicate: "
                     << new_pair.first << "[" << new_pair.second << "]"
                     << ". Removing predicate step.";
        new_pair.second = InvalidateUnpopulatedVariables(new_pair.second);
      }
      new_redpath_steps.push_back(new_pair);
    }
    redpath_steps_ = new_redpath_steps;
  }

  RedPathIterator GetRedPathIterator() { return redpath_steps_.begin(); }

  bool IsEndOfRedPath(const RedPathIterator &iter) {
    return (iter != redpath_steps_.end()) &&
           (next(iter) == redpath_steps_.end());
  }

  bool HasRedPath() const { return subquery_.has_redpath(); }
  std::string RedPathToString() const {
    if (HasRedPath()) { return subquery_.redpath(); }
    return "";
  }

  bool HasUriReferenceRedpath() const {
    return subquery_.has_uri_reference_redpath();
  }
  std::string UriReferenceRedpathToString() const {
    if (HasUriReferenceRedpath()) { return subquery_.uri_reference_redpath(); }
    return "";
  }
  absl::string_view UriReferenceRedpath() const {
    return subquery_.uri_reference_redpath();
  }

  std::string GetSubqueryId() const { return subquery_.subquery_id(); }

  // Terminates redpath step traversal in a subquery.
  void TerminateSubquery() {
    terminate_subquery_ = true;
  }

  bool IsSubqueryTerminated() const { return terminate_subquery_; }

 private:
  DelliciusQuery::Subquery subquery_;
  Normalizer *normalizer_;
  // Collection of RedPath Step expressions - (NodeName + Predicate) in the
  // RedPath of a Subquery.
  // Eg. /Chassis[*]/Sensors[1] - {(Chassis, *), (Sensors, 1)}
  std::vector<std::pair<std::string, std::string>> redpath_steps_;
  std::vector<SubqueryHandle *> child_subquery_handles_;
  // Dataset of parent subquery to link the current subquery output with.
  SubqueryDataSet *parent_subquery_data_set_ = nullptr;
  // Terminate the subquery if the response size limit is reached.
  bool terminate_subquery_ = false;
};

struct RedPathContext {
  // Pointer to the SubqueryHandle object the RedPath associates with.
  SubqueryHandle *subquery_handle;
  // Dataset of the root RedPath to which the current RedPath dataset is
  // linked.
  SubqueryDataSet *root_redpath_dataset = nullptr;
  // Iterator configured to iterate over RedPath steps - NodeName and
  // Predicate pair - irrelevant for uri_reference_redpath.
  RedPathIterator redpath_steps_iterator;
  // Client callback to send the subquery results
  std::function<bool(const DelliciusQueryResult& result)> callback = nullptr;
};

// A ContextNode describes the RedfishObject relative to which one or more
// RedPath expressions or uri_reference_redpath is executed along with metadata
// necessary for the query operation and tracking.
struct ContextNode {
  // Redfish object relative to which RedPath expression executes.
  std::unique_ptr<RedfishObject> redfish_object;
  // RedPath contexts to execute relative to the Redfish Object.
  std::vector<RedPathContext> redpath_ctx_multiple;
  // Last RedPath executed to get the Redfish object.
  std::string last_executed_redpath;
};

// QueryPlanner encapsulates the logic to interpret subqueries, deduplicate
// RedPath path expressions, dispatch an optimum number of redfish resource
// requests, and return normalized response data per given property
// specification.
class QueryPlanner final : public QueryPlannerInterface {
 public:
  QueryPlanner(const DelliciusQuery &query,
    std::vector<std::unique_ptr<SubqueryHandle>> subquery_handles,
    RedPathRedfishQueryParams redpath_to_query_params,
    RedfishInterface *redfish_interface = nullptr)
      : plan_id_(query.query_id()),
        subquery_handles_(std::move(subquery_handles)),
        redpath_to_query_params_(
            CombineQueryParams(query, std::move(redpath_to_query_params))),
        redfish_interface_(redfish_interface) {}

  DelliciusQueryResult Run(const RedfishVariant &variant, const Clock &clock,
                           QueryTracker *tracker,
                           const QueryVariables &variables,
                           RedfishMetrics *metrics = nullptr) override;

  void Run(const RedfishVariant &variant, const Clock &clock,
           QueryTracker *tracker, const QueryVariables &variables,
           absl::FunctionRef<bool(const DelliciusQueryResult &result)> callback,
           RedfishMetrics *metrics = nullptr) override;

  void ProcessSubqueries(const RedfishVariant &variant,
          const QueryVariables &variables,
          std::function<bool(const DelliciusQueryResult &result)> callback,
          DelliciusQueryResult &result,
          QueryTracker *tracker);

  RedfishVariant FetchUriReference(const ContextNode &context_node,
          const std::string &node_name,
          DelliciusQueryResult &result,
          QueryTracker *tracker) const;

  RedfishVariant FetchUri(absl::string_view uri,
                QueryTracker *tracker = nullptr) const;

 private:
  const std::string plan_id_;
  // Collection of all SubqueryHandle instances including both root and child
  // handles.
  std::vector<std::unique_ptr<SubqueryHandle>> subquery_handles_;
  const RedPathRedfishQueryParams redpath_to_query_params_;
  RedfishInterface *redfish_interface_;
};

using SubqueryHandleCollection = std::vector<std::unique_ptr<SubqueryHandle>>;

// Generates SubqueryHandles for all Root Subqueries after resolving links
// within each subquery.
class SubqueryHandleFactory {
 public:
  static absl::StatusOr<SubqueryHandleCollection> CreateSubqueryHandles(
      const DelliciusQuery &query, Normalizer *normalizer) {
    return std::move(SubqueryHandleFactory(query, normalizer))
        .GetSubqueryHandles();
  }

 private:
  SubqueryHandleFactory(const DelliciusQuery &query, Normalizer *normalizer)
      : query_(query), normalizer_(normalizer) {
    for (const auto &subquery : query.subquery()) {
      id_to_subquery_[subquery.subquery_id()] = subquery;
    }
  }

  absl::StatusOr<SubqueryHandleCollection> GetSubqueryHandles();

  // Builds SubqueryHandle objects for subqueries linked together in a chain
  // through 'root_subquery_ids' property.
  // Args:
  //   subquery_id: Identifier of the subquery for which SubqueryHandle is
  //   built.
  //   subquery_id_chain: Stores visited ids to help identify loop in
  //   chain.
  //   child_subquery_handle: last built subquery handle to link as
  //   child node.
  absl::Status BuildSubqueryHandleChain(
      const std::string &subquery_id,
      absl::flat_hash_set<std::string> &subquery_id_chain,
      SubqueryHandle *child_subquery_handle);

  const DelliciusQuery &query_;
  absl::flat_hash_map<std::string, std::unique_ptr<SubqueryHandle>>
      id_to_subquery_handle_;
  absl::flat_hash_map<std::string, DelliciusQuery::Subquery> id_to_subquery_;
  Normalizer *normalizer_;
};

// Executes the next predicate expression in each RedPath and returns those
// RedPath contexts whose filter criteria is met by the given RedfishObject.
std::vector<RedPathContext> ExecutePredicateFromEachSubquery(
    const std::vector<RedPathContext> &redpath_ctx_multiple,
    const RedfishObject &redfish_object, size_t node_index,
    size_t node_set_size) {
  std::vector<RedPathContext> filtered_redpath_context;
  for (const auto &redpath_ctx : redpath_ctx_multiple) {
    if (!redpath_ctx.subquery_handle) {
      continue;
    }
    if (!ApplyPredicateRule(redfish_object, node_index, node_set_size,
                            redpath_ctx.redpath_steps_iterator)) {
      continue;
    }
    filtered_redpath_context.push_back(redpath_ctx);
  }
  return filtered_redpath_context;
}

// Populates Query Result for the requested properties for fully resolved
// RedPath expressions or returns RedPath contexts that have unresolved
// RedPath steps. When full resolved RedPath contexts have child RedPaths
// contexts linked, first the result is populated and then child RedPath
// contexts are retrieved and returned.
std::vector<RedPathContext> PopulateResultOrContinueQuery(
    const RedfishObject &redfish_object,
    const std::vector<RedPathContext> &redpath_ctx_multiple,
    DelliciusQueryResult &result) {
  std::vector<RedPathContext> redpath_ctx_unresolved;
  if (redpath_ctx_multiple.empty()) return redpath_ctx_unresolved;
  for (const auto &redpath_ctx : redpath_ctx_multiple) {
    const auto &subquery_handle = redpath_ctx.subquery_handle;
    if (subquery_handle->IsSubqueryTerminated()) {
      LOG(WARNING) << "Subquery already terminated, skipping.";
      continue;
    }

    bool is_end_of_redpath = subquery_handle->HasUriReferenceRedpath();
    if (subquery_handle->HasRedPath()) {
      is_end_of_redpath = subquery_handle->IsEndOfRedPath(
          redpath_ctx.redpath_steps_iterator);
    }

    // If there aren't any child subqueries and all step expressions in the
    // current RedPath context have been processed, we can populate the query
    // result for requested properties.
    if (is_end_of_redpath && !subquery_handle->HasChildSubqueries()) {
      subquery_handle
          ->Normalize(redfish_object, result, redpath_ctx.root_redpath_dataset,
                      redpath_ctx.callback)
          .IgnoreError();
      continue;
    }
    // If we have reached the end of RedPath expression but there are child
    // subqueries linked.
    if (is_end_of_redpath) {
      absl::StatusOr<SubqueryDataSet *> last_normalized_dataset;
      if (last_normalized_dataset = subquery_handle->Normalize(
              redfish_object, result, redpath_ctx.root_redpath_dataset,
              redpath_ctx.callback);
          !last_normalized_dataset.ok()) {
        continue;
      }
      // We will insert all the RedPath contexts in the list tracked for the
      // new context node.
      for (auto &child_subquery_handle :
           subquery_handle->GetChildSubqueryHandles()) {
        if (!child_subquery_handle) continue;
        redpath_ctx_unresolved.push_back(
            {child_subquery_handle, *last_normalized_dataset,
             child_subquery_handle->GetRedPathIterator(),
             redpath_ctx.callback});
      }
      continue;
    }
    redpath_ctx_unresolved.push_back(redpath_ctx);
    if (subquery_handle->HasRedPath()) {
      ++redpath_ctx_unresolved.back().redpath_steps_iterator;
    }
  }
  return redpath_ctx_unresolved;
}

// Returns a collection of RedPathContext objects that do not have a predicate
// expression in their step expression.
std::vector<RedPathContext> FilterRedPathWithNoPredicate(
    const std::vector<RedPathContext> &redpath_ctx_multiple) {
  std::vector<RedPathContext> redpath_ctx_no_predicate;
  for (const auto &redpath_ctx : redpath_ctx_multiple) {
    if (redpath_ctx.subquery_handle->HasRedPath()) {
      if (redpath_ctx.redpath_steps_iterator->second.empty()) {
        redpath_ctx_no_predicate.push_back(redpath_ctx);
      }
    }
  }
  return redpath_ctx_no_predicate;
}

using NodeNameToRedPathContexts =
    absl::flat_hash_map<std::string, std::vector<RedPathContext>>;

// Deduplicates the next NodeName expression in the RedPath/UriReference
// of each subquery and returns NodeName to Subquery Iterators map. This is to
// ensure Redfish Request is sent out once but the dataset obtained can be
// processed per Subquery using the mapped RedPathContext objects.
NodeNameToRedPathContexts DeduplicateNodeNamesAcrossSubqueries(
    std::vector<RedPathContext> &&redpath_context_multiple) {
  NodeNameToRedPathContexts node_to_redpath_contexts;
  for (auto &&redpath_context : redpath_context_multiple) {
    // Pair resource name and those RedPaths/UriReference that have this
    // resource as next NodeName.
    std::string node_name;
    if (redpath_context.subquery_handle->HasUriReferenceRedpath()) {
      node_name =
          redpath_context.subquery_handle->UriReferenceRedpathToString();
    } else if (redpath_context.subquery_handle->HasRedPath()) {
      node_name = redpath_context.redpath_steps_iterator->first;
    }
    node_to_redpath_contexts[node_name].push_back(redpath_context);
  }
  return node_to_redpath_contexts;
}

// Execute the next predicate expressions relative to given context_node from
// each mapped RedPath expression.
// Returns Context Node with an updated RedPath list whose predicate expressions
// filter criteria is met by the mapped context node.
ContextNode ExecutePredicateExpression(const int node_index,
                                       const size_t node_count,
                                       ContextNode context_node,
                                       DelliciusQueryResult &result) {
  // At this step only those RedPath contexts will be returned whose filter
  // criteria is met by the RedfishObject.
  std::vector<RedPathContext> redpath_ctx_filtered =
      ExecutePredicateFromEachSubquery(context_node.redpath_ctx_multiple,
                                       *context_node.redfish_object, node_index,
                                       node_count);
  if (redpath_ctx_filtered.empty()) return context_node;
  redpath_ctx_filtered = PopulateResultOrContinueQuery(
      *context_node.redfish_object, redpath_ctx_filtered, result);
  // Prepare the RedfishObject to serve as ContextNode for remaining
  // unresolved RedPath expressions.
  context_node.redpath_ctx_multiple = std::move(redpath_ctx_filtered);
  return context_node;
}

void PopulateSubqueryErrorStatus(
    const absl::Status &node_variant_status,
    const std::vector<RedPathContext> &redpath_ctx_multiple,
    DelliciusQueryResult &result, const std::string &node_name,
    const std::string &last_executed_redpath) {
  ::google::rpc::Code error_code = ::google::rpc::Code::INTERNAL;
  absl::StatusCode code = node_variant_status.code();
  if (code == absl::StatusCode::kNotFound) {
    error_code = ::google::rpc::Code::NOT_FOUND;
  } else if (code == absl::StatusCode::kDeadlineExceeded) {
    error_code = ::google::rpc::Code::DEADLINE_EXCEEDED;
  } else if (code == absl::StatusCode::kUnauthenticated) {
    error_code = ::google::rpc::Code::UNAUTHENTICATED;
  }
  // If the Get fails for the node name, mark the failure status in the
  // relevant subqueries.
  for (const auto &redpath_ctx : redpath_ctx_multiple) {
    ::google::rpc::Status *subquery_status =
        (*result.mutable_subquery_output_by_id())[redpath_ctx.subquery_handle
                                                      ->GetSubqueryId()]
            .mutable_status();
    subquery_status->set_code(error_code);
    subquery_status->set_message(
        absl::StrCat("Cannot resolve NodeName ", node_name,
                     " to valid Redfish object at path ", last_executed_redpath,
                     ". Redfish Request failed with error: ",
                     node_variant_status.ToString()));

    // Add the current subquery error to the DelliciusQueryResult error summary.
    SubqueryErrorSummary error_summary;
    error_summary.set_node_name(node_name);
    error_summary.set_last_executed_redpath(last_executed_redpath);
    *error_summary.mutable_status() = *subquery_status;
    error_summary.set_error_message(std::string(node_variant_status.message()));
    QueryErrors *errors = result.mutable_query_errors();
    (*errors->mutable_subquery_id_to_error_summary())
        [redpath_ctx.subquery_handle->GetSubqueryId()] = error_summary;
    errors->mutable_overall_error_summary()->append(absl::StrCat(
        ::google::rpc::Code_Name(error_code), " error occurred for subquery ",
        redpath_ctx.subquery_handle->GetSubqueryId(),
        " when processing node: ", node_name, "\n"));
  }
}

// Returns true if any RedPathContexts have a UriReferenceRedpath.
bool HasUriReferenceRedpath(
    const std::vector<RedPathContext> &redpath_ctx_multiple) {
  for (auto &&redpath_context : redpath_ctx_multiple) {
    if (redpath_context.subquery_handle->HasUriReferenceRedpath()) return true;
  }
  return false;
}

// Recursively executes RedPath Step expressions across subqueries.
// Dispatches Redfish resource request for each unique NodeName in RedPath
// Step expressions across subqueries followed by invoking predicate handlers
// from each subquery to further refine the data that forms the context node
// of next step expression in each qualified subquery.
void ExecuteRedPathStepFromEachSubquery(
    QueryPlanner *qp, const RedPathRedfishQueryParams &redpath_to_query_params,
    ContextNode &context_node, DelliciusQueryResult &result,
    QueryTracker *tracker) {
  // Return if the Context Node does not contain a valid RedfishObject.
  if (!context_node.redfish_object ||
      context_node.redpath_ctx_multiple.empty()) {
    return;
  }

  // First, we will pull NodeName from each RedPath expression across
  // subqueries and then deduplicate them to get a map between NodeName and
  // RedPaths that have the NodeName in common. Recall that in a RedPath
  // "/Chassis[*]/Sensors[*]", RedPath steps are "Chassis[*]" and "Sensors[*]"
  // and NodeName expressions are "Chassis" and "Sensors".
  NodeNameToRedPathContexts node_name_to_redpath_contexts =
      DeduplicateNodeNamesAcrossSubqueries(
          std::move(context_node.redpath_ctx_multiple));
  // Return if there is no redpath expression left to process across
  // subqueries.
  if (node_name_to_redpath_contexts.empty()) {
    return;
  }

  // Set of Nodes that are obtained by executing RedPath expressions relative
  // to Redfish Object encapsulated in given 'context_node'.
  std::vector<ContextNode> context_nodes;

  // We will query each NodeName in node_name_to_redpath_contexts map created
  // above and apply predicate expressions from each RedPath to filter the
  // nodes that produces next set of context nodes.
  for (auto &[node_name, redpath_ctx_multiple] :
       node_name_to_redpath_contexts) {
    // redpath expression handling begins
    std::string redpath_to_execute =
        absl::StrCat(context_node.last_executed_redpath, "/", node_name);

    // Get QueryRule configured for the RedPath expression we are about to
    // execute.
    auto get_params_for_redpath =
        GetQueryParamsForRedPath(redpath_to_query_params, redpath_to_execute);

    RedfishVariant node_set_as_variant(absl::OkStatus());

    if (HasUriReferenceRedpath(redpath_ctx_multiple)) {
      // Resolve the nested node in the Uri_Reference_Redpath expression
      // and the fetch the resolved Uri.
      node_set_as_variant = qp->FetchUriReference(context_node, node_name,
                                result, tracker);
    } else {
      // Dispatch Redfish Request for the Redfish Resource associated with the
      // NodeName expression.
      node_set_as_variant =
        context_node.redfish_object->Get(node_name, get_params_for_redpath);
    }

    // Add the last executed RedPath to the record.
    if (tracker) {
      tracker->redpaths_queried.insert(
          {redpath_to_execute, get_params_for_redpath});
    }

    // If NodeName does not resolve to a valid Redfish Resource, skip it!
    if (!node_set_as_variant.status().ok()) {
      PopulateSubqueryErrorStatus(node_set_as_variant.status(),
                                  redpath_ctx_multiple, result, node_name,
                                  context_node.last_executed_redpath);
      // It is not considered an error to not find requested nodes in the query.
      // So here we just log and skip the iteration.
      DLOG(INFO) << "Cannot resolve NodeName " << node_name
                 << " to valid Redfish object at path "
                 << context_node.last_executed_redpath;
      continue;
    }

    // Handle case where RedPath contexts have no predicate expression to
    // execute in their next step expression.
    std::vector<RedPathContext> redpath_ctx_no_predicate =
        FilterRedPathWithNoPredicate(redpath_ctx_multiple);
    if (!redpath_ctx_no_predicate.empty()) {
      std::unique_ptr<RedfishObject> node_as_object =
          node_set_as_variant.AsObject();
      if (!node_as_object) continue;
      std::vector<RedPathContext> redpath_ctx_filtered =
          PopulateResultOrContinueQuery(*node_as_object,
                                        redpath_ctx_no_predicate, result);
      ContextNode new_context_node{
          .redfish_object = std::move(node_as_object),
          .redpath_ctx_multiple = std::move(redpath_ctx_filtered),
          .last_executed_redpath = redpath_to_execute};
      context_nodes.push_back(std::move(new_context_node));
    }

    if (redpath_ctx_no_predicate.size() == redpath_ctx_multiple.size()) {
      continue;
    }

    // Initialize count to 1 since we know there is at least one Redfish node.
    // This node count could be more than 1 if we are dealing with Redfish
    // collection.
    size_t node_count = 1;

    // First try to access the Redfish node as collection
    GetParams redpath_params = GetQueryParamsForRedPath(
        redpath_to_query_params,
        absl::StrCat(redpath_to_execute, "[", kPredicateSelectAll, "]"));

    std::unique_ptr<RedfishIterable> node_as_iterable =
        node_set_as_variant.AsIterable(
            RedfishVariant::IterableMode::kAllowExpand,
            redpath_params.freshness);

    if (node_as_iterable == nullptr) {
      // We now know that the Redfish node is not a collection/array.
      // We will access the redfish node as a singleton RedfishObject.
      std::unique_ptr<RedfishObject> node_as_object =
          node_set_as_variant.AsObject();
      if (!node_as_object) continue;
      ContextNode new_context_node = ExecutePredicateExpression(
          0, node_count,
          {.redfish_object = std::move(node_as_object),
           .redpath_ctx_multiple = redpath_ctx_multiple,
           .last_executed_redpath = redpath_to_execute},
          result);
      context_nodes.push_back(std::move(new_context_node));
      continue;
    }

    // Redfish node is a collection. So from tracker's perspective we are
    // going to execute '[*]' predicate expression as we iterate over each
    // member in collection to test predicate filters.
    absl::StrAppend(&redpath_to_execute, "[", kPredicateSelectAll, "]");
    if (tracker) {
      tracker->redpaths_queried.insert({redpath_to_execute, redpath_params});
    }
    node_count = node_as_iterable->Size();

    for (int node_index = 0; node_index < node_count; ++node_index) {
      // If we are dealing with RedfishCollection, get collection member as
      // RedfishObject.
      RedfishVariant indexed_node = (*node_as_iterable)[node_index];
      if (!indexed_node.status().ok()) {
        PopulateSubqueryErrorStatus(indexed_node.status(), redpath_ctx_multiple,
                                    result, node_name, redpath_to_execute);
        DLOG(INFO)
            << "Cannot resolve NodeName " << node_name
            << " to valid Redfish object when executing collection redpath "
            << redpath_to_execute;
        continue;
      }
      std::unique_ptr<RedfishObject> indexed_node_as_object =
          indexed_node.AsObject();
      if (!indexed_node_as_object) continue;
      ContextNode new_context_node = ExecutePredicateExpression(
          node_index, node_count,
          {.redfish_object = std::move(indexed_node_as_object),
           .redpath_ctx_multiple = redpath_ctx_multiple,
           .last_executed_redpath = redpath_to_execute},
          result);
      context_nodes.push_back(std::move(new_context_node));
    }
  }

  // Now, for each new Context node obtained after applying Predicate
  // expression from all RedPath expressions, execute next RedPath Step
  // expression from every RedPath context mapped to the context node.
  for (auto &new_context_node : context_nodes) {
    ExecuteRedPathStepFromEachSubquery(qp, redpath_to_query_params,
                                       new_context_node, result, tracker);
  }
}

absl::StatusOr<SubqueryDataSet *> SubqueryHandle::Normalize(
    const RedfishObject &redfish_object, DelliciusQueryResult &result,
    SubqueryDataSet *parent_subquery_dataset,
    const std::function<bool(const DelliciusQueryResult &result)> &callback) {
  auto id = subquery_.subquery_id();
  ECCLESIA_ASSIGN_OR_RETURN(SubqueryDataSet subquery_dataset,
                            normalizer_->Normalize(redfish_object, subquery_));

  // Insert normalized data in the parent Subquery dataset if provided.
  // Otherwise, add the dataset in the query result.
  SubqueryOutput *subquery_output = nullptr;
  if (!parent_subquery_dataset) {
    subquery_output =
        &(*result.mutable_subquery_output_by_id())[subquery_.subquery_id()];
  } else {
    subquery_output = &(
        *parent_subquery_dataset
             ->mutable_child_subquery_output_by_id())[subquery_.subquery_id()];
  }
  // Check if size limit would be honored on appending the normalized data to
  // the result
  if (subquery_.has_max_size_in_bytes() && callback != nullptr) {
    size_t result_bytes =
        result.ByteSizeLong() + subquery_dataset.ByteSizeLong();
    if (result_bytes > subquery_.max_size_in_bytes()) {
      bool should_continue = callback(result);
      subquery_output->Clear();
      if (!should_continue) {
        TerminateSubquery();
        return nullptr;
      }
    }
  }

  auto *dataset = subquery_output->mutable_data_sets()->Add();
  if (dataset != nullptr) {
    *dataset = std::move(subquery_dataset);
  }
  return dataset;
}

void QueryPlanner::ProcessSubqueries(
    const RedfishVariant &variant,
    const QueryVariables &query_variables,
    const std::function<bool(const DelliciusQueryResult &result)> callback,
    DelliciusQueryResult &result, QueryTracker *tracker) {
  std::unique_ptr<RedfishObject> redfish_object = variant.AsObject();
  if (!redfish_object) {
    result.mutable_status()->set_code(::google::rpc::Code::FAILED_PRECONDITION);
    result.mutable_status()->set_message(absl::StrCat(
        "Cannot query service root for query with id: ", result.query_id(),
        ". Check host configuration."));
    return;
  }

  // We will create ContextNode for the RedfishObject relative to which all
  // RedPath expressions will execute.
  ContextNode context_node{.redfish_object = std::move(redfish_object)};

  // Now we create RedPathContext for each RedPath across subqueries and map
  // them to the ContextNode.
  for (auto &subquery_handle : subquery_handles_) {
    // Substitute any variables with their values provided by the query
    // engine.
    subquery_handle->SubstituteVariables(query_variables);

    // Only consider subqueries that have RedPath expressions to execute
    // relative to service root. This step filters out any child subqueries
    // which execute relative to other subqueries.
    if (!subquery_handle || subquery_handle->IsChildSubquery()) continue;

    // A context node can usually have multiple RedPath expressions mapped.
    // Let's instantiate the RedPathContext list with the one RedPath in the
    // subquery.
    RedPathIterator path_iter = subquery_handle->GetRedPathIterator();
    std::vector<RedPathContext> redpath_ctx_multiple = {
        {subquery_handle.get(), nullptr, path_iter, callback}};

    if (path_iter->first.empty()) {
      // A special case where properties need to be queried from service root
      // itself.
        redpath_ctx_multiple = PopulateResultOrContinueQuery(
            *context_node.redfish_object, redpath_ctx_multiple, result);
    }
    // Update ContextNode with RedPath contexts created for the subquery.
    context_node.redpath_ctx_multiple.insert(
      context_node.redpath_ctx_multiple.end(), redpath_ctx_multiple.begin(),
      redpath_ctx_multiple.end());
  }

  // Recursively execute each RedPath step across subqueries.
  ExecuteRedPathStepFromEachSubquery(this, redpath_to_query_params_,
                                     context_node, result, tracker);
}

DelliciusQueryResult QueryPlanner::Run(const RedfishVariant &variant,
                                       const Clock &clock,
                                       QueryTracker *tracker,
                                       const QueryVariables &query_variables,
                                       RedfishMetrics *metrics) {
  DelliciusQueryResult result;
  result.set_query_id(plan_id_);
  ProcessSubqueries(variant, query_variables, nullptr, result, tracker);
  if (metrics != nullptr) {
    *result.mutable_redfish_metrics() = *metrics;
  }
  return result;
}

void QueryPlanner::Run(
    const RedfishVariant &variant, const Clock &clock, QueryTracker *tracker,
    const QueryVariables &query_variables,
    absl::FunctionRef<bool(const DelliciusQueryResult &result)> callback,
    RedfishMetrics *metrics) {
  DelliciusQueryResult result;
  result.set_query_id(plan_id_);
  SetTime(clock, *result.mutable_start_timestamp());
  ProcessSubqueries(variant, query_variables, callback, result, tracker);
  SetTime(clock, *result.mutable_end_timestamp());
  if (metrics != nullptr) {
    *result.mutable_redfish_metrics() = *metrics;
  }
  callback(result);
}

// Fetch the Redfish object at the given URI_Reference_Redpath.
// The uri_reference_redpath is a non-navigational property and cannot be
// obtained by uri links. It is a nested node that's resolved using the
// redfish tree in the context_node.
RedfishVariant QueryPlanner::FetchUriReference(
          const ContextNode &context_node,
          const std::string &node_name,
          DelliciusQueryResult &result,
          QueryTracker *tracker) const
{
  const RedfishObject &redfish_object = *(context_node.redfish_object);
  // normalize the redpath format to nested node format
  std::string normalized_node_name = absl::StrReplaceAll(
        node_name, {{"/", "."}});
  // resolve the nest node represented in the normalized_node_name
  absl::StatusOr<nlohmann::json> json_obj =
        ResolveNodeNameToJsonObj(redfish_object, normalized_node_name);
  if (!json_obj.ok()) {
    return RedfishVariant(absl::NotFoundError(absl::StrCat
          ("Failed to resolve NodeName: ", normalized_node_name)));
  }

  std::string uri = json_obj->get<std::string>();
  return FetchUri(uri, tracker);
}

// Fetch the redfish object at "uri"
RedfishVariant QueryPlanner::FetchUri(absl::string_view uri,
                            QueryTracker *tracker) const
{
  // Get QueryRule configured for the RedPath expression we are about to
  // execute.
  auto get_params_for_redpath =
      GetQueryParamsForRedPath(redpath_to_query_params_, uri);

  // Dispatch Redfish Request to fetch the Redfish Resource associated with the
  // Uri.
  RedfishVariant node_set_as_variant = ecclesia::FetchUri(redfish_interface_,
                                            uri, get_params_for_redpath);
  // Add the last executed uri to the record.
  if (tracker) {
    std::string uri_str = std::string(uri);
    tracker->redpaths_queried.insert({uri_str, get_params_for_redpath});
  }

  // Propagate a more useful message to the client.
  if (!node_set_as_variant.status().ok() &&
      absl::IsNotFound(node_set_as_variant.status())) {
    return RedfishVariant(absl::NotFoundError(absl::StrCat("Failed to fetch "
        "uri: ", uri, " Error: ", node_set_as_variant.status().message())));
  }

  return node_set_as_variant;
}

absl::Status SubqueryHandleFactory::BuildSubqueryHandleChain(
    const std::string &subquery_id,
    absl::flat_hash_set<std::string> &subquery_id_chain,
    SubqueryHandle *child_subquery_handle) {
  auto id_to_subquery_iter = id_to_subquery_.find(subquery_id);
  if (id_to_subquery_iter == id_to_subquery_.end()) {
    return absl::InternalError(
        absl::StrFormat("Cannot find a subquery for id: %s", subquery_id));
  }
  DelliciusQuery::Subquery &subquery = id_to_subquery_iter->second;
  // Subquery links create a loop if same subquery id exists in the chain.
  if (!subquery_id_chain.insert(subquery_id).second) {
    return absl::InternalError("Loop detected in subquery links");
  }

  // Find SubqueryHandle for given SubqueryId
  auto id_to_subquery_handle_iter = id_to_subquery_handle_.find(subquery_id);
  // If SubqueryHandle exists for the given identifier and a child subquery
  // handle is provided, link the child SubqueryHandle.
  if (id_to_subquery_handle_iter != id_to_subquery_handle_.end()) {
    if (child_subquery_handle != nullptr) {
      id_to_subquery_handle_iter->second->AddChildSubqueryHandle(
          child_subquery_handle);
    }
    return absl::OkStatus();
  }

  std::unique_ptr<SubqueryHandle> new_subquery_handle;
  if (subquery.has_redpath()) {
    // Create a new SubqueryHandle.
    absl::StatusOr<std::vector<RedPathStep>> steps =
          RedPathToSteps(subquery.redpath());
    if (!steps.ok()) {
      LOG(ERROR) << "Cannot create SubqueryHandle for " << subquery_id;
      return steps.status();
    }
    new_subquery_handle = std::make_unique<SubqueryHandle>(
      subquery, (std::move(steps)).value(), normalizer_);
  } else {
    new_subquery_handle = std::make_unique<SubqueryHandle>(
      subquery, std::vector<RedPathStep>(), normalizer_);
  }

  // Raw pointer used to link SubqueryHandle with parent subquery if any.
  SubqueryHandle *new_subquery_handle_ptr = new_subquery_handle.get();
  // Link the given child SubqueryHandle.
  if (child_subquery_handle != nullptr) {
    new_subquery_handle->AddChildSubqueryHandle(child_subquery_handle);
  }
  id_to_subquery_handle_[subquery_id] = std::move(new_subquery_handle);
  // Return if Subquery does not have any root Subquery Ids linked i.e. the
  // subquery itself is a root.
  if (subquery.root_subquery_ids().empty()) {
    return absl::OkStatus();
  }
  // Recursively build root subquery handles.
  for (const auto &root_subquery_id : subquery.root_subquery_ids()) {
    absl::flat_hash_set<std::string> subquery_id_chain_per_root =
        subquery_id_chain;
    auto status = BuildSubqueryHandleChain(
        root_subquery_id, subquery_id_chain_per_root, new_subquery_handle_ptr);
    if (!status.ok()) {
      return status;
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<SubqueryHandleCollection>
SubqueryHandleFactory::GetSubqueryHandles() {
  for (const auto &subquery : query_.subquery()) {
    absl::flat_hash_set<std::string> subquery_id_chain;
    absl::Status status = BuildSubqueryHandleChain(subquery.subquery_id(),
                                                   subquery_id_chain, nullptr);
    if (!status.ok()) {
      return status;
    }
  }
  SubqueryHandleCollection subquery_handle_collection;
  for (auto &&[_, subquery_handle] : id_to_subquery_handle_) {
    subquery_handle_collection.push_back(std::move(subquery_handle));
  }
  if (subquery_handle_collection.empty()) {
    return absl::InternalError("No SubqueryHandle created");
  }
  return subquery_handle_collection;
}

}  // namespace

// Builds the default query planner.
absl::StatusOr<std::unique_ptr<QueryPlannerInterface>> BuildDefaultQueryPlanner(
  const DelliciusQuery &query,
  RedPathRedfishQueryParams redpath_to_query_params, Normalizer *normalizer,
  ecclesia::RedfishInterface *redfish_interface) {
  absl::StatusOr<SubqueryHandleCollection> subquery_handle_collection =
      SubqueryHandleFactory::CreateSubqueryHandles(query, normalizer);
  if (!subquery_handle_collection.ok()) {
    return subquery_handle_collection.status();
  }
  return std::make_unique<QueryPlanner>(query,
                                        *std::move(subquery_handle_collection),
                                        std::move(redpath_to_query_params),
                                        redfish_interface);
}

}  // namespace ecclesia
