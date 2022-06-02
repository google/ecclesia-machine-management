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

// Convenience wrapper for the third_party ipmitool library calls.
#ifndef ECCLESIA_LIB_IPMI_IPMI_HANDLE_H_
#define ECCLESIA_LIB_IPMI_IPMI_HANDLE_H_

#include <cstdint>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ecclesia/lib/arbiter/arbiter.h"
#include "ecclesia/lib/ipmi/ipmi_raw_interface.h"
#include "ecclesia/lib/ipmi/ipmi_request.h"
#include "ecclesia/lib/ipmi/ipmi_response.h"
#include "ecclesia/lib/ipmi/raw/ipmitool.h"

extern "C" {
// Forward declaration so that C header inclusion is in .cc only.
struct ipmi_intf;
}

namespace ecclesia {

// IpmiHandle represents an IPMI interface. It should be acquired from the
// singleton IpmiManager's Acquire method. Use the static getter to call the
// IpmiManager's Acquire method like so:
// `ecclesia::GlobalIpmiManager().Acquire(...);`
//
// Note that Acquire must wait for exclusive access to the IPMI. Holding a
// reference to an IpmiHandle means you are holding up ALL other IPMI traffic,
// so only hold a reference to this for successive operations that must be
// transactional (in terms of IPMI communication) and wrap up in quick
// succession. DO NOT grab a reference to this and stash it in a class to be
// used later, you will cause IPMI related deadlock.
//
// IpmiHandle is an interface that wraps most common ipmitool calls. It is
// intended to be a thin wrapper layer with 1:1 mapping, i.e. Send()
// corresponds to ipmi_send() call in the C library, and handles the basic
// jobs such as input/output transformations and error checking.

class IpmiHandle : RawIpmiInterface {
 public:
  struct SdrSensorIdentifier {
    uint8_t sensor;
    uint8_t target;
    uint8_t lun;
    uint8_t channel;
  };

  struct DeviceId {
    uint32_t manufacturer_id;
    uint8_t firmware_major_rev;
    uint8_t firmware_minor_rev;
    uint8_t firmware_aux_rev[4];
  };

  IpmiHandle() = default;
  IpmiHandle(const IpmiHandle &other) = delete;
  IpmiHandle &operator=(const IpmiHandle &other) = delete;

  // Returns the raw interface. Do not store this pointer, only use it while
  // holding an IpmiHandle for the specific interface needed.
  virtual ipmi_intf *raw_intf() = 0;

  // Wrapper for ipmi_sdr_get_sensor_thresholds.
  virtual absl::StatusOr<IpmiResponse> SdrGetSensorThresholds(
      SdrSensorIdentifier sensor_id) const = 0;

  // Wrapper for ipmi_sdr_get_sensor_reading_ipmb.
  virtual absl::StatusOr<IpmiResponse> SdrGetSensorReadingIpmb(
      SdrSensorIdentifier sensor_id) const = 0;

  // Wrapper for ipmi_chassis_power_control.
  virtual absl::Status ChassisPowerControl(uint8_t control_code) const = 0;

  // Send IPMI data specified by a buffer.
  absl::StatusOr<IpmiResponse> Send(const IpmiRequest &request) override {
    RawIpmitool tool(raw_intf());
    return tool.Send(request);
  }
};

class IpmiHandleImpl : public IpmiHandle {
 public:
  explicit IpmiHandleImpl(ExclusiveLock<ipmi_intf *> active_interface_lock);

  IpmiHandleImpl(const IpmiHandleImpl &other) = delete;

  IpmiHandleImpl &operator=(const IpmiHandleImpl &other) = delete;

  ipmi_intf *raw_intf() override;

  absl::StatusOr<IpmiResponse> SdrGetSensorThresholds(
      SdrSensorIdentifier sensor_id) const override;

  absl::StatusOr<IpmiResponse> SdrGetSensorReadingIpmb(
      SdrSensorIdentifier sensor_id) const override;

  absl::Status ChassisPowerControl(uint8_t control_code) const override;

 private:
  const ExclusiveLock<ipmi_intf *> active_interface_lock_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_IPMI_IPMI_HANDLE_H_
