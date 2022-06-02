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

#ifndef ECCLESIA_LIB_IPMI_IPMI_RESPONSE_H_
#define ECCLESIA_LIB_IPMI_IPMI_RESPONSE_H_

#include <cstdint>
#include <vector>

#include "ecclesia/lib/ipmi/ipmi_consts.h"

namespace ecclesia {

// IpmiResponse is an implementation agnostic structure representing the
// response from an IPMI command (LAN/host/other).
//
// IPMI commands return a code and optionally data.  When there is data, the
// code is typically 0 for OK.  However, this is implementation dependent.
struct IpmiResponse {
  int code;
  std::vector<uint8_t> data;

  bool operator==(const IpmiResponse &other) const {
    return code == other.code && data == other.data;
  }

  bool operator!=(const IpmiResponse &other) const { return !(*this == other); }
};

inline IpmiResponse OkIpmiResponse() { return {.code = IPMI_OK_CODE}; }

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_IPMI_IPMI_RESPONSE_H_
