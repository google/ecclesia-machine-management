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

#include "ecclesia/lib/redfish/types.h"
#include <optional>

#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"

namespace ecclesia {

std::optional<ResourceTypeAndVersion> GetResourceTypeAndVersionFromOdataType(
    absl::string_view type) {
  // Resource type should be of format "#<Resource>.v<version>.<Resource>"
  std::vector<std::string> type_parts = absl::StrSplit(type, '.');

  // `type_parts` should be 3 parts; the 2nd part should start with "v"
  if (type_parts.size() != 3) return std::nullopt;

  absl::string_view version = type_parts[1];
  if (absl::ConsumePrefix(&version, "v")) {
    return ResourceTypeAndVersion{.resource_type = type_parts[2],
                                  .version = std::string(version)};
  }
  return std::nullopt;
}

}  // namespace ecclesia
