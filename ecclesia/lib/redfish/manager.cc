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

#include "ecclesia/lib/redfish/manager.h"

#include <memory>
#include <string>
#include <utility>

#include "google/protobuf/duration.pb.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/property_definitions.h"
#include "ecclesia/lib/redfish/status.h"
#include "ecclesia/lib/time/proto.h"

namespace ecclesia {

absl::StatusOr<std::unique_ptr<RedfishObject>> GetManagerForRoot(
    RedfishVariant root) {
  // corresponds to this Service Root, we iterate through the Managers to find
  // a `ServiceEntryPointUUID` property that matches the Service Root `UUID`.
  //
  // DMTF has proposed a new property to create a simpler link from the
  // Service Root to the servicing Manager resource:
  // https://github.com/DMTF/Redfish/issues/4546. Once DMTF has approved &
  // rolled out this change, the below logic should be updated accordingly.
  std::unique_ptr<RedfishObject> root_obj = root.AsObject();
  if (root_obj == nullptr) {
    return absl::InternalError(absl::StrCat(
        "Unable to get Service Root as object. ", root.status().message()));
  }

  std::optional<std::string> root_uuid = root_obj->GetNodeValue<PropertyUuid>();
  if (!root_uuid.has_value()) {
    return absl::InternalError(UnableToGetPropertyMessage<PropertyUuid>());
  }

  std::unique_ptr<RedfishObject> manager_obj;
  root[kRfPropertyManagers].Each().Do([&](std::unique_ptr<RedfishObject> &obj) {
    std::optional<std::string> entry_point_uuid =
        obj->GetNodeValue<PropertyServiceEntryPointUuid>();
    if (entry_point_uuid.has_value() && *entry_point_uuid == *root_uuid) {
      manager_obj = std::move(obj);
      return RedfishIterReturnValue::kStop;
    }
    return RedfishIterReturnValue::kContinue;
  });

  if (manager_obj == nullptr) {
    return absl::InternalError("Unable to locate Manager for Service Root.");
  }
  return manager_obj;
}

absl::StatusOr<google::protobuf::Duration> GetUptimeForManager(
    const RedfishObject &mgr_obj) {
  std::optional<absl::Time> dt = mgr_obj.GetNodeValue<PropertyDateTime>();
  if (!dt.has_value()) {
    return absl::InternalError(UnableToGetPropertyMessage<PropertyDateTime>());
  }
  std::optional<absl::Time> last_reset_dt =
      mgr_obj.GetNodeValue<PropertyLastResetTime>();
  if (!last_reset_dt.has_value()) {
    return absl::InternalError(
        UnableToGetPropertyMessage<PropertyLastResetTime>());
  }

  absl::Duration uptime = (*dt) - (*last_reset_dt);
  absl::StatusOr<google::protobuf::Duration> uptime_pb =
      AbslDurationToProtoDuration(uptime);
  if (!uptime_pb.ok()) {
    return absl::InternalError(absl::StrCat(
        "Unable to create uptime protobuf. ", uptime_pb.status().message()));
  }
  return uptime_pb;
}

}  // namespace ecclesia
