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

#ifndef ECCLESIA_LIB_REDFISH_UTILS_H_
#define ECCLESIA_LIB_REDFISH_UTILS_H_

#include <memory>
#include <optional>
#include <string>

#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/property_definitions.h"
#include "ecclesia/lib/redfish/transport/interface.h"

namespace ecclesia {

// Given a Redfish node that contains a Status, return true if the status is
// “Enabled”; return false otherwise.
//
// An example of such a node is a temperature sensor:
//
// {
//     "@odata.id": "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/0",
//     "@odata.type": "#Thermal.v1_6_0.Temperature",
//     "RelatedItem": [
//         {
//             "@odata.id": "/redfish/v1/Systems/system/Memory/0"
//         }
//     ],
//     "Status": {
//         "State": "Enabled"
//     },
//     "Name": "dimm0",
//     "ReadingCelsius": 40,
//     "UpperThresholdCritical": 85
// }
inline bool ObjectIsEnabled(const RedfishObject &node_obj) {
  std::unique_ptr<RedfishObject> status =
      node_obj[kRfPropertyStatus].AsObject();
  if (!status) return false;
  std::optional<std::string> state = status->GetNodeValue<PropertyState>();
  if (!state) return false;
  return *state == "Enabled";
}

inline bool AssemblyIsEnabled(const RedfishObject &assembly_obj) {
  auto status = assembly_obj[kRfPropertyStatus].AsObject();
  // If assembly doesn't report status, treat it as enabled.
  if (!status) return true;

  auto state = status->GetNodeValue<PropertyState>();
  if (!state) return false;

  return *state == "Enabled";
}

// This helper function returns the resource type as a string by parsing the
// "@odata.type" field.
std::optional<std::string> GetResourceType(const RedfishObject *node);

// A helper function to get "converted" resource name. The name is generally
// converted from the "Name" property like " ABc XYz "->"abc_xyz". For certain
// types of resources the name is obtained from other context of the resource.
std::optional<std::string> GetConvertedResourceName(const RedfishObject &node);

std::string TruncateLastUnderScoreAndNumericSuffix(absl::string_view str);

std::string RedfishTransportBytesToString(const RedfishTransport::bytes &bytes);

RedfishTransport::bytes GetBytesFromString(absl::string_view str);

// Parses a string as JSON and returns a nlohmann::json object. This
// provides a good set of arguments to nlohmann::json::parse(). Note
// that by default nlohmann::json::parse() can throw or abort().
inline nlohmann::json ParseJson(absl::string_view json_str) {
  return nlohmann::json::parse(json_str,
                               nullptr,  // No parse callback needed
                               false);   // Not allowing exception
}

// Serializes a JSON object into a string. This provides a good set of arguments
// to nlohmann::json::dump(), which can throw or abort() by default. Invalid
// UTF-8 bytes are dropped.
inline std::string JsonToString(const nlohmann::json &data,
                                const int indent = -1) {
  return data.dump(
      indent,
      /*indent_char=*/' ',
      /*ensure_ascii=*/false,
      /*error_handler=*/nlohmann::json::error_handler_t::ignore);
}

}  // namespace ecclesia

#endif
