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

#include "ecclesia/lib/redfish/location.h"

#include <memory>
#include <optional>
#include <string>

#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/property_definitions.h"

namespace ecclesia {

std::optional<SupplementalLocationInfo> GetSupplementalLocationInfo(
    const RedfishObject &obj) {
  std::unique_ptr<RedfishObject> location_obj =
      obj[kRfPropertyLocation].AsObject();
  // If no Location info is present, try populating via PhysicalLocation
  // instead.
  if (location_obj == nullptr) {
    location_obj = obj[kRfPropertyPhysicalLocation].AsObject();
  }
  if (location_obj != nullptr) {
    std::optional<std::string> part_location_context =
        location_obj->GetNodeValue<PropertyPartLocationContext>();
    std::unique_ptr<RedfishObject> part_location_obj =
        (*location_obj)[kRfPropertyPartLocation].AsObject();
    if (part_location_obj != nullptr) {
      std::optional<std::string> service_label =
          part_location_obj->GetNodeValue<PropertyServiceLabel>();
      if (service_label.has_value() || part_location_context.has_value()) {
        SupplementalLocationInfo stable_id = {
            .service_label = service_label.value_or(""),
            .part_location_context = part_location_context.value_or("")};
        return stable_id;
      }
    }
  }
  return std::nullopt;
}

}  // namespace ecclesia
