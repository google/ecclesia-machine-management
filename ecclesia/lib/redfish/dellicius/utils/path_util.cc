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
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/interface.h"

namespace ecclesia {

std::vector<std::string> SplitNodeNameForNestedNodes(absl::string_view expr) {
  expr = absl::StripAsciiWhitespace(expr);
  std::vector<std::string> nodes;
  if (auto pos = expr.find("@odata."); pos != std::string::npos) {
    std::string odata_str = std::string(expr.substr(pos));
    nodes = absl::StrSplit(expr.substr(0, pos), '.', absl::SkipEmpty());
    nodes.push_back(std::move(odata_str));
  } else {
    nodes = absl::StrSplit(expr, '.', absl::SkipEmpty());
  }
  return nodes;
}

absl::StatusOr<nlohmann::json> ResolveNodeNameToJsonObj(
    const RedfishVariant &variant, absl::string_view node_name) {
  std::vector<std::string> node_names = SplitNodeNameForNestedNodes(node_name);
  if (node_names.empty()) {
    return absl::InternalError("Given NodeName is empty or invalid.");
  }
  std::unique_ptr<RedfishObject> object = variant.AsObject();
  if (object == nullptr) {
    return absl::InternalError("Null Redfish object.");
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

}  // namespace ecclesia
