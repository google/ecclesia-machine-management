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

#include "ecclesia/lib/redfish/json_ptr.h"

#include <string>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {

// Implements RFC6901.
nlohmann::json HandleJsonPtr(nlohmann::json json,
                             absl::string_view pointer_str) {
  if (pointer_str.empty()) return json;
  // Split tokens
  std::vector<absl::string_view> tokens = absl::StrSplit(pointer_str, '/');
  // All tokens must be prefixed with "/", so element 0 will always be empty.
  if (tokens.size() < 2) return nlohmann::json::value_t::discarded;

  nlohmann::json &json_curr = json;
  // Skip the first element (the empty element 0). Use remaining tokens to
  // iterate over all elements.
  for (int i = 1; i < tokens.size(); ++i) {
    std::string token(tokens[i]);
    if (absl::StrContains(token, '~')) {
      // Handle token escape sequences.
      if (absl::StrReplaceAll({{"~0", "~"}, {"~1", "/"}}, &token) == 0) {
        // Invalid escape sequence.
        return nlohmann::json::value_t::discarded;
      }
    }

    switch (json_curr.type()) {
      case nlohmann::json::value_t::array:
        int array_index;
        if (absl::SimpleAtoi(token, &array_index) && array_index >= 0 &&
            array_index < json_curr.size()) {
          json_curr = json_curr[array_index];
          continue;
        }
        return nlohmann::json::value_t::discarded;
      case nlohmann::json::value_t::object:
        if (auto next_itr = json_curr.find(token);
            next_itr != json_curr.end()) {
          json_curr = next_itr.value();
          continue;
        }
        return nlohmann::json::value_t::discarded;
      case nlohmann::json::value_t::null:
      case nlohmann::json::value_t::string:
      case nlohmann::json::value_t::boolean:
      case nlohmann::json::value_t::number_integer:
      case nlohmann::json::value_t::number_unsigned:
      case nlohmann::json::value_t::number_float:
      case nlohmann::json::value_t::binary:
      case nlohmann::json::value_t::discarded:
      default:
        return nlohmann::json::value_t::discarded;
    }
  }

  return json_curr;
}

}  // namespace ecclesia
