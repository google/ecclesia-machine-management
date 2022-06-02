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

#ifndef ECCLESIA_LIB_IPMI_KCS_CONSTANTS_H_
#define ECCLESIA_LIB_IPMI_KCS_CONSTANTS_H_

#include <cstdint>
#include <vector>

#include "absl/types/span.h"
#include "ecclesia/lib/ipmi/ipmi_consts.h"

namespace ecclesia::kcs {

inline void SetLun(std::vector<uint8_t> &message, uint8_t lun) {
  message[kNetFnLunOffset] &= ~kLogicalUnitNumberMask;
  message[kNetFnLunOffset] |= (lun & kLogicalUnitNumberMask);
}

inline uint8_t GetLun(absl::Span<const uint8_t> message) {
  return message[kNetFnLunOffset] & kLogicalUnitNumberMask;
}

inline void SetNetFn(std::vector<uint8_t> &message, uint8_t netfn) {
  message[kNetFnLunOffset] &= kLogicalUnitNumberMask;
  message[kNetFnLunOffset] |= (netfn << kNetworkFunctionShift);
}

inline uint8_t GetNetFn(absl::Span<const uint8_t> message) {
  return message[kNetFnLunOffset] >> kNetworkFunctionShift;
}

inline void SetCommand(std::vector<uint8_t> &message, uint8_t command) {
  message[kCommandOffset] = command;
}

inline uint8_t GetCommand(absl::Span<const uint8_t> message) {
  return message[kCommandOffset];
}

inline void SetCompletionCode(std::vector<uint8_t> &message, uint8_t code) {
  message[kCompletionCodeOffset] = code;
}

inline uint8_t GetCompletionCode(absl::Span<const uint8_t> message) {
  return message[kCompletionCodeOffset];
}

}  // namespace ecclesia::kcs

#endif  // ECCLESIA_LIB_KCS_CONSTANTS_H_
