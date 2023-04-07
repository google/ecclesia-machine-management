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

#ifndef ECCLESIA_LIB_REDFISH_DELLICIUS_TOOLS_REDFISH_BACKEND_H_
#define ECCLESIA_LIB_REDFISH_DELLICIUS_TOOLS_REDFISH_BACKEND_H_

#include <memory>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/transport/interface.h"

namespace ecclesia {

absl::StatusOr<std::unique_ptr<ecclesia::RedfishTransport>>
CreateRedfishTransport(absl::string_view target, absl::string_view type);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_DELLICIUS_TOOLS_REDFISH_BACKEND_H_
