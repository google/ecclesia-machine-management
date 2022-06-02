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

#include "ecclesia/lib/ipmi/ipmi_pair.h"

#include "ecclesia/lib/ipmi/ipmi_consts.h"

namespace ecclesia {

const IpmiPair IpmiPair::kGetDeviceId(IpmiNetworkFunction::kApp,
                                      IpmiCommand::kGetDeviceId);
const IpmiPair IpmiPair::kBmcColdReset(IpmiNetworkFunction::kApp,
                                       IpmiCommand::kBmcColdReset);
const IpmiPair IpmiPair::kBmcWarmReset(IpmiNetworkFunction::kApp,
                                       IpmiCommand::kBmcWarmReset);
const IpmiPair IpmiPair::kGetWatchdogTimer(IpmiNetworkFunction::kApp,
                                           IpmiCommand::kGetWatchdogTimer);
const IpmiPair IpmiPair::kLanSetConfig(IpmiNetworkFunction::kTransport,
                                       IpmiCommand::kLanSetConfig);
const IpmiPair IpmiPair::kLanGetConfig(IpmiNetworkFunction::kTransport,
                                       IpmiCommand::kLanGetConfig);
const IpmiPair IpmiPair::kSetChannelAccess(IpmiNetworkFunction::kApp,
                                           IpmiCommand::kSetChannelAccess);

}  // namespace ecclesia
