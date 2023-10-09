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

#ifndef ECCLESIA_LIB_REDFISH_DELLICIUS_UTILS_PARSERS_H_
#define ECCLESIA_LIB_REDFISH_DELLICIUS_UTILS_PARSERS_H_

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "ecclesia/lib/file/cc_embed_interface.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/interface.h"

namespace ecclesia {

absl::StatusOr<absl::flat_hash_map<std::string, RedPathRedfishQueryParams>>
ParseQueryRulesFromEmbeddedFiles(
    const std::vector<EmbeddedFile> &embedded_query_rules);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_DELLICIUS_UTILS_PARSERS_H_
