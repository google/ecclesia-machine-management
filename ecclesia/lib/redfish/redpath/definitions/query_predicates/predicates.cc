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

#include "ecclesia/lib/redfish/redpath/definitions/query_predicates/predicates.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ecclesia/lib/redfish/dellicius/utils/path_util.h"
#include "ecclesia/lib/status/macros.h"
#include "single_include/nlohmann/json.hpp"
#include "re2/re2.h"

namespace ecclesia {

namespace {

// Pattern for predicate formatted with relational operators:
constexpr LazyRE2 kPredicateRegexRelationalOperator = {
    R"(^([a-zA-Z#@][0-9a-zA-Z.\\]*)(?:(!=|>|<|=|>=|<=))([a-zA-Z0-9._\-\:#\\ ]+)$)"};

// Pattern for Redfish standard (ISO 8601) datetime string.
// Example: 2022-03-16T15:52:00
constexpr LazyRE2 kRedfishDatetimeRegex = {
    R"(^(?:[1-9]\d{3}-(?:(?:0[1-9]|1[0-2])-(?:0[1-9]|1\d|2[0-8])|(?:0[13-9]|1[0-2])-(?:29|30)|(?:0[13578]|1[02])-31)|(?:[1-9]\d(?:0[48]|[2468][048]|[13579][26])|(?:[2468][048]|[13579][26])00)-02-29)T(?:[01]\d|2[0-3]):[0-5]\d:[0-5]\d(?:\.\d{1,9})?(?:Z|[+-][01]\d:[0-5]\d)?$)"};

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

template <typename t>
bool TestLogicalOp(const std::string &op, t lhs, t rhs) {
  if (op == ">=") return lhs >= rhs;
  if (op == ">") return lhs > rhs;
  if (op == "<=") return lhs <= rhs;
  if (op == "<") return lhs < rhs;
  if (op == "!=") return lhs != rhs;
  return lhs == rhs;
}

absl::StatusOr<bool> ApplyNumberComparisonFilter(const std::string &op,
                                                 const nlohmann::json &obj,
                                                 double rhs) {
  double number;
  if (obj.is_number()) {
    number = obj.get<double>();
  } else if (!absl::SimpleAtod(obj.get<std::string>(), &number)) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid number comparison. Json Object: ", obj.dump(),
                     "TestValue: ", rhs));
  }
  return TestLogicalOp<double>(op, number, rhs);
}

// Helper function used to validate two timestamp strings are in line with the
// expected redfish standard and applies a given comparator.
absl::StatusOr<bool> ApplyDateTimeComparisonFilter(
    const std::string &op, const std::string &lhs_time_str,
    absl::string_view test_value) {
  absl::Time rhs_time;
  absl::Time lhs_time;

  // Parse the user supplied timestamp into the desired format.
  if (!absl::ParseTime(kRedfishDatetimeNoOffset, test_value, &rhs_time,
                       /*err=*/nullptr)) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid datetime string in predicate: ", test_value));
  }

  // Parse the timestamp from the Redfish property into the desired format.
  if (!absl::ParseTime(kRedfishDatetimePlusOffset, lhs_time_str, &lhs_time,
                       /*err=*/nullptr)) {
    return absl::InternalError(absl::StrCat(
        "Invalid datetime string in redfish property: ", lhs_time_str));
  }
  return TestLogicalOp<absl::Time>(op, lhs_time, rhs_time);
}

bool IsDateTimeString(absl::string_view test_value) {
  return RE2::FullMatch(test_value, *kRedfishDatetimeRegex);
}

// Handler for predicate expressions containing relational operators.
absl::StatusOr<bool> PredicateFilterByNodeComparison(
    const nlohmann::json &json_object, absl::string_view predicate) {
  std::string node_name;
  std::string op;
  std::string test_value;
  bool ret = false;
  if (!RE2::FullMatch(predicate, *kPredicateRegexRelationalOperator, &node_name,
                      &op, &test_value)) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid node comparison: ", predicate));
  }

  double value;
  auto json_obj = ResolveRedPathNodeToJson(json_object, node_name);
  if (!json_obj.ok()) {
    return false;
  }
  if (IsDateTimeString(test_value)) {
    return json_obj->is_string()
               ? ApplyDateTimeComparisonFilter(op, json_obj->get<std::string>(),
                                               test_value)
               : false;
  }
  // Number comparison.
  if (absl::SimpleAtod(test_value, &value)) {
    ECCLESIA_ASSIGN_OR_RETURN(
        ret, ApplyNumberComparisonFilter(op, *json_obj, value));
  } else if (test_value == kBinaryOperandFalse) {
    ret = ApplyStringComparisonFilter(
        [json_obj]() { return *json_obj == false; }, op);
  } else if (test_value == kBinaryOperandTrue) {
    ret = ApplyStringComparisonFilter(
        [json_obj]() { return *json_obj == true; }, op);
  } else if (test_value == "null") {
    // For the property value is null.
    ret = ApplyStringComparisonFilter(
        [json_obj]() { return json_obj->is_null(); }, op);
  } else {
    // For the property value's type is string.
    const auto condition = [json_obj, &test_value]() {
      absl::StrReplaceAll({{"\\", ""}}, &test_value);
      return *json_obj == test_value;
    };
    ret = ApplyStringComparisonFilter(condition, op);
  }
  return ret;
}

// Handler for '[nodename]'
// Checks if given Redfish Resource contains predicate string.
bool PredicateFilterByNodeName(const nlohmann::json &json_object,
                               absl::string_view predicate) {
  std::vector<std::string> node_names = SplitNodeNameForNestedNodes(predicate);
  if (node_names.empty()) {
    return false;
  }
  nlohmann::json leaf = json_object;
  for (auto const &name : node_names) {
    if (!leaf.contains(name)) {
      return false;
    }
    leaf = leaf.at(name);
  }
  return true;
}

}  // namespace

absl::StatusOr<bool> ApplyPredicateRule(const nlohmann::json &json_object,
                                        const PredicateOptions &options) {
  if (options.predicate.empty()) {
    return absl::InvalidArgumentError("Empty predicate");
  }
  absl::string_view logical_operation = kLogicalOperatorAnd;

  // Set to true to create a default boolean operand for logical operations.
  // A single predicate translates `True and <predicate>`.
  bool is_filter_success = true;
  std::vector<absl::string_view> expressions =
      SplitExprByDelimiterWithEscape(options.predicate, " ", '\\');
  for (absl::string_view expr : expressions) {
    // If expression is a logical operator, capture it and move to next
    // expression
    if (expr == kLogicalOperatorAnd || expr == kLogicalOperatorOr) {
      // A binary operator is parsed only when last operator has been applied.
      // If `logical_operation` is not empty, last operator is not applied
      // and we are seeing another operator in the expression.
      // We can safely consider this as an invalid expression.
      if (!logical_operation.empty()) {
        return absl::InvalidArgumentError(
            absl::StrCat("Invalid predicate: ", options.predicate));
      }
      logical_operation = expr;
      continue;
    }

    // There should always be a logical operation defined for the predicates.
    // Default logical operation is 'and' between a predicate expression and
    // default boolean operand 'true'
    if (logical_operation.empty()) {
      return absl::InvalidArgumentError(
          absl::StrCat("Invalid predicate: ", options.predicate));
    }

    size_t num;
    bool single_predicate_result = false;

    // If '[last()]' predicate expression, check if current node at last
    // index.
    if ((expr == kPredicateSelectLastIndex &&
         options.node_index == options.node_set_size - 1) ||
        // If '[Index]' predicate expression, check if current node at given
        // index
        (absl::SimpleAtoi(expr, &num) && num == options.node_index) ||
        // If '[*]' predicate expression, no filter required.
        (expr == kPredicateSelectAll)) {
      single_predicate_result = true;
    } else if (std::any_of(kRelationsOperators.begin(),
                           kRelationsOperators.end(), [&](const char *op) {
                             return absl::StrContains(expr, op);
                           })) {
      // Look for predicate expression containing relational operators.
      ECCLESIA_ASSIGN_OR_RETURN(
          single_predicate_result,
          PredicateFilterByNodeComparison(json_object, expr));
    } else if (absl::StartsWith(expr, "!")) {
      // For predicate [!<NodeName>]
      single_predicate_result =
          !PredicateFilterByNodeName(json_object, expr.substr(1));
    } else {
      // For predicate[<NodeName>]
      single_predicate_result = PredicateFilterByNodeName(json_object, expr);
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

}  // namespace ecclesia
