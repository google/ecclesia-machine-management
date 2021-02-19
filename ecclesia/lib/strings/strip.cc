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

#include "ecclesia/lib/strings/strip.h"

#include "absl/strings/string_view.h"

namespace ecclesia {
namespace {
constexpr absl::string_view kWhiteSpaces = " \n\r\t\f\v";
}

absl::string_view TrimSuffixWhiteSpaces(absl::string_view in_str) {
  const auto pos = in_str.find_last_not_of(kWhiteSpaces);
  if (pos != absl::string_view::npos) {
    return in_str.substr(0, pos + 1);
  }
  return absl::string_view();
}

absl::string_view TrimPrefixWhiteSpaces(absl::string_view in_str) {
  const auto pos = in_str.find_first_not_of(kWhiteSpaces);
  if (pos != absl::string_view::npos) {
    return in_str.substr(pos);
  }
  return absl::string_view();
}
}  // namespace ecclesia
