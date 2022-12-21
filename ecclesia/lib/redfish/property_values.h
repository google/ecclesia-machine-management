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

namespace ecclesia {
inline constexpr char kReadingTypePower[] = "Power";
inline constexpr char kReadingTypeRotational[] = "Rotational";
inline constexpr char kReadingTypeTemperature[] = "Temperature";
inline constexpr char kReadingTypeVoltage[] = "Voltage";
inline constexpr char kReadingTypeCurrent[] = "Current";
inline constexpr char kReadingTypePercent[] = "Percent";
inline constexpr char kReadingTypeFrequency[] = "Frequency";
inline constexpr char kReadingTypeEnergy[] = "Energy";
inline constexpr char kReadingUnitW[] = "W";
inline constexpr char kReadingUnitRpm[] = "RPM";
inline constexpr char kReadingUnitCel[] = "Cel";
inline constexpr char kReadingUnitV[] = "V";
inline constexpr char kReadingUnitA[] = "A";
inline constexpr char kReadingUnitPercent[] = "%";
inline constexpr char kReadingUnitJoules[] = "J";
inline constexpr char kReadingUnitHertz[] = "Hz";
inline constexpr char kHealthOk[] = "OK";
inline constexpr char kStateEnabled[] = "Enabled";
inline constexpr char kStateDisabled[] = "Disabled";
inline constexpr char kStateAbsent[] = "Absent";
inline constexpr char kStateUnavailableOffline[] = "UnavailableOffline";
inline constexpr char kEntryTypeOem[] = "Oem";
inline constexpr char kOemBareMetalReady[] = "BareMetalReady";
inline constexpr char kOemRecordFormatBmc[] = "BMC Chassis Entry";
inline constexpr char kCertificateTypePEM[] = "PEM";
inline constexpr char kCertificateTypePEMchain[] = "PEMchain";
inline constexpr char kPowerStateOn[] = "On";
inline constexpr char kPowerStateOff[] = "Off";
inline constexpr char kPowerStatePaused[] = "Paused";
inline constexpr char kPowerStatePoweringOn[] = "PoweringOn";
inline constexpr char kPowerStatePoweringOff[] = "PoweringOff";
inline constexpr char kProcessorTypeAccelerator[] = "Accelerator";
inline constexpr char kProcessorTypeCpu[] = "CPU";
inline constexpr char kProcessorTypeGPU[] = "GPU";
inline constexpr char kSwitchTypeNvLink[] = "NVLink";
}  // namespace ecclesia

namespace ecclesia {

// Value definitions for the Oem.Google.TopologyRepresentation field.
inline constexpr char kTopologyRepresentationV1[] = "redfish-devpath-v1";
inline constexpr char kTopologyRepresentationV2[] = "redfish-devpath-v2";

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_PROPERTY_VALUES_H_
