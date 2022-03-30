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

#ifndef ECCLESIA_LIB_STRINGS_CASE_H_
#define ECCLESIA_LIB_STRINGS_CASE_H_

#include <optional>
#include <string>

#include "absl/strings/string_view.h"

namespace ecclesia {

// Given an alpha string, translate it between CamelCase and snake_case.
// If the given string does not appear to fit the expected input format then
// nullopt is returned.
std::optional<std::string> CamelcaseToSnakecase(absl::string_view name);

// Given an alpha string, translate it between snake_case and CamelCase.
// If the given string does not appear to fit the expected input format then
// nullopt is returned.
std::optional<std::string> SnakecaseToCamelcase(absl::string_view name);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_STRINGS_CASE_H_
