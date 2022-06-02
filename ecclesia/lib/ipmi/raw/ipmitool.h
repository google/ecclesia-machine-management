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

#ifndef ECCLESIA_LIB_IPMI_RAW_IPMITOOL_H_
#define ECCLESIA_LIB_IPMI_RAW_IPMITOOL_H_

#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ecclesia/lib/ipmi/ipmi_raw_interface.h"
#include "ecclesia/lib/ipmi/ipmi_request.h"
#include "ecclesia/lib/ipmi/ipmi_response.h"

struct ipmi_intf;

extern "C" {
// This struct is defined in src/plugins/open/open.c
extern struct ipmi_intf ipmi_open_intf;

struct ipmi_rs;
}  // extern "C"

namespace ecclesia {

// This is a Raw IPMI interface.
class RawIpmitool : public RawIpmiInterface {
 public:
  RawIpmitool() : RawIpmitool(&ipmi_open_intf) {}

  explicit RawIpmitool(ipmi_intf *intf) : intf_(intf) {}

  // The Send commands will return a util::error if:
  // 1) the ipmitool call return a nullptr.
  // 2) the response code returned via ipmi is non-zero.
  absl::StatusOr<IpmiResponse> Send(const IpmiRequest &request) override;

  absl::StatusOr<ecclesia::IpmiResponse> SendWithRetry(
      const IpmiRequest &request, int retries);

  absl::Status Raw(const std::vector<uint8_t> &buffer, ipmi_rs **resp);

 private:
  ipmi_intf *intf_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_IPMI_RAW_IPMITOOL_H_
