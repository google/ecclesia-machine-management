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

#ifndef ECCLESIA_LIB_REDFISH_HEALTH_ROLLUP_H_
#define ECCLESIA_LIB_REDFISH_HEALTH_ROLLUP_H_

#include <optional>
#include <string>

#include "absl/functional/any_invocable.h"
#include "absl/status/statusor.h"
#include "ecclesia/lib/redfish/health_rollup.pb.h"
#include "ecclesia/lib/redfish/interface.h"

namespace ecclesia {

// Extracts health rollup information (e.g. MessageArgs, Severity, and
// Timestamp) for each Condition in the Status payload of the provided
// RedfishObject.
absl::StatusOr<HealthRollup> ExtractHealthRollup(const RedfishObject &obj);

// An overloaded version of `ExtractHealthRollup` for which the caller may
// provide a devpath resolver callback, to be called either on the
// `RedfishObject` pointed to by the OriginOfCondition property (if present), or
// the `RedfishObject` containing the Conditions array (if absent).
// By default, parse only if the overall resource health is Warning or Critical,
// but users may use `parse_with_ok_health` to parse even if the health is OK.
absl::StatusOr<HealthRollup> ExtractHealthRollup(
    const RedfishObject &obj,
    absl::AnyInvocable<std::optional<std::string>(const RedfishObject &)>
        devpath_resolver,
    bool parse_with_ok_health);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_HEALTH_ROLLUP_H_
