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

#ifndef ECCLESIA_LIB_STRINGS_STRING_VIEW_H_
#define ECCLESIA_LIB_STRINGS_STRING_VIEW_H_

#include <cstddef>

#include "absl/strings/string_view.h"

namespace ecclesia {

// Construct a constexpr string_view from a string literal. This is intended to
// provide something functionally similar to what actual string_view literals
// can do, in particular easily constructing a string_view that contains
// embedded NUL characters. If you were to write string_view("a\0b\0c\0") then
// you would get a view of length 1 and not the view of length 6 that you were
// likely trying to construct.
//
// Note that this function is designed specifically to be used with literal
// strings and so it will remove the implicit trailing NUL. This allows for
// StringViewLiteral("a\0b\0c\0") to produce the same result as "a\0b\0c\0"sv
// but it also means that if you use it with an actual char array you may get an
// unexpected trunctation.
template <size_t N>
constexpr absl::string_view StringViewLiteral(const char (&literal)[N]) {
  static_assert(N > 0);  // Literals should always have a terminating NUL.
  return absl::string_view(literal, N - 1);
}

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_STRINGS_STRING_VIEW_H_
