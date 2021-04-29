/*
 * Copyright 2021 Google LLC
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

#ifndef ECCLESIA_LIB_REDFISH_PROPERTY_VALUES_H_
#define ECCLESIA_LIB_REDFISH_PROPERTY_VALUES_H_

namespace libredfish {
inline constexpr char kReadingTypePower[] = "Power";
inline constexpr char kReadingTypeRotational[] = "Rotational";
inline constexpr char kReadingTypeTemperature[] = "Temperature";
inline constexpr char kReadingUnitW[] = "W";
inline constexpr char kReadingUnitRpm[] = "RPM";
inline constexpr char kReadingUnitCel[] = "Cel";
inline constexpr char kHealthOk[] = "OK";
inline constexpr char kStateEnabled[] = "Enabled";
inline constexpr char kStateAbsent[] = "Absent";
}  // namespace libredfish

namespace ecclesia {

// Value definitions for the Oem.Google.TopologyRepresentation field.
inline constexpr char kTopologyRepresentationV1[] = "redfish-devpath-v1";

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_PROPERTY_VALUES_H_
