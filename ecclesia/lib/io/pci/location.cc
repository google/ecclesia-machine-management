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

#include "ecclesia/lib/io/pci/location.h"

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "re2/re2.h"

namespace ecclesia {

absl::optional<PciLocation> PciLocation::FromString(absl::string_view str) {
  static constexpr LazyRE2 kRegex = {
      R"(([[:xdigit:]]{4}):([[:xdigit:]]{2}):([[:xdigit:]]{2})\.([0-7]{1}))"};

  int domain, bus, device, function;
  if (!RE2::FullMatch(str, *kRegex, RE2::Hex(&domain), RE2::Hex(&bus),
                      RE2::Hex(&device), RE2::Hex(&function))) {
    return absl::nullopt;
  }
  return PciLocation::TryMake(domain, bus, device, function);
}

absl::optional<PciDeviceLocation> PciDeviceLocation::FromString(
    absl::string_view str) {
  static constexpr LazyRE2 kRegex = {
      R"(([[:xdigit:]]{4}):([[:xdigit:]]{2}):([[:xdigit:]]{2}))"};

  int domain, bus, device;
  if (!RE2::FullMatch(str, *kRegex, RE2::Hex(&domain), RE2::Hex(&bus),
                      RE2::Hex(&device))) {
    return absl::nullopt;
  }
  return PciDeviceLocation::TryMake(domain, bus, device);
}

}  // namespace ecclesia
