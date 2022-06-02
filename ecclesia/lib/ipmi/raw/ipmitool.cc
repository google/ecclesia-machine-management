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

#include "ecclesia/lib/ipmi/raw/ipmitool.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "ecclesia/lib/ipmi/ipmi_response.h"
#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/logging.h"

extern "C" {
#include "include/ipmitool/ipmi_intf.h"
}

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "ecclesia/lib/ipmi/ipmi_consts.h"
#include "ecclesia/lib/ipmi/ipmi_request.h"

namespace ecclesia {

absl::Status RawIpmitool::Raw(const std::vector<uint8_t> &buffer,
                              ipmi_rs **resp) {
  const uint8_t *bytes = buffer.data();
  uint32_t length = buffer.size();

  // The length needs to be at least two bytes [netfn][cmd]
  if (length < ecclesia::kMinimumIpmiPacketLength)
    return absl::InvalidArgumentError("Insufficient bytes to raw call");

  uint32_t data_length = length - ecclesia::kMinimumIpmiPacketLength;
  uint8_t data[ecclesia::kMaximumPipelineBandwidth] = {0};
  ipmi_rq request = {};

  // Skip beyond netfn and command.
  if (data_length > 0) {
    std::memcpy(data, &bytes[ecclesia::kMinimumIpmiPacketLength], data_length);
  }

  ipmi_intf_session_set_timeout(intf_, 15);
  ipmi_intf_session_set_retry(intf_, 1);

  request.msg.netfn = bytes[0];
  request.msg.lun = 0x00;
  request.msg.cmd = bytes[1];
  request.msg.data = data;
  request.msg.data_len = data_length;

  ipmi_rs *response = intf_->sendrecv(intf_, &request);
  if (nullptr == response)
    return absl::InternalError("response was NULL from intf->sendrecv");

  // If you gave us a pointer to a pointer, we'll update where it points.
  if (resp) *resp = response;

  if (response->ccode > 0) {
    if (ecclesia::IPMI_TIMEOUT_COMPLETION_CODE == response->ccode) {
      return absl::InternalError("Timeout from IPMI");
    }
    return absl::InternalError(
        absl::StrCat("Unable to send code: ", response->ccode));
  }

  return absl::OkStatus();
}

absl::StatusOr<ecclesia::IpmiResponse> RawIpmitool::SendWithRetry(
    const ecclesia::IpmiRequest &request, int retries) {
  ipmi_rs *resp;

  int tries = retries + 1;
  Check(tries > 0, "tries > 0")
      << "Negative number passed to function through retries count";

  std::vector<uint8_t> buffer(ecclesia::kMinimumIpmiPacketLength +
                              request.data.size());
  buffer[0] = static_cast<uint8_t>(request.network_function);
  buffer[1] = static_cast<uint8_t>(request.command);
  if (!request.data.empty()) {
    std::memcpy(&buffer[ecclesia::kMinimumIpmiPacketLength],
                request.data.data(), request.data.size());
  }

  int count = 0;
  absl::Status result;
  while (count < tries) {
    result = Raw(buffer, &resp);
    if (result.ok()) break;
    count++;
  }

  if (!result.ok()) {
    return absl::InternalError(
        absl::StrCat("Failed to send IPMI command after ", count, " tries."));
  }

  // Set each parameter independently in case there are more later.
  ecclesia::IpmiResponse response;
  response.code = resp->ccode;
  response.data = std::vector<uint8_t>(resp->data, resp->data + resp->data_len);
  return response;
}

absl::StatusOr<ecclesia::IpmiResponse> RawIpmitool::Send(
    const ecclesia::IpmiRequest &request) {
  return SendWithRetry(request, 0);
}

}  // namespace ecclesia
