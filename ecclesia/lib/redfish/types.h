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

#ifndef ECCLESIA_LIB_REDFISH_TYPES_H_
#define ECCLESIA_LIB_REDFISH_TYPES_H_

#include <optional>
#include <string>

#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/interface.h"

namespace ecclesia {

enum NodeType { kBoard, kConnector, kDevice, kCable };

// Resource type and version, as obtained through @odata.type.
struct ResourceTypeAndVersion {
  const std::string resource_type;
  const std::string version;
};

// Parses resource type and version from @odata.type with format
// "#<Resource>.v<version>.<Resource>"
std::optional<ResourceTypeAndVersion> GetResourceTypeAndVersionFromOdataType(
    absl::string_view type);

// Helper function to work with RedfishObject directly.
std::optional<ResourceTypeAndVersion> GetResourceTypeAndVersionForObject(
    const RedfishObject &);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_TYPES_H_
