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

#include <memory>

#include "absl/status/status.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/interface.h"

namespace ecclesia {

std::vector<std::string> SplitNodeNameForNestedNodes(
    absl::string_view expression) {
  constexpr absl::string_view kEscapeSequenceMarker = "$$";
  expression = absl::StripAsciiWhitespace(expression);
  std::string expr =
      absl::StrReplaceAll(expression, {{"\\.", kEscapeSequenceMarker}});
  std::vector<std::string> nodes;
  nodes = absl::StrSplit(expr, '.', absl::SkipEmpty());
  for (auto &node : nodes) {
    absl::StrReplaceAll({{kEscapeSequenceMarker, "."}}, &node);
  }
  return nodes;
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
  for (auto const &name : node_names) {
    if (!json_obj.contains(name)) {
      return absl::InternalError(
          absl::StrFormat("Node %s not found in json object", name));
    }
    json_obj = json_obj.at(name);
  }
  return json_obj;
}

}  // namespace ecclesia
