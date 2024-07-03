/*
 * Copyright 2024 Google LLC
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

#ifndef ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_PREDICATES_UTIL_H_
#define ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_PREDICATES_UTIL_H_

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace ecclesia {

// This method walks both input strings and compares the characters one by one.
// For non-numeric portions, it does lexicographic comparison. For number
// portions, it does standard numeric comparisons. returns an int x where:
//  x < 0 if lhs ~< rhs,
//  x > 0 if lhs >~ rhs,
//  x = 0 if lhs == rhs.
absl::StatusOr<int> FuzzyStringComparison(absl::string_view lhs,
                                          absl::string_view rhs);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_PREDICATES_UTIL_H_
