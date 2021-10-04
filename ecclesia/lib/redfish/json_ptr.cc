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

#include "ecclesia/lib/logging/logging.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {

nlohmann::json HandleJsonPtr(nlohmann::json json,
                             absl::string_view pointer_str) {
  try {
    nlohmann::json::json_pointer ptr((std::string(pointer_str)));
    return json.at(ptr);
  } catch (...) {
    // We use a general catch for any exceptions from parsing the json_pointer
    // and accessing the json with it. We avoid specific exception catching in
    // case nlohmann::json ever gets updated with new exceptions being thrown
    // to avoid accidentally breaking dependents of this code. In general,
    // we treat any exceptions thrown (bad pointer syntax, pointer reference not
    // found, etc.) as a failure to handle the JSON pointer and return the same
    // discarded value in all cases.
    return nlohmann::json::value_t::discarded;
  }
}

}  // namespace ecclesia
