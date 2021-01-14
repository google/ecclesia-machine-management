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
#include <string>
#include <vector>

#include "absl/strings/numbers.h"
#include "absl/types/optional.h"
#include "ecclesia/lib/io/pci/location.h"
#include "ecclesia/lib/io/pci/signature.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/property_definitions.h"

namespace libredfish {

struct SmartReading {
  std::string name;                 // The key to be reported in the response
  absl::optional<int> maybe_value;  // The value read from the redfish resource
};

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
inline bool ComponentIsEnabled(libredfish::RedfishObject *node_obj) {
  auto status = node_obj->GetNode(libredfish::kRfPropertyStatus).AsObject();
  if (!status) return false;
  auto state = status->GetNodeValue<libredfish::PropertyState>();
  if (!state) return false;
  return *state == "Enabled";
}

// Read string field from a redfish object and convert it to int32_t
// the format in the string is not limited.
template <typename ResourceT,
          std::enable_if_t<
              std::is_same_v<typename ResourceT::type, std::string>, int> = 0>
inline absl::optional<int32_t> RedfishStrTo32Base(const RedfishObject &obj) {
  auto maybe_value = obj.GetNodeValue<ResourceT>();
  int32_t number;
  if (!maybe_value.has_value() ||
      !absl::numbers_internal::safe_strto32_base(*maybe_value, &number, 0)) {
    return absl::nullopt;
  }
  return number;
}

// Given a Redifhs node that contains PciLocation, parse the domain, bus, device
// and function field and return a ecclesia::PCiLocation.
// An example of PciLocation redfish object:
// {
//   "Domain": "0x0000",
//   "Bus": "0xda",
//   "Device": "0x00",
//   "Function": "0x0"
// }
absl::optional<ecclesia::PciLocation> ReadPciLocation(
    const RedfishObject &pci_location_obj);

// Given a pcie function redfish object, read the vendor id, device id,
// subsystem id, subsystem vendor id and return these values as PciFullSignature
// An example of pcie function redfish object:
// {
//   "DeviceId": "0xABCD",
//   "VendorId": "0xABCD",
//   "SubsystemId": "0xABCD",
//   "SubsystemVendorId": "0xABCD",
// }
absl::optional<ecclesia::PciFullSignature> ReadPciFullSignature(
    const RedfishObject &pcie_function_obj);

// Parse Nvme smart data from SMARTAttributes redfish object
// An example of SMARTAttributes redfish object:
// {
//   "AvailableSparePercent" : 100,
//   "AvailableSparePercentThreshold" : 10,
//   "CompositeTemperatureKelvins" : 317,
//   "CriticalTemperatureTimeMinute" : 0,
//   "CriticalWarning" : 0
// }
std::vector<SmartReading> ReadSmartData(const RedfishObject &obj);

// Parse Nvme smart data from redfish storage object, the smart data is stored
// under StorageControllers.NVMeControllerProperties.Oem.Google.SMARTAttributes
absl::optional<std::vector<SmartReading>> ReadSmartDataFromStorage(
    const RedfishObject &obj);
}  // namespace libredfish

#endif
