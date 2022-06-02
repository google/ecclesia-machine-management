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

#ifndef ECCLESIA_LIB_IPMI_IPMI_COMMAND_IDENTIFIER_H_
#define ECCLESIA_LIB_IPMI_IPMI_COMMAND_IDENTIFIER_H_

#include <cstdint>
#include <tuple>
#include <utility>

namespace ecclesia {

// Payload container for SendReceive
struct IpmiCommandIdentifier {
  uint8_t netfn;
  uint8_t lun;
  uint8_t command;
  bool operator==(const IpmiCommandIdentifier &other) const {
    return std::tie(netfn, lun, command) ==
           std::tie(other.netfn, other.lun, other.command);
  }
  template <typename H>
  friend H AbslHashValue(H h, const IpmiCommandIdentifier &id) {
    return H::combine(std::move(h), id.netfn, id.lun, id.command);
  }
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_IPMI_IPMI_COMMAND_IDENTIFIER_H_
