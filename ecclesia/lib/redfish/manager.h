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

#ifndef ECCLESIA_LIB_REDFISH_MANAGER_H_
#define ECCLESIA_LIB_REDFISH_MANAGER_H_

#include <memory>

#include "google/protobuf/duration.pb.h"
#include "absl/status/statusor.h"
#include "ecclesia/lib/redfish/interface.h"

namespace ecclesia {

// Attempts to find the Manager resource object that acts as the Root and
// returns it, or returns an error status if unable to find it.
absl::StatusOr<std::unique_ptr<RedfishObject>> GetManagerForRoot(
    RedfishVariant root);

// Returns an "uptime" for a Manager resource by calculating the duration
// between the current system time and the last reset time.
absl::StatusOr<google::protobuf::Duration> GetUptimeForManager(
    const RedfishObject &mgr_obj);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_MANAGER_H_
