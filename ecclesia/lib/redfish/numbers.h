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
#include <string>
#include <type_traits>

#include "absl/strings/numbers.h"
#include "absl/types/optional.h"
#include "ecclesia/lib/redfish/interface.h"

namespace libredfish {

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

}  // namespace libredfish

#endif  // ECCLESIA_LIB_REDFISH_NUMBERS_H_
