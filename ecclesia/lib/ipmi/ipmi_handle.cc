/*
 * Copyright 2021 Google LLC
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

#include "ecclesia/lib/ipmi/ipmi_handle.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "ecclesia/lib/arbiter/arbiter.h"
#include "ecclesia/lib/ipmi/ipmi_response.h"

extern "C" {
#include "include/ipmitool/ipmi_chassis.h"  // IWYU pragma: keep
#include "include/ipmitool/ipmi_intf.h"
#include "include/ipmitool/ipmi_mc.h"  // IWYU pragma: keep
#include "include/ipmitool/ipmi_sdr.h"  // IWYU pragma: keep
#include "include/ipmitool/ipmi_sensor.h"  // IWYU pragma: keep
}  // extern "C"

namespace ecclesia {

namespace {

std::string SensorToString(IpmiHandle::SdrSensorIdentifier sensor_id) {
  return absl::Substitute("(Sensor $0:$1)", sensor_id.lun, sensor_id.sensor);
}

// Perform common error checks for the returned ipmi_rs struct.
absl::StatusOr<IpmiResponse> ToIpmiResponseChecked(
    struct ipmi_rs *rsp, absl::string_view caller_str) {
  if (rsp == nullptr) {
    return absl::InternalError(
        absl::Substitute("$0: response is null.", caller_str));
  }

  // rsp->data is an array with size 'IPMI_BUF_SIZE'. Make sure we don't
  // go out of bounds.
  if (rsp->data_len > IPMI_BUF_SIZE) {
    return absl::OutOfRangeError(absl::Substitute(
        "$0: response len $1 is out of range.", caller_str, rsp->data_len));
  }

  return IpmiResponse{
      .code = rsp->ccode,
      .data = std::vector<uint8_t>(rsp->data, rsp->data + rsp->data_len)};
}
}  // namespace

IpmiHandleImpl::IpmiHandleImpl(
    ecclesia::ExclusiveLock<ipmi_intf *> active_interface_lock)
    : active_interface_lock_(std::move(active_interface_lock)) {}

ipmi_intf *IpmiHandleImpl::raw_intf() { return *active_interface_lock_; }

absl::StatusOr<IpmiResponse> IpmiHandleImpl::SdrGetSensorThresholds(
    SdrSensorIdentifier sensor_id) const {
  struct ipmi_rs *rsp = ipmi_sdr_get_sensor_thresholds(
      *active_interface_lock_, sensor_id.sensor, sensor_id.target,
      sensor_id.lun, sensor_id.channel);

  auto error_str = absl::Substitute("Failed to get thresholds for $0",
                                    SensorToString(sensor_id));

  return ToIpmiResponseChecked(rsp, error_str);
}

absl::StatusOr<IpmiResponse> IpmiHandleImpl::SdrGetSensorReadingIpmb(
    SdrSensorIdentifier sensor_id) const {
  struct ipmi_rs *rsp = ipmi_sdr_get_sensor_reading_ipmb(
      *active_interface_lock_, sensor_id.sensor, sensor_id.target,
      sensor_id.lun, sensor_id.channel);

  auto error_str =
      absl::Substitute("Failed to read $0", SensorToString(sensor_id));

  return ToIpmiResponseChecked(rsp, error_str);
}

absl::Status IpmiHandleImpl::ChassisPowerControl(uint8_t control_code) const {
  if (ipmi_chassis_power_control(*active_interface_lock_, control_code) == 0) {
    return absl::OkStatus();
  }
  return absl::InternalError(absl::Substitute(
      "Failed to issue Chassis Power Control to $0", control_code));
}

}  // namespace ecclesia
