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

#include "ecclesia/lib/redfish/redpath/definitions/query_predicates/variable_substitution.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_predicates/predicate_object.h"
#include "ecclesia/lib/status/macros.h"

namespace ecclesia {
namespace {

constexpr std::string_view kPredicateSelectAll = "*";

std::string ReplaceMultiValueVariables(
    std::string &new_predicate, std::string_view variable_name,
    const size_t var_pos, const int values_count,
    const std::vector<std::string> &var_values) {
  // For variables with multiple values, we expand the expression with the
  // variable into an expression surrounded by parenthesis, with ORs.
  // example: A and B=$VAR -> A and (B=var_val0 or B=var_val1)
  size_t last_space_pos = new_predicate.substr(0, var_pos).find_last_of(' ');
  // the substring between the last space and the variable name is the
  // "property_name=" portion of the templated expression.
  if (last_space_pos == std::string_view::npos) {
    last_space_pos = new_predicate.find_first_not_of('(');
  } else {
    last_space_pos++;
  }
  std::string property_with_op =
      new_predicate.substr(last_space_pos, var_pos - last_space_pos);
  // Build up new expression, starting with an open parenthesis.
  std::string new_expression = "(";
  for (int i = 0; i < values_count - 1; ++i) {
    absl::StrAppend(&new_expression,
                    // NO_CDC: Size of var_values is checked in caller.
                    property_with_op, var_values[i], " or ");
  }
  // Append last expression and a closing parenthesis.
  absl::StrAppend(&new_expression, property_with_op,
                  // NO_CDC: Size of var_values is checked in caller.
                  var_values[values_count - 1], ")");
  // Replace "templated_prop=$variable" with the new expression.
  return absl::StrReplaceAll(
      new_predicate,
      {{absl::StrCat(property_with_op, variable_name), new_expression}});
}

void RemoveUnpopulatedExpressions(PredicateObject &predicate_object) {
  bool first = true;
  int logical_operators_index = 0;
  std::vector<std::string> new_logical_operators;
  std::vector<PredicateObject> new_child_predicates;
  for (PredicateObject child : predicate_object.child_predicates) {
    if (!child.relational_expression.rel_operator.empty()) {
      if (absl::StrContains(child.relational_expression.rhs, "$")) {
        // If an unpopulated variable is found do not add it or the accompanying
        // logical operator.
        logical_operators_index++;
        continue;
      }
      new_child_predicates.push_back(child);
      // Property existence expressions cannot have variables, so just add them.
    } else if (!child.relational_expression.property_name.empty()) {
      new_child_predicates.push_back(child);
    } else {
      // Recurse over a child predicate.
      RemoveUnpopulatedExpressions(child);
      // If all child predicates are removed just skip this child altogether.
      if (child.child_predicates.empty()) {
        logical_operators_index++;
        continue;
      }
      new_child_predicates.push_back(child);
    }
    // Add the next logical operator. The first expression to be processed will
    // not add a logical operator.
    if (first) {
      first = false;
    } else {
      new_logical_operators.push_back(
          predicate_object.logical_operators.at(logical_operators_index++));
    }
  }
  predicate_object.child_predicates = new_child_predicates;
  predicate_object.logical_operators = new_logical_operators;
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
absl::StatusOr<std::string> InvalidateUnpopulatedVariables(
    std::string_view predicate) {
  ECCLESIA_ASSIGN_OR_RETURN(PredicateObject predicate_object,
                            CreatePredicateObject(predicate));
  // If the predicate is just a single expression with an unpopulated variable,
  // simply return the select-all symbol '*'.
  if (!predicate_object.relational_expression.rel_operator.empty()) {
    return std::string(kPredicateSelectAll);
  }
  // Call a recursive function to remove the relational expressions with an
  // unpopulated variable.
  RemoveUnpopulatedExpressions(predicate_object);
  return PredicateObjectToString(predicate_object);
}

}  // namespace

absl::StatusOr<std::string> SubstituteVariables(
    std::string_view predicate, const QueryVariables &variables) {
  std::string new_predicate = std::string(predicate);
  for (const auto &value : variables.variable_values()) {
    if (value.name().empty()) continue;
    std::string variable_name = absl::StrCat("$", value.name());
    const size_t var_pos = new_predicate.find(variable_name);
    if (var_pos == std::string_view::npos) continue;
    int values_count = value.values_size();
    const auto &var_values = value.values();
    std::vector<std::string> var_values_vector(var_values.begin(),
                                               var_values.end());
    if (values_count == 1) {
      new_predicate =
          absl::StrReplaceAll(new_predicate, {{variable_name, var_values[0]}});
    } else if (values_count > 1) {
      new_predicate =
          ReplaceMultiValueVariables(new_predicate, variable_name, var_pos,
                                     values_count, var_values_vector);
    }
  }
  if (absl::StrContains(new_predicate, "$")) {
    ECCLESIA_ASSIGN_OR_RETURN(new_predicate,
                              InvalidateUnpopulatedVariables(new_predicate));
  }
  return new_predicate;
}

}  // namespace ecclesia
