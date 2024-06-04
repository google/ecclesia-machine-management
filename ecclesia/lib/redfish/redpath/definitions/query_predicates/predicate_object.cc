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

#include <cstddef>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
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

}  // namespace

absl::StatusOr<PredicateObject> CreatePredicateObject(
    absl::string_view predicate) {
  // The high level logic of this function is to break down the predicate into
  // logical operators (or/and) and the relational expressions (prop>value).
  // After breaking them up go through the relational expressions and break them
  // down into parts and turn them into objects.
  PredicateObject predicate_object;
  std::vector<std::string> logical_split;
  // Break up by logical operators
  std::vector<std::string> or_blocks = absl::StrSplit(predicate, " or ");
  // Number of "or" elements to include in the split
  size_t or_count = or_blocks.size() - 1;
  for (const std::string &block : or_blocks) {
    std::vector<std::string> and_blocks = absl::StrSplit(block, " and ");
    if (and_blocks.size() > 1) {
      // Number of "and" elements to include in the split
      size_t and_count = and_blocks.size() - 1;
      // Add the and blocks delimited by " and "
      for (const std::string &and_block : and_blocks) {
        logical_split.push_back(and_block);
        if (and_count > 0) {
          and_count--;
          predicate_object.logical_operators.push_back(" and ");
        }
      }
    } else {
      logical_split.push_back(block);
    }
    if (or_count > 0) {
      or_count--;
      predicate_object.logical_operators.push_back(" or ");
    }
  }
  // Go through relational expressions and encode them.
  for (const std::string &expression : logical_split) {
    auto encoded_expression = EncodeRelationalExpression(expression);
    if (encoded_expression.ok()) {
      predicate_object.expressions.push_back(encoded_expression.value());
    } else {
      // Invalid expression, return an error
      return absl::InvalidArgumentError(encoded_expression.status().message());
    }
  }
  return predicate_object;
}

std::string PredicateObjectToString(const PredicateObject &predicate_object) {
  std::string predicate_string;
  int logical_index = 0;
  for (const RelationalExpression &expression : predicate_object.expressions) {
    absl::StrAppend(&predicate_string, expression.lhs, expression.rel_operator,
                    expression.rhs);
    if (logical_index < predicate_object.logical_operators.size()) {
      absl::StrAppend(&predicate_string,
                      predicate_object.logical_operators[logical_index]);
      logical_index++;
    }
  }
  return predicate_string;
}
}  // namespace ecclesia
