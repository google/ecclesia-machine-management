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

#include "ecclesia/lib/redfish/storage.h"

#include <memory>
#include <optional>
#include <vector>

#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/property_definitions.h"

namespace ecclesia {

std::vector<SmartReading> ReadSmartData(const RedfishObject &obj) {
  std::vector<SmartReading> readings = {
      {"critical_warning",
       obj.GetNodeValue<OemGooglePropertyCriticalWarning>()},
      {"composite_temperature_kelvins",
       obj.GetNodeValue<OemGooglePropertyCompositeTemperatureKelvins>()},
      {"available_spare", obj.GetNodeValue<OemGooglePropertyAvailableSpare>()},
      {"available_spare_threshold",
       obj.GetNodeValue<OemGooglePropertyAvailableSpareThreshold>()},
      {"critical_comp_time",
       obj.GetNodeValue<OemGooglePropertyCriticalTemperatureTimeMinute>()}};
  return readings;
}

std::optional<std::vector<SmartReading>> ReadSmartDataFromStorageController(
    const RedfishObject &obj) {
  auto smart_attributes_obj =
      obj[kRfPropertyOem][kRfOemPropertyGoogle][kRfOemPropertySmartAttributes]
          .AsObject();
  if (!smart_attributes_obj) return std::nullopt;

  return ReadSmartData(*smart_attributes_obj);
}

}  // namespace ecclesia
