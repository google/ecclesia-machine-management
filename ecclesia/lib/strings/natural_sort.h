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

#ifndef ECCLESIA_LIB_STRINGS_STRING_H_
#define ECCLESIA_LIB_STRINGS_STRING_H_

#include "absl/strings/string_view.h"

namespace ecclesia {

// Implements Natural Sort comparison
// (https://en.wikipedia.org/wiki/Natural_sort_order)
//
// This function compares the string parts directly;
// If there is a sequence of numbers for each these are converted to integers
// and compared.
int NaturalSortCmp(absl::string_view lhs, absl::string_view rhs);

// Less than comparator for Natural Sort
inline bool NaturalSortLessThan(absl::string_view lhs,
                                    absl::string_view rhs) {
  return NaturalSortCmp(lhs, rhs) < 0;
}

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_STRINGS_STRING_H_
