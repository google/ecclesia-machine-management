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

#include "ecclesia/lib/redfish/interface.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "re2/re2.h"

// Pattern for expression [lhs][operator][rhs]
constexpr LazyRE2 kRelationalExpressionRegex = {
    "^(?P<left>[^\\s<=>!]+)(?:(<=|>=|!=|>|<|=)(?P<right>[^<=>!]+))$"};


namespace ecclesia {
namespace {

// A simple structure containing the information in a relational expression.
struct RelationalExpression {
  std::string lhs;
  std::string rel_operator;
  std::string rhs;
};

// Contains all of the information contained in a predicate. The ordering of the
// operators and the expressions are important. Each logical operator goes
// between two expressions. Here is a visualization:
// logical_operators: {and, or}
// expressions: {exp1, exp2, exp3}
// This would turn into "exp1 and exp2 or exp3".
struct EncodedPredicate {
  std::vector<std::string> logical_operators;
  std::vector<RelationalExpression> expressions;
};

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

// Takes a Redpath format predicate and turns it into a machine readable object
// that will be used later for $filter transforms.
// Example: "Prop1<=42 or Prop1>84"
//   logical_operators = [" or "]
//   expressions = [[lhs: "Prop1", rel_operator: "<=", rhs: "42"],
//                  [lhs: "Prop1", rel_operator: ">", rhs: "84"]
//                 ]
absl::StatusOr<EncodedPredicate> EncodePredicate(absl::string_view predicate) {
  // The high level logic of this function is to break down the predicate into
  // logical operators (or/and) and the relational expressions (prop>value).
  // After breaking them up go through the relational expressions and break them
  // down into parts and turn them into objects.
  EncodedPredicate encoded_predicate;
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
          encoded_predicate.logical_operators.push_back(" and ");
        }
      }
    } else {
      logical_split.push_back(block);
    }
    if (or_count > 0) {
      or_count--;
      encoded_predicate.logical_operators.push_back(" or ");
    }
  }
  // Go through relational expressions and encode them.
  for (const std::string &expression : logical_split) {
    auto encoded_expression = EncodeRelationalExpression(expression);
    if (encoded_expression.ok()) {
      encoded_predicate.expressions.push_back(encoded_expression.value());
    } else {
      // Invalid expression, return an error
      return absl::InvalidArgumentError(encoded_expression.status().message());
    }
  }
  return encoded_predicate;
}

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
std::string GenerateFilterString(const EncodedPredicate &predicate_object) {
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

void RedfishQueryParamFilter::BuildFromRedpathPredicate(
    absl::string_view predicate) {
  auto encoded_predicate = EncodePredicate(predicate);
  if (!encoded_predicate.ok()) {
    // Invalid predicate, set filter_string_ to empty, indicating an invalid
    // expression, and return.
    filter_string_ = "";
    return;
  }
  filter_string_ = GenerateFilterString(*encoded_predicate);
}

void RedfishQueryParamFilter::BuildFromRedpathPredicateList(
    const std::vector<std::string> &predicates) {
  std::vector<std::string> filter_strings;
  filter_strings.reserve(predicates.size());
  for (absl::string_view predicate : predicates) {
    auto encoded_predicate = EncodePredicate(predicate);
    if (!encoded_predicate.ok()) {
      // Invalid predicate, set filter_string_ to empty, indicating an invalid
      // expression, and return.
      filter_string_ = "";
      return;
    }
    filter_strings.push_back(GenerateFilterString(*encoded_predicate));
  }
  filter_string_ = absl::StrJoin(filter_strings, "%20or%20");
}

RedfishQueryParamTop::RedfishQueryParamTop(
    size_t numMembers)
    : numMembers_(numMembers) {}

std::string RedfishQueryParamTop::ToString() const {
  return absl::StrCat("$top=", numMembers_);
}

absl::Status RedfishQueryParamTop::ValidateRedfishSupport(
    const absl::optional<RedfishSupportedFeatures> &features) {
  if (!features.has_value()) {
    return absl::InternalError("Top query parameter is not supported.");
  }
  if (!features->top_skip.enable) {
    return absl::InternalError("'$top' and '$skip' are not supported");
  }
  return absl::OkStatus();
}

RedfishQueryParamExpand::RedfishQueryParamExpand(
    RedfishQueryParamExpand::Params params)
    : type_(params.type), levels_(params.levels) {}

std::string RedfishQueryParamExpand::ToString() const {
  std::string expand_type;
  switch (type_) {
    case ExpandType::kBoth:
      expand_type = "*";
      break;
    case ExpandType::kNotLinks:
      expand_type = ".";
      break;
    case ExpandType::kLinks:
      expand_type = "~";
      break;
  }
  return absl::StrCat("$expand=", expand_type, "($levels=", levels_, ")");
}

absl::Status RedfishQueryParamExpand::ValidateRedfishSupport(
    const absl::optional<RedfishSupportedFeatures> &features) const {
  if (!features.has_value()) {
    return absl::InternalError("Expands are not supported.");
  }
  std::string expand_type;
  switch (type_) {
    case ExpandType::kBoth:
      if (!features->expand.expand_all) {
        return absl::InternalError("'expand_all' is not supported");
      }
      break;
    case ExpandType::kNotLinks:
      if (!features->expand.no_links) {
        return absl::InternalError("'no_links' is not supported");
      }
      break;
    case ExpandType::kLinks:
      if (!features->expand.links) {
        return absl::InternalError("'links' is not supported");
      }
      break;
  }
  if (levels_ > features->expand.max_levels) {
    return absl::InternalError(
        "number of levels exceed max levels set in redfish features");
  }
  return absl::OkStatus();
}
std::unique_ptr<RedfishObject> RedfishVariant::AsFreshObject() const {
  if (!ptr_) return nullptr;
  std::unique_ptr<RedfishObject> obj = ptr_->AsObject();
  if (!obj) return nullptr;
  return obj->EnsureFreshPayload().value_or(nullptr);
}

}  // namespace ecclesia
