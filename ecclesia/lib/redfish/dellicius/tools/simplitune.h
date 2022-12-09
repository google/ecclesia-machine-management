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

#ifndef ECCLESIA_LIB_REDFISH_DELLICIUS_TOOLS_SIMPLITUNE_H_
#define ECCLESIA_LIB_REDFISH_DELLICIUS_TOOLS_SIMPLITUNE_H_

#include "ecclesia/lib/redfish/dellicius/engine/internal/interface.h"
#include "ecclesia/lib/redfish/dellicius/engine/query_rules.pb.h"

namespace ecclesia {

// Generates a collection of RedPath prefix set where each prefix has a unique
// expand configuration.
std::vector<RedPathRedfishQueryParams> GenerateExpandConfigurations(
    const QueryTracker &query_tracker);

// Converts a RedPath prefix set to RedPathPrefixSetWithQueryParams proto.
QueryRules::RedPathPrefixSetWithQueryParams GetQueryRuleProtoFromConfig(
    const RedPathRedfishQueryParams &redpath_to_query_params);
}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_DELLICIUS_TOOLS_SIMPLITUNE_H_
