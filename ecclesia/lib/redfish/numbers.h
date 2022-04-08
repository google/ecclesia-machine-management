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

#ifndef ECCLESIA_LIB_REDFISH_NUMBERS_H_
#define ECCLESIA_LIB_REDFISH_NUMBERS_H_

#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>

#include "absl/strings/numbers.h"
#include "ecclesia/lib/redfish/interface.h"

namespace ecclesia {

// Read string field from a redfish object and convert it to int32_t
// the format in the string is not limited.
template <typename ResourceT,
          std::enable_if_t<
              std::is_same_v<typename ResourceT::type, std::string>, int> = 0>
inline std::optional<int32_t> RedfishStrTo32Base(const RedfishObject &obj) {
  auto maybe_value = obj.GetNodeValue<ResourceT>();
  int32_t number;
  if (!maybe_value.has_value() ||
      !absl::numbers_internal::safe_strto32_base(*maybe_value, &number, 0)) {
    return std::nullopt;
  }
  return number;
}

// Read string field from a redfish object and convert it to a specified int
// type. The string format is assumed to be a hex string.
template <typename PropertyT, typename IntType,
          std::enable_if_t<
              std::is_same_v<typename PropertyT::type, std::string>, int> = 0>
inline std::optional<IntType> RedfishHexStrPropertyToInt(
    const RedfishObject &obj) {
  auto maybe_value = obj.GetNodeValue<PropertyT>();
  IntType number;
  if (!maybe_value.has_value() ||
      !absl::numbers_internal::safe_strtoi_base(*maybe_value, &number, 16)) {
    return std::nullopt;
  }
  return number;
}

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_NUMBERS_H_
