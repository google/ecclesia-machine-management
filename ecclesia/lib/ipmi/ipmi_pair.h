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

#ifndef ECCLESIA_LIB_IPMI_IPMI_PAIR_H_
#define ECCLESIA_LIB_IPMI_IPMI_PAIR_H_

#include <type_traits>

#include "ecclesia/lib/ipmi/ipmi_consts.h"

namespace ecclesia {

class IpmiPair {
 public:
  static const IpmiPair kGetDeviceId;
  static const IpmiPair kBmcColdReset;
  static const IpmiPair kBmcWarmReset;
  static const IpmiPair kGetWatchdogTimer;
  static const IpmiPair kLanSetConfig;
  static const IpmiPair kLanGetConfig;
  static const IpmiPair kSetChannelAccess;

  IpmiPair(const IpmiPair &) = delete;
  IpmiPair &operator=(const IpmiPair &) = delete;

  IpmiPair(const IpmiPair &&) = delete;
  IpmiPair &operator=(const IpmiPair &&) = delete;

  IpmiNetworkFunction network_function() const { return network_function_; }
  IpmiCommand command() const { return command_; }

  IpmiPair(IpmiNetworkFunction netfn, IpmiCommand cmd)
      : network_function_(netfn), command_(cmd) {}

 private:
  IpmiNetworkFunction network_function_;
  IpmiCommand command_;
};

//  The static instances cannot be constexpr because the type is incomplete.
static_assert(
    std::is_trivially_destructible<IpmiPair>(),
    "To properly handle data lifetime for the static instances of this object "
    "as non-constexpr evaluable it must be trivial destructible.");

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_IPMI_IPMI_PAIR_H_
