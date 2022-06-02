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

#ifndef ECCLESIA_LIB_IPMI_IPMI_REQUEST_H_
#define ECCLESIA_LIB_IPMI_IPMI_REQUEST_H_

#include <cstdint>

#include "absl/types/span.h"
#include "ecclesia/lib/ipmi/ipmi_consts.h"
#include "ecclesia/lib/ipmi/ipmi_pair.h"

namespace ecclesia {

// IpmiRequest is an implementation agnostic structure that is meant to grow
// over time to support different transport requirements.  The current members
// of the structure may be insufficient for all future cases, but these can be
// added as needed without changing the interface.
struct IpmiRequest {
  // IPMI network function.
  IpmiNetworkFunction network_function;
  // IPMI command.
  IpmiCommand command;
  // Read-only view of the data, must outlive call.
  absl::Span<const uint8_t> data;

  explicit IpmiRequest(const ecclesia::IpmiPair &netfn_cmd)
      : network_function(netfn_cmd.network_function()),
        command(netfn_cmd.command()) {}

  IpmiRequest(const ecclesia::IpmiPair &netfn_cmd,
              absl::Span<const uint8_t> data)
      : network_function(netfn_cmd.network_function()),
        command(netfn_cmd.command()),
        data(data) {}

  bool operator==(const IpmiRequest &other) const {
    return network_function == other.network_function &&
           command == other.command && data == other.data;
  }

  bool operator!=(const IpmiRequest &other) const { return !(*this == other); }
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_IPMI_IPMI_REQUEST_H_
