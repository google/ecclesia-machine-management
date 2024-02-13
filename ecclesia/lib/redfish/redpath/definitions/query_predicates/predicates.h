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

#ifndef ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_PREDICATES_PREDICATES_H_
#define ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_PREDICATES_PREDICATES_H_

#include <cstddef>
#include <string>

#include "absl/status/statusor.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {

struct PredicateOptions {
  std::string predicate;
  size_t node_index;
  size_t node_set_size;
};

// Qualifies given `RedfishObject` using the filter criteria in
// `PredicateOptions`.
// Returns true if predicate can be applied successfully else returns false.
absl::StatusOr<bool> ApplyPredicateRule(const nlohmann::json &json_object,
                                        const PredicateOptions &options);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_PREDICATES_PREDICATES_H_
