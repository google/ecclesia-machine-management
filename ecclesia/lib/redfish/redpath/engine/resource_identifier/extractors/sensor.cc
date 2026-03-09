/*
 * Copyright 2026 Google LLC
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

#include "ecclesia/lib/redfish/redpath/engine/resource_identifier/extractors/sensor.h"

#include <optional>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/property_definitions.h"
#include "ecclesia/lib/redfish/redpath/engine/resource_identifier/extractor.h"

namespace ecclesia {

absl::StatusOr<IdentifierExtractorIntf::ResourceIdentifier>
SensorIdentifierExtractor::Extract(const RedfishObject& obj,
                                   const Deps& deps) const {
  IdentifierExtractorIntf::ResourceIdentifier identifier;
  if (absl::StatusOr<IdentifierExtractorIntf::ResourceIdentifier>
          related_item_identifier = related_item_extractor_.Extract(obj, deps);
      related_item_identifier.ok()) {
    identifier.identifier = std::move(related_item_identifier->identifier);
  }

  std::string resource_type;
  // This function is called unconditionally, therefore short-circuit
  // inexpensively if this object is not a Sensor.
  if (!obj.Get(ecclesia::PropertyOdataType::Name).GetValue(&resource_type) ||
      !absl::StrContains(resource_type, ResourceSensor::Name)) {
    return absl::NotFoundError(absl::StrCat(
        "Object is not a ", kRfPropertySensors, ": ", resource_type));
  }
  CollectedProperty::SensorIdentifier sensor_identifier;
  std::optional<std::string> name = obj.GetNodeValue<PropertyName>();
  if (name.has_value()) {
    sensor_identifier.set_name(*name);
  }
  std::optional<std::string> reading_type =
      obj.GetNodeValue<PropertyReadingType>();
  if (reading_type.has_value()) {
    sensor_identifier.set_reading_type(*reading_type);
  }
  std::optional<std::string> reading_units =
      obj.GetNodeValue<PropertyReadingUnits>();
  if (reading_units.has_value()) {
    sensor_identifier.set_reading_units(*reading_units);
  }
  identifier.sensor_identifier = std::move(sensor_identifier);

  return identifier;
}

}  // namespace ecclesia
