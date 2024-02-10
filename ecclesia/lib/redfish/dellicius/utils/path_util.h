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

#ifndef ECCLESIA_LIB_REDFISH_DELLICIUS_UTILS_PATH_UTIL_H_
#define ECCLESIA_LIB_REDFISH_DELLICIUS_UTILS_PATH_UTIL_H_

#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/interface.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {

// Splits a given expression by given delimiter.
// A delimiter is skipped when escaped using given escape_character.
// Example:
//   expression = "Hello\\ world test"
//   SplitExprByDelimiterWithEscape(expression, " ", '\\')
//     returns {"Hello\\ world", "test"}
std::vector<absl::string_view> SplitExprByDelimiterWithEscape(
    absl::string_view expression, absl::string_view delimiter,
    char escape_character);

// Gets all the node names in the given expression.
// parent.child.grandchild -> {parent, child, grandchild}
std::vector<std::string> SplitNodeNameForNestedNodes(
    absl::string_view expression);

// Helper function to resolve a RedPath node relative to given JSON.
// Returns JSON object referenced by the node expression.
// Example:
//  ResolveRedPathNodeToJson(some_json_object, Threshold.UpperCritial.Reading)
//  would return JSON object {"Reading": 23} nested in some_json_object.
absl::StatusOr<nlohmann::json> ResolveRedPathNodeToJson(
    const nlohmann::json &json_object, absl::string_view node_name);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_DELLICIUS_UTILS_PATH_UTIL_H_
