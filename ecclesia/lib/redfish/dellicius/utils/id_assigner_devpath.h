/*
 * Copyright 2023 Google LLC
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

#ifndef ECCLESIA_LIB_REDFISH_DELLICIUS_UTILS_ID_ASSIGNER_DEVPATH_H_
#define ECCLESIA_LIB_REDFISH_DELLICIUS_UTILS_ID_ASSIGNER_DEVPATH_H_

#include <memory>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "ecclesia/lib/redfish/dellicius/utils/id_assigner.h"
namespace ecclesia {

// Creates an implementation of the IdAssigner which provides an
// identifier based on a provided static map using the |devpath| property in the
// SubqueryDataSet as the mapping input.
std::unique_ptr<IdAssigner<std::string>> NewMapBasedDevpathAssigner(
    absl::flat_hash_map<std::string, std::string> map);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_DELLICIUS_UTILS_ID_ASSIGNER_DEVPATH_H_
