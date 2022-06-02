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

#ifndef ECCLESIA_LIB_IPMI_IPMI_STATS_H_
#define ECCLESIA_LIB_IPMI_IPMI_STATS_H_

#include <cstdint>

#include "absl/container/flat_hash_map.h"
#include "absl/time/time.h"
#include "ecclesia/lib/circularbuffer/circularbuffer.h"
#include "ecclesia/lib/ipmi/ipmi_command_identifier.h"

namespace ecclesia {

// IPMI request statistics for monitoring.
struct IpmiCommandStats {
  // Tracking command stat at transport layer: any command getting response
  // code IPMI_CC_OK counts as success, irrespective of possible failures in
  // higher layer parsing response payloads.
  struct TransportStat {
    uint64_t count = 0;
    uint64_t success_count = 0;
    absl::Time start_time;
    // Measured from issuing command to getting response at the gsys layer.
    CircularBuffer<absl::Duration> recent_latencies{100};
  };
  absl::flat_hash_map<IpmiCommandIdentifier, TransportStat> command_stats;
  // Global should equal sum of all command counts. Saves summations.
  uint64_t global_count = 0;
  uint64_t global_success_count = 0;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_IPMI_IPMI_STATS_H_
