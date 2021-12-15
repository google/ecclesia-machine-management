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

#include "ecclesia/magent/lib/event_logger/indus/system_event_visitors.h"

#include <memory>
#include <utility>

#include "absl/memory/memory.h"
#include "absl/time/time.h"
#include "ecclesia/magent/lib/event_logger/system_event_visitors.h"
#include "ecclesia/magent/lib/mcedecoder/cpu_topology.h"
#include "ecclesia/magent/lib/mcedecoder/indus/dimm_translator.h"
#include "ecclesia/magent/lib/mcedecoder/mce_decode.h"

namespace ecclesia {

namespace {

std::unique_ptr<MceDecoder> CreateIndusMceDecoder(
    std::unique_ptr<CpuTopologyInterface> cpu_topology) {
  CpuVendor vendor = CpuVendor::kIntel;
  CpuIdentifier identifier = CpuIdentifier::kSkylake;
  return std::make_unique<MceDecoder>(vendor, identifier,
                                      std::move(cpu_topology),
                                      std::make_unique<IndusDimmTranslator>());
}

}  // namespace

std::unique_ptr<CpuErrorCountingVisitor> CreateIndusCpuErrorCountingVisitor(
    absl::Time lower_bound,
    std::unique_ptr<CpuTopologyInterface> cpu_topology) {
  auto mce_decoder = CreateIndusMceDecoder(std::move(cpu_topology));
  auto mce_adapter =
      std::make_unique<MceDecoderAdapter>(std::move(mce_decoder));
  return std::make_unique<CpuErrorCountingVisitor>(lower_bound,
                                                   std::move(mce_adapter));
}

std::unique_ptr<DimmErrorCountingVisitor> CreateIndusDimmErrorCountingVisitor(
    absl::Time lower_bound,
    std::unique_ptr<CpuTopologyInterface> cpu_topology) {
  auto mce_decoder = CreateIndusMceDecoder(std::move(cpu_topology));
  auto mce_adapter =
      std::make_unique<MceDecoderAdapter>(std::move(mce_decoder));
  return std::make_unique<DimmErrorCountingVisitor>(lower_bound,
                                                    std::move(mce_adapter));
}

}  // namespace ecclesia
