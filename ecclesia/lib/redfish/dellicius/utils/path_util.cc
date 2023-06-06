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

#include "ecclesia/lib/redfish/dellicius/utils/path_util.h"

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/interface.h"
#include "re2/re2.h"

namespace ecclesia {

namespace {

// Pattern for valid segment within a property path
// Example:
//   <VALID> properties { property: "outer.inner[2].child" type: STRING }
//   <INVALID> properties { property: "outer.inner.2.child" type: STRING }
//   The second example has segments "outer", "inner", "2" and "child" where "2"
//   is an invalid segment.
constexpr LazyRE2 kValidPropertyPathSegment = {
    "^([a-zA-Z#@][0-9a-zA-Z#@.]+)(?:\\[([0-9]+)\\]|)$"};

std::optional<std::pair<std::string, int>> SplitNodeNameIfArrayType(
    absl::string_view node_name) {
  std::string string_index;
  int index;
  std::string node_name_stripped;
  if (!RE2::FullMatch(node_name, *kValidPropertyPathSegment,
                      &node_name_stripped, &string_index)) {
    return std::nullopt;
  }

  if (!absl::SimpleAtoi(string_index, &index)) {
    return std::nullopt;
  }

  return std::pair<std::string, int>(node_name_stripped, index);
}

}  // namespace

std::vector<absl::string_view> SplitExprByDelimiterWithEscape(
    absl::string_view expression, absl::string_view delimiter,
    char escape_character) {
  std::vector<absl::string_view> split_expressions;
  expression = absl::StripAsciiWhitespace(expression);
  if (expression.empty()) return split_expressions;

  size_t start_of_expr = 0;
  size_t next_separator_at = expression.find(delimiter, 0);

  // If first character itself is the delimiter, skip it. Note if delimiter is
  // ' ' and is first character then it would have been stripped already by
  // StripAsciiWhitespace().
  if (next_separator_at == 0) {
    next_separator_at = expression.find(delimiter, next_separator_at + 1);
    ++start_of_expr;
  }

  while (next_separator_at != std::string::npos) {
    if (expression[next_separator_at - 1] != escape_character) {
      split_expressions.push_back(
          expression.substr(start_of_expr, next_separator_at - start_of_expr));
      start_of_expr = next_separator_at + 1;
    }
    next_separator_at = expression.find(delimiter, next_separator_at + 1);
  }
  split_expressions.push_back(expression.substr(start_of_expr));
  return split_expressions;
}

std::vector<std::string> SplitNodeNameForNestedNodes(
    absl::string_view expression) {
  std::vector<absl::string_view> node_names =
      SplitExprByDelimiterWithEscape(expression, ".", '\\');
  std::vector<std::string> node_names_without_escape = {node_names.begin(),
                                                        node_names.end()};
  // Strip escape characters from each expression.
  for (std::string &node : node_names_without_escape) {
    absl::StrReplaceAll({{"\\", ""}}, &node);
  }
  return node_names_without_escape;
}

absl::StatusOr<nlohmann::json> ResolveNodeNameToJsonObj(
    const RedfishObject &redfish_object, absl::string_view node_name) {
  std::vector<std::string> node_names = SplitNodeNameForNestedNodes(node_name);
  if (node_names.empty()) {
    return absl::InternalError("Given NodeName is empty or invalid.");
  }
  nlohmann::json json_obj = redfish_object.GetContentAsJson();
  // If given expression has multiple nodes, we need to return the json object
  // associated with the leaf node.
  for (auto &name : node_names) {
    // If name referencing array, split name into array_name and array_index
    int index = -1;
    std::optional<std::pair<std::string, int>> name_and_index =
        SplitNodeNameIfArrayType(name);
    if (name_and_index.has_value()) {
      name = name_and_index->first;
      index = name_and_index->second;
    }
    if (!json_obj.contains(name)) {
      return absl::InternalError(
          absl::StrFormat("Node %s not found in json object", name));
    }
    json_obj = json_obj.at(name);

    // If we have a valid index, refine further
    if (index >= 0) {
      json_obj = json_obj[index];
    }
  }
  return json_obj;
}

}  // namespace ecclesia
