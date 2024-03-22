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

#ifndef ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_PREDICATES_FILTER_H_
#define ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_PREDICATES_FILTER_H_

#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
namespace ecclesia {

// Builds a $filter query parameter string from a Redpath predicate. The output
// string is compliant with the Redfish Spec (DSP0266_1.20.0) 7.3.4 The $filter
// query parameter.
absl::StatusOr<std::string> BuildFilterFromRedpathPredicate(
    absl::string_view predicate);
// Builds a $filter query parameter string from a list of Redpath predicates.
// The resultant filter strings for each predicate will be joined by the "or"
// operator. The output string is compliant with the Redfish Spec
// (DSP0266_1.20.0) 7.3.4 The $filter query parameter.
absl::StatusOr<std::string> BuildFilterFromRedpathPredicateList(
    const std::vector<std::string> &predicates);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_PREDICATES_FILTER_H_
