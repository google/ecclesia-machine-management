/*
 * Copyright 2024 Google LLC
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

#ifndef ECCLESIA_LIB_REDFISH_LOCATION_H_
#define ECCLESIA_LIB_REDFISH_LOCATION_H_

#include <optional>
#include <string>

#include "ecclesia/lib/redfish/interface.h"

namespace ecclesia {

// Additional Location info from Redfish URI(s) associated with a Node.
struct SupplementalLocationInfo {
  // Location.PartLocation.ServiceLabel from DMTF Redfish schema.
  std::string service_label;
  // Location.PartLocationContext from DMTF Redfish schema.
  std::string part_location_context;
  bool operator==(const SupplementalLocationInfo &other) const {
    return service_label == other.service_label &&
           part_location_context == other.part_location_context;
  }
};

std::optional<SupplementalLocationInfo> GetSupplementalLocationInfo(
    const RedfishObject &obj);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_LOCATION_H_
