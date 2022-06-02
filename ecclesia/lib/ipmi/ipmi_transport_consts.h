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

#ifndef ECCLESIA_LIB_IPMI_IPMI_TRANSPORT_CONSTS_H_
#define ECCLESIA_LIB_IPMI_IPMI_TRANSPORT_CONSTS_H_

#include <cstdint>

namespace ecclesia {

enum IPv6RouterConf {
  IPV6_ROUTER_STATIC = (1 << 0),   // Static
  IPV6_ROUTER_DYNAMIC = (1 << 1),  // Dynamic
};

enum Ipv6Ipv4Support {
  IPV6_ONLY_SUPPORTED = (1 << 1),  // IPv6Only
  IPV6_IPV4_DUALSTACK = (1 << 2),  // DualStack
  IPV6_LAN_ALERTING = (1 << 2),    // IPv6Alerts
};

enum class Ipv6StaticAddressStatus : uint8_t {
  kActive = 0,
  kDisabled = 1,
  kPending = 2,
  kFailed = 3,
  kDeprecated = 4,
  kInvalid = 5,
};

enum class IPv6StaticAddressEnable : uint8_t {
  // As per IPMI specification for data 2 of IPv6 Static Address parameter data,
  // bit 7 is enable/disable.
  kStaticAddressDisabled = 0,
  kStaticAddressEnabled = 1 << 7,
};

// byte 0 - skipped in this count.
// byte 1 - set selector
// byte 2 - enable/disable high bit, address source/type low bits.
// bytes3-19 - ipv6 address network endian (16 bytes).
// byte 20 - prefix length
// byte 21 - address status (read-only).
//
// If there are 0 static address, data 21 is disabled, all else should be
// ignored.
//
// Bytes 1-21 are required, the length of the response is therefore 21, less one
// for the common parameter.
inline constexpr int kIpv6StaticAddressParameterLength = 20;

// Address source type byte. [7] - enabled, [6:4] - reserved, [3:0] - must be
// 0 for static (all other values are reserved).
inline constexpr uint8_t kIpv6StaticAddressReservedBitMask = 0x7f;
inline constexpr uint8_t kIpv6StaticAddressEnabledMask = 0x80;

// These all assume a message of 20 bytes in length with byte 1 (offset 0) being
// the Address Selector and byte 20 being the status.
inline constexpr uint8_t kIpv6StaticAddressAddressSelectorOffset = 0;
inline constexpr uint8_t kIpv6StaticAddressSourceTypeOffset = 1;
inline constexpr uint8_t kIpv6StaticAddressAddressOffset = 2;  // offsets 2-17.
inline constexpr uint8_t kIpv6StaticAddressPrefixLengthOffset = 18;
inline constexpr uint8_t kIpv6StaticAddressStatusOffset = 19;

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_IPMI_IPMI_TRANSPORT_CONSTS_H_
