/*
 * Copyright 2022 Google LLC
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

#include "ecclesia/lib/strings/case.h"

#include <optional>
#include <string>

#include "absl/strings/ascii.h"
#include "absl/strings/string_view.h"

namespace ecclesia {

std::optional<std::string> CamelcaseToSnakecase(absl::string_view name) {
  std::string snake_case;
  for (char c : name) {
    if (!absl::ascii_isalpha(c)) return std::nullopt;
    if (absl::ascii_isupper(c)) {
      if (!snake_case.empty()) snake_case.push_back('_');
      snake_case.push_back(absl::ascii_tolower(c));
    } else {
      snake_case.push_back(c);
    }
  }
  return snake_case;
}

std::optional<std::string> SnakecaseToCamelcase(absl::string_view name) {
  std::string camel_case;
  bool next_to_upper = true;
  for (char c : name) {
    if (!absl::ascii_isalpha(c) && c != '_') return std::nullopt;
    if (!absl::ascii_islower(c) && c != '_') return std::nullopt;
    if (c == '_') {
      next_to_upper = true;
    } else if (next_to_upper) {
      camel_case.push_back(absl::ascii_toupper(c));
      next_to_upper = false;
    } else {
      camel_case.push_back(c);
    }
  }
  return camel_case;
}

}  // namespace ecclesia
