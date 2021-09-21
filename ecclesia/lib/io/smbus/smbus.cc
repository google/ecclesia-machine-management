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

#include "ecclesia/lib/io/smbus/smbus.h"

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "re2/re2.h"

namespace ecclesia {

absl::optional<SmbusLocation> SmbusLocation::FromString(
    absl::string_view smbus_location) {
  static constexpr LazyRE2 kRegex = {"(\\d+)-([[:xdigit:]]{2})$"};
  int bus_num, address_num;
  if (!RE2::FullMatch(smbus_location, *kRegex, RE2::Arg(&bus_num),
                      RE2::Hex(&address_num))) {
    return absl::nullopt;
  }
  auto bus = SmbusBus::TryMake(bus_num);
  auto address = SmbusAddress::TryMake(address_num);
  if (!bus.has_value() || !address.has_value()) {
    return absl::nullopt;
  }
  return SmbusLocation(*bus, *address);
}

}  // namespace ecclesia
