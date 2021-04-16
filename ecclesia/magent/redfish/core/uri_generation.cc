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

#include "ecclesia/magent/redfish/core/uri_generation.h"

#include <string>

#include "absl/strings/str_format.h"
#include "absl/strings/str_replace.h"
#include "ecclesia/lib/io/pci/location.h"
#include "ecclesia/magent/redfish/core/redfish_keywords.h"

namespace ecclesia {

std::string GetPcieFunctionUri(const PciDbdfLocation& location) {
  const std::string pcie_device_str =
      absl::StrFormat("%04x:%02x:%02x", location.domain().value(),
                      location.bus().value(), location.device().value());
  return absl::StrReplaceAll(
      kPCIeFunctionUriPattern,
      {{"([\\w:]+)", pcie_device_str},
       {"(\\d+)", std::to_string(location.function().value())}});
}

}  // namespace ecclesia
