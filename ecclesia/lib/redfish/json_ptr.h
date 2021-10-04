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

#ifndef ECCLESIA_LIB_REDFISH_JSON_PTR_H_
#define ECCLESIA_LIB_REDFISH_JSON_PTR_H_

#include "absl/strings/string_view.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {

// Resolves a JSON pointer, as defined by RFC6901.
// See https://datatracker.ietf.org/doc/html/rfc6901
// Returns a copy of the pointed-to JSON on success, json::value_t::discarded
// on failure.
nlohmann::json HandleJsonPtr(nlohmann::json json,
                             absl::string_view pointer_str);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_JSON_PTR_H_
