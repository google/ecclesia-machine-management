/*
 * Copyright 2020 Google LLC
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

#include "ecclesia/lib/redfish/utils.h"

#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include "absl/strings/ascii.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/property_definitions.h"

namespace libredfish {

std::optional<std::string> GetResourceType(const RedfishObject *node) {
  const auto odata_type = node->GetNodeValue<PropertyOdataType>();
  if (odata_type.has_value()) {
    // "@odata.type" field should be in the format
    // "#<Resource>.v<version>.<Resource>"
    std::vector<std::string> type_parts = absl::StrSplit(*odata_type, '.');

    if (type_parts.size() == 3) {
      return type_parts[2];
    }
  }
  return std::nullopt;
}

std::optional<std::string> GetConvertedResourceName(const RedfishObject *node) {
  const auto resource_type = GetResourceType(node);
  // Specially get the resource name for certain resource types.
  if (resource_type.has_value()) {
    if (*resource_type == "Memory") {
      // For memory resource, extract the "MemoryDeviceType" field for the
      // corresponding resource name, e.g., "ddr4", "ddr5".
      const auto mem_device_type =
          node->GetNodeValue<PropertyMemoryDeviceType>();
      if (mem_device_type.has_value() &&
          mem_device_type->substr(0, 3) == "DDR") {
        return absl::AsciiStrToLower(mem_device_type->substr(0, 4));
      }
    }
  }

  // Generally get the resource name by directly converting the "Name" property.
  const auto property_name = node->GetNodeValue<PropertyName>();
  if (property_name.has_value()) {
    // Strip the leading and tailing spaces.
    absl::string_view stripped_property_name =
        absl::StripAsciiWhitespace(*property_name);
    // Replace space with "_" and convert the whole string to lower cases.
    return absl::AsciiStrToLower(
        absl::StrReplaceAll(stripped_property_name, {{" ", "_"}}));
  }

  return std::nullopt;
}

}  // namespace libredfish
