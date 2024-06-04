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

#include "ecclesia/lib/redfish/redpath/definitions/query_predicates/filter.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_predicates/predicate_object.h"
#include "ecclesia/lib/status/macros.h"

namespace ecclesia {
namespace {

// Encodes special characters in predicates. Special characters are derived
// from https://datatracker.ietf.org/doc/html/rfc3986#section-2.2
std::string EncodeSpecialCharacters(absl::string_view filter_string) {
  std::vector<std::pair<std::string, std::string>> special_character_encodings =
      {
          {"+", "%2B"}, {" ", "%20"}, {":", "%3A"}, {"/", "%2F"}, {"?", "%3F"},
          {"#", "%25"}, {"[", "%5B"}, {"]", "%5D"}, {"@", "%40"}, {"!", "%21"},
          {"$", "%24"}, {"&", "%26"}, {"'", "%27"}, {"(", "%28"}, {")", "%29"},
          {"*", "%2A"}, {",", "%2C"}, {";", "%3B"}, {"=", "%3D"},
      };
  return absl::StrReplaceAll(filter_string, special_character_encodings);
}

// Takes a RelationalExpression in Redpath format and returns a
// RelationalExpression in $filter format after applying a number of transforms.
RelationalExpression ApplyTransformsToExpression(
    RelationalExpression redpath_expression) {
  std::vector<std::pair<std::string, std::string>> relational_operators = {
      {"<", " lt "},  {">", " gt "}, {"<=", " le "},
      {">=", " ge "}, {"=", " eq "}, {"!=", " ne "}};
  RelationalExpression new_expression = std::move(redpath_expression);
  // Substitute relational operators
  new_expression.rel_operator =
      absl::StrReplaceAll(new_expression.rel_operator, relational_operators);
  // Replace periods with slashes in the left-hand-side
  new_expression.lhs = absl::StrReplaceAll(new_expression.lhs, {{".", "/"}});
  // Add quotes to string base types.
  std::string rhs_string = new_expression.rhs;
  int num;
  float float_num;
  // Check to see if the token is a number, for this just check if its an
  // int or a float.
  std::vector<std::string> booleans = {"true", "false"};
  // Check that the right-hand-side is not a number.
  if (!absl::SimpleAtoi(new_expression.rhs, &num) &&
      !absl::SimpleAtof(new_expression.rhs, &float_num)) {
    // Check that the right-hand-side is not a boolean.
    if (std::find(booleans.begin(), booleans.end(), new_expression.rhs) ==
        booleans.end()) {
      std::string first_char = new_expression.rhs.substr(0, 1);
      // Check if the first character is a single quote. If so, the quotes are
      // already in place.
      if (first_char != "'") {
        new_expression.rhs = absl::StrCat("'", new_expression.rhs, "'");
      }
    }
  }
  return new_expression;
}

// Takes a EncodedPredicate in $filter form and returns a $filter string that
// abides by the Redfish Specification 7.3.4
std::string GenerateFilterString(const PredicateObject &predicate_object) {
  std::vector<RelationalExpression> expressions;
  expressions.reserve(predicate_object.expressions.size());
  // Before creating the $filter string, all of the differences between Redpath
  // and $filter format need to be applied
  for (const RelationalExpression &expression : predicate_object.expressions) {
    expressions.push_back(ApplyTransformsToExpression(expression));
  }
  // Combine the transformed expressions to the $filter string joined with the
  // logical operators if necessary.
  std::string filter_string;
  int logical_index = 0;
  for (const RelationalExpression &expression : expressions) {
    absl::StrAppend(&filter_string, expression.lhs, expression.rel_operator,
                    expression.rhs);
    if (logical_index < predicate_object.logical_operators.size()) {
      absl::StrAppend(&filter_string,
                      predicate_object.logical_operators[logical_index]);
      logical_index++;
    }
  }
  // Substitute special characters with encodings
  return EncodeSpecialCharacters(filter_string);
}

}  // namespace

absl::StatusOr<std::string> BuildFilterFromRedpathPredicate(
    absl::string_view predicate) {
  ECCLESIA_ASSIGN_OR_RETURN(PredicateObject encoded_predicate,
                            CreatePredicateObject(predicate));
  return GenerateFilterString(encoded_predicate);
}

absl::StatusOr<std::string> BuildFilterFromRedpathPredicateList(
    const std::vector<std::string> &predicates) {
  std::vector<std::string> filter_strings;
  filter_strings.reserve(predicates.size());
  for (absl::string_view predicate : predicates) {
    ECCLESIA_ASSIGN_OR_RETURN(PredicateObject encoded_predicate,
                              CreatePredicateObject(predicate));
    filter_strings.push_back(GenerateFilterString(encoded_predicate));
  }
  return absl::StrJoin(filter_strings, "%20or%20");
}

}  // namespace ecclesia
