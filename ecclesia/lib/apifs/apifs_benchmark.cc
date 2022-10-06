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

#include <string>

#include "benchmark/benchmark.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/apifs/apifs.h"

// In order to produce meaningful results, these benchmarks require you to
// specify sysfs file(s) for operating on. This does mean that the actual
// timings can vary wildly depending on what driver and hardware is backing the
// access, but it should still produce usable numbers for comparing runs.
ABSL_FLAG(std::string, sysfs_file_for_benchmark, "",
          "The sysfs file to read from for benchmarking");

namespace ecclesia {
namespace {

void BM_CreateApifsFileAndRead(benchmark::State &state) {
  std::string path = absl::GetFlag(FLAGS_sysfs_file_for_benchmark);
  CHECK(!path.empty()) << "--sysfs_file_for_benchmark has been set";

  for (auto s : state) {
    ApifsFile pci_state(path);
    benchmark::DoNotOptimize(pci_state.Read());
  }
}

void BM_ReuseApifsFileAndRead(benchmark::State &state) {
  std::string path = absl::GetFlag(FLAGS_sysfs_file_for_benchmark);
  CHECK(!path.empty()) << "--sysfs_file_for_benchmark has been set";
  ApifsFile pci_state(path);

  for (auto s : state) {
    benchmark::DoNotOptimize(pci_state.Read());
  }
}

BENCHMARK(BM_CreateApifsFileAndRead);
BENCHMARK(BM_ReuseApifsFileAndRead);

}  // namespace
}  // namespace ecclesia
