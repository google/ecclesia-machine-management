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

#ifndef ECCLESIA_LIB_REDFISH_RESULT_H_
#define ECCLESIA_LIB_REDFISH_RESULT_H_

#include <iosfwd>
#include <string>
#include <tuple>

#include "absl/strings/string_view.h"

namespace libredfish {

// This data class contains a value and its corresponding devpath.
template <typename T>
struct Result {
  std::string devpath;
  T value;
  bool operator==(const Result &other) const {
    return std::tie(devpath, value) == std::tie(other.devpath, other.value);
  }
  bool operator!=(const Result &other) const { return *this != other; }
};

// Output stream operator.
template <typename T>
inline std::ostream &operator<<(std::ostream &os, const Result<T> &result) {
  static constexpr absl::string_view dp_prefix = "{ devpath: \"";
  static constexpr absl::string_view value_prefix = "\" value: ";
  static constexpr absl::string_view suffix = " }";
  return os << dp_prefix << result.devpath << value_prefix << result.value
            << suffix;
}

}  // namespace libredfish

#endif  // ECCLESIA_LIB_REDFISH_RESULT_H_
