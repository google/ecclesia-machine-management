/*
 * Copyright 2020 Google LLC
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

#ifndef ECCLESIA_MAGENT_LIB_EVENT_LOGGER_INDUS_SYSTEM_EVENT_VISITORS_H_
#define ECCLESIA_MAGENT_LIB_EVENT_LOGGER_INDUS_SYSTEM_EVENT_VISITORS_H_

#include <memory>

#include "absl/time/time.h"
#include "ecclesia/lib/mcedecoder/cpu_topology.h"
#include "ecclesia/magent/lib/event_logger/system_event_visitors.h"

// This file provides concrete system event visitors for counting cpu and memory
// errors on an Indus platform

namespace ecclesia {

// Factory functions to create error counting visitors for Indus platform
// This is to hide the platform specific object that are needed for the
// construction of the mcedecoder required by these visitors
std::unique_ptr<CpuErrorCountingVisitor> CreateIndusCpuErrorCountingVisitor(
    absl::Time lower_bound, std::unique_ptr<CpuTopologyInterface> cpu_topology);
std::unique_ptr<DimmErrorCountingVisitor> CreateIndusDimmErrorCountingVisitor(
    absl::Time lower_bound, std::unique_ptr<CpuTopologyInterface> cpu_topology);

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_LIB_EVENT_LOGGER_INDUS_SYSTEM_EVENT_VISITORS_H_
