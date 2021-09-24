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

#ifndef ECCLESIA_LIB_REDFISH_PCI_H_
#define ECCLESIA_LIB_REDFISH_PCI_H_

#include <optional>

#include "ecclesia/lib/io/pci/location.h"
#include "ecclesia/lib/io/pci/signature.h"
#include "ecclesia/lib/redfish/interface.h"

namespace libredfish {
// Given a Redifhs node that contains PciLocation, parse the domain, bus, device
// and function field and return a ecclesia::PCiLocation.
// An example of PciLocation redfish object:
// {
//   "Domain": "0x0000",
//   "Bus": "0xda",
//   "Device": "0x00",
//   "Function": "0x0"
// }
std::optional<ecclesia::PciDbdfLocation> ReadPciLocation(
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
std::optional<ecclesia::PciFullSignature> ReadPciFullSignature(
    const RedfishObject &pcie_function_obj);

}  // namespace libredfish

#endif  // ECCLESIA_LIB_REDFISH_PCI_H_
