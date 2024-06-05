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

#include "ecclesia/lib/redfish/redpath/definitions/query_predicates/predicate_object.h"

#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/status/macros.h"
#include "re2/re2.h"

namespace ecclesia {
namespace {

// Pattern for expression [lhs][operator][rhs]
constexpr LazyRE2 kRelationalExpressionRegex = {
    "^(?P<left>[^\\s<=>!]+)(?:(<=|>=|!=|>|<|=)(?P<right>[^<=>!]+))$"};

absl::StatusOr<RelationalExpression> EncodeRelationalExpression(
    absl::string_view expression) {
  RelationalExpression relational_expression;

  // Regex match the expression. The operator must be a valid relational
  // operator and the right hand side must not have spaces or characters
  // included in relation operators.
  std::string lhs;
  std::string op;
  std::string rhs;
  if (RE2::FullMatch(expression, *kRelationalExpressionRegex, &lhs, &op,
                     &rhs)) {
    relational_expression.lhs = lhs;
    relational_expression.rel_operator = op;
    relational_expression.rhs = rhs;
    return relational_expression;
  }
  return absl::InvalidArgumentError("Invalid expression");
}

bool IsRelationalExpression(absl::string_view expression) {
  return RE2::FullMatch(expression, *kRelationalExpressionRegex);
}

// Iterates over the given string, capturing everything within the first open
// parenthesis.
std::string GetChildPredicate(absl::string_view predicate) {
  std::string child_predicate;
  int parenthesis_index = 1;
  std::string expression;
  for (char c : predicate) {
    if (c == ')') {
      if (parenthesis_index == 1) {
        return expression;
      }
      expression += c;
      parenthesis_index--;
      continue;
    }
    if (c == '(') {
      parenthesis_index++;
      expression += c;
      continue;
    }
    expression += c;
  }
  return child_predicate;
}

// Iterates over the given string, capturing the first relational expression in
// it's entirety.
std::string GetChildRelationalExpression(absl::string_view predicate) {
  std::string child_relational_expression;
  bool escaped = false;
  for (char c : predicate) {
    if (escaped) {
      child_relational_expression += c;
      escaped = false;
      continue;
    }
    if (c == '\\') {
      escaped = true;
      child_relational_expression += c;
      continue;
    }
    if (c == ' ') {
      return child_relational_expression;
    }
    child_relational_expression += c;
  }
  return child_relational_expression;
}

// The predicate part will be prefixed with a logical operator, so this method's
// goal is to identify which operator it is.
absl::StatusOr<std::string> GetLogicalOperator(
    absl::string_view predicate_part) {
  if (absl::StartsWith(predicate_part, " and ")) {
    return " and ";
  }
  if (absl::StartsWith(predicate_part, " or ")) {
    return " or ";
  }
  return absl::InvalidArgumentError("Unknown logical operator");
}

}  // namespace

absl::StatusOr<PredicateObject> CreatePredicateObject(
    absl::string_view predicate) {
  // Base case. If the entire predicate is a single relational expression encode
  // and return.
  PredicateObject predicate_object;
  if (IsRelationalExpression(predicate)) {
    ECCLESIA_ASSIGN_OR_RETURN(RelationalExpression relexp,
                              EncodeRelationalExpression(predicate));
    predicate_object.relational_expression = relexp;
    return predicate_object;
  }
  int index = 0;
  // The predicate is a logical expression, so now we must iterate through the
  // entire predicate, processing each expression as its found.
  while (index < predicate.size()) {
    char first_character = predicate[index];
    // If it starts with a parenthesis its a child predicate
    if (first_character == '(') {
      index++;
      std::string child_expression = GetChildPredicate(predicate.substr(index));
      // Add one for close parenthesis.
      index += static_cast<int>(child_expression.size()) + 1;
      // Recursively call the child expression and add the result as a child.
      ECCLESIA_ASSIGN_OR_RETURN(PredicateObject child_predicate,
                                CreatePredicateObject(child_expression));
      predicate_object.child_predicates.push_back(std::move(child_predicate));
      // If the expressions does not start with a parenthesis the expression is
      // relational.
    } else {
      std::string child_relational_expression =
          GetChildRelationalExpression(predicate.substr(index));
      index += static_cast<int>(child_relational_expression.size());
      PredicateObject relational_predicate;
      ECCLESIA_ASSIGN_OR_RETURN(
          RelationalExpression relational_expression,
          EncodeRelationalExpression(child_relational_expression));
      relational_predicate.relational_expression = relational_expression;
      predicate_object.child_predicates.push_back(relational_predicate);
    }
    // If we have not reached the end of the predicate then there must be a
    // logical operator.
    if (index < predicate.size()) {
      ECCLESIA_ASSIGN_OR_RETURN(std::string logop,
                                GetLogicalOperator(predicate.substr(index)));
      index += static_cast<int>(logop.size());
      predicate_object.logical_operators.push_back(logop);
    }
  }
  return predicate_object;
}

std::string PredicateObjectToString(const PredicateObject &predicate_object) {
  // This is the "base case", as in there are no children so we just construct
  // the relational expression and return.
  if (predicate_object.child_predicates.empty()) {
    return absl::StrCat(predicate_object.relational_expression.lhs,
                        predicate_object.relational_expression.rel_operator,
                        predicate_object.relational_expression.rhs);
  }
  std::string predicate_string;
  int logical_index = 0;
  // If the predicate object has children recurse over them.
  for (const PredicateObject &child_predicate :
       predicate_object.child_predicates) {
    // Don't recurse if a "leaf" is found as it will add extraneous parenthesis.
    if (child_predicate.child_predicates.empty()) {
      absl::StrAppend(&predicate_string,
                      child_predicate.relational_expression.lhs,
                      child_predicate.relational_expression.rel_operator,
                      child_predicate.relational_expression.rhs);
    } else {
      absl::StrAppend(&predicate_string, "(",
                      PredicateObjectToString(child_predicate), ")");
    }
    if (logical_index < predicate_object.logical_operators.size()) {
      absl::StrAppend(&predicate_string,
                      predicate_object.logical_operators[logical_index]);
      logical_index++;
    }
  }
  return predicate_string;
}
}  // namespace ecclesia
