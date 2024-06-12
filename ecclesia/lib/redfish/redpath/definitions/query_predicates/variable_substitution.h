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

#ifndef ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_PREDICATES_VARIABLE_SUBSTITUTION_H_
#define ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_PREDICATES_VARIABLE_SUBSTITUTION_H_

#include <string_view>

#include "absl/status/statusor.h"
#include "ecclesia/lib/redfish/dellicius/query/query_variables.pb.h"
namespace ecclesia {

absl::StatusOr<std::string> SubstituteVariables(
    std::string_view predicate, const QueryVariables& variables);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_PREDICATES_VARIABLE_SUBSTITUTION_H_
