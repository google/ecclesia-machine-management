/*
 * Copyright 2026 Google LLC
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

#include "benchmark/benchmark.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/dellicius/utils/path_util.h"

namespace ecclesia {
namespace {

void BM_SplitShort(benchmark::State& state) {
  for (auto s : state) {
    benchmark::DoNotOptimize(SplitNodeNameForNestedNodes("Thresholds"));
  }
}
BENCHMARK(BM_SplitShort);

void BM_SplitMedium(benchmark::State& state) {
  for (auto s : state) {
    benchmark::DoNotOptimize(
        SplitNodeNameForNestedNodes("Thresholds.UpperCritical.Reading"));
  }
}
BENCHMARK(BM_SplitMedium);

void BM_SplitLongEscaped(benchmark::State& state) {
  for (auto s : state) {
    benchmark::DoNotOptimize(SplitNodeNameForNestedNodes(
        "Actions.#Chassis\\.Reset.@Redfish\\.ActionInfo.@odata\\.id"));
  }
}
BENCHMARK(BM_SplitLongEscaped);

void BM_SplitWithSpaces(benchmark::State& state) {
  for (auto s : state) {
    benchmark::DoNotOptimize(SplitNodeNameForNestedNodes(
        "  Thresholds.UpperCritical.@odata\\.id   "));
  }
}
BENCHMARK(BM_SplitWithSpaces);

}  // namespace
}  // namespace ecclesia
