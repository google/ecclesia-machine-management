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

#ifndef ECCLESIA_MAGENT_REDFISH_CORE_URI_GENERATION_H_
#define ECCLESIA_MAGENT_REDFISH_CORE_URI_GENERATION_H_

#include <string>

#include "ecclesia/lib/io/pci/location.h"

namespace ecclesia {

// Construct the PCIeFunction URI from the BDF and Redfish Schema
std::string GetPcieFunctionUri(const PciLocation& location);

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_REDFISH_CORE_URI_GENERATION_H_
