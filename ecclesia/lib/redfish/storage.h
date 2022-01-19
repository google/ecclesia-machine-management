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

#ifndef ECCLESIA_LIB_REDFISH_STORAGE_H_
#define ECCLESIA_LIB_REDFISH_STORAGE_H_

#include <optional>
#include <string>
#include <vector>

#include "ecclesia/lib/redfish/interface.h"

namespace ecclesia {

struct SmartReading {
  std::string name;                 // The key to be reported in the response
  std::optional<int> maybe_value;   // The value read from the redfish resource
};

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
std::optional<std::vector<SmartReading>> ReadSmartDataFromStorageController(
    const RedfishObject &obj);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_STORAGE_H_
