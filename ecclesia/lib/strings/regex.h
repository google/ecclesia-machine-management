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

#ifndef ECCLESIA_LIB_STRINGS_REGEX_H_
#define ECCLESIA_LIB_STRINGS_REGEX_H_

#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "re2/re2.h"

namespace ecclesia {
// Apply an RE2::FullMatch with the given str and pattern, capture the groups
// and return a tuple with types defined by ArgTypes if matches, otherwise
// return nullopt.
template <typename... ArgTypes>
absl::optional<std::tuple<ArgTypes...>> RegexFullMatch(absl::string_view str,
                                                       const RE2 &pattern) {
  std::tuple<ArgTypes...> values;
  if (std::apply(
          [&](ArgTypes &&...params) {
            return RE2::FullMatch(str, pattern, &params...);
          },
          std::move(values))) {
    return values;
  }
  return absl::nullopt;
}
}  // namespace ecclesia

#endif  // ECCLESIA_LIB_STRINGS_REGEX_H_
