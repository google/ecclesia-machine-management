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
#include <vector>

#include "absl/strings/ascii.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/property_definitions.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/redfish/types.h"

namespace ecclesia {
namespace {
template <typename P>
std::optional<std::string> GetConvertedResourceType(const RedfishObject &node) {
  const std::optional<ResourceTypeAndVersion> resource_type_and_version =
      GetResourceTypeAndVersionForObject(node);
  // Specially get the resource name for certain resource types.
  if (resource_type_and_version.has_value()) {
    if (resource_type_and_version->resource_type == "Memory") {
      // For memory resource, extract the "MemoryDeviceType" field for the
      // corresponding resource name, e.g., "ddr4", "ddr5".
      if (const std::optional<std::string> mem_device_type =
              node.GetNodeValue<PropertyMemoryDeviceType>();
          mem_device_type.has_value() &&
          mem_device_type->substr(0, 3) == "DDR") {
        return absl::AsciiStrToLower(mem_device_type->substr(0, 4));
      }
    }
  }
  // Generally get the resource name by directly converting the P property.
  const auto property_name = node.GetNodeValue<P>();
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
}  // namespace

std::optional<std::string> GetConvertedResourceName(const RedfishObject &node) {
  return GetConvertedResourceType<PropertyName>(node);
}

std::optional<std::string> GetConvertedResourceModel(
    const RedfishObject &node) {
  return GetConvertedResourceType<PropertyModel>(node);
}

// This function trims the string's suffix of last underscore and the numeric
// following it, if such a valid suffix is present in the input string.
std::string TruncateLastUnderScoreAndNumericSuffix(absl::string_view str) {
  size_t last_underscore_index = str.find_last_of('_');
  int numeric_value;
  std::string modified_str;
  if ((last_underscore_index != std::string::npos) &&
      (last_underscore_index < (str.length() - 1)) &&
      absl::SimpleAtoi(
          str.substr(last_underscore_index + 1, str.length() - 1),
                      &numeric_value)) {
    modified_str = std::string(str.substr(0, last_underscore_index));
  } else {
    modified_str = std::string(str);
  }
  return modified_str;
}

std::string RedfishTransportBytesToString(
    const RedfishTransport::bytes &bytes) {
  return std::string(bytes.begin(), bytes.end());
}

RedfishTransport::bytes GetBytesFromString(absl::string_view str) {
  return RedfishTransport::bytes(str.begin(), str.end());
}

// Returns the URI from Json object or NotFound error
absl::StatusOr<std::string> GetObjectUri(const nlohmann::json &json) {
  std::string reference;
  if (json.is_object()) {
    if (auto odata = json.find(PropertyOdataId::Name);
        // Object can be re-read by URI. Store it.
        odata != json.end() && odata.value().is_string()) {
      return odata.value().get<std::string>();
    }
  }
  return absl::NotFoundError("Unable to find @odata.id");
}

// Extends uri with query parameters if needed
std::string GetUriWithQueryParameters(absl::string_view uri,
                                      const GetParams &params) {
  auto query_params = params.GetQueryParams();
  if (query_params.empty()) {
    return std::string(uri);
  }
  std::string query_str = absl::StrJoin(
      query_params.begin(), query_params.end(), "&",
      [](std::string *output, const GetParamQueryInterface *param) {
        output->append(param->ToString());
      });
  return absl::StrCat(uri, "?", query_str);
}

}  // namespace ecclesia
