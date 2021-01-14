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

#include <cstdint>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "re2/re2.h"

namespace ecclesia {

const RE2 kPciLocationRegexp = {
    R"(([0-9a-fA-F]{4}):([0-9a-fA-F]{2}):([0-9a-fA-F]{2}).([0-7]{1}))"};

absl::optional<PciLocation> PciLocation::FromString(absl::string_view dev_str) {
  uint32_t domain;
  uint32_t bus;
  uint32_t device;
  uint32_t function;
  if (!RE2::FullMatch(dev_str, kPciLocationRegexp, RE2::Hex(&domain),
                      RE2::Hex(&bus), RE2::Hex(&device), RE2::Hex(&function))) {
    return absl::nullopt;
  }

  auto maybe_domain = PciDomain::TryMake(domain);
  auto maybe_bus = PciBusNum::TryMake(bus);
  auto maybe_device = PciDeviceNum::TryMake(device);
  auto maybe_function = PciFunctionNum::TryMake(function);

  if (!maybe_domain.has_value() || !maybe_bus.has_value() ||
      !maybe_device.has_value() || !maybe_function.has_value()) {
    return absl::nullopt;
  }

  return PciLocation(maybe_domain.value(), maybe_bus.value(),
                     maybe_device.value(), maybe_function.value());
}
}  // namespace ecclesia
