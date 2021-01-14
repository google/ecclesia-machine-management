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

#include "ecclesia/magent/lib/event_logger/intel_cpu_topology.h"

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/logging.h"

namespace ecclesia {

namespace {
const IntelCpuTopology::Options &GetDefaultOptions() {
  static const IntelCpuTopology::Options *const options =
      new IntelCpuTopology::Options();
  return *options;
}

}  // namespace

IntelCpuTopology::IntelCpuTopology()
    : apifs_(GetDefaultOptions().apifs_path),
      lpu_to_package_id_(GetCpuTopology()) {}

IntelCpuTopology::IntelCpuTopology(const Options &options)
    : apifs_(options.apifs_path), lpu_to_package_id_(GetCpuTopology()) {}

// Since CPU topology is static, we will read it when the object being
// constructed, cache it in vector lpu_to_package_id_.
std::vector<int> IntelCpuTopology::GetCpuTopology() {
  std::string online;
  std::vector<int> ret;

  if (!ReadApifsFile<std::string>("online", &online).ok()) {
    ErrorLog() << "Failed to find number of CPUs.";
    return ret;
  }
  std::vector<absl::string_view> range = absl::StrSplit(online, '-');

  if (range.size() != 2) {
    ErrorLog() << "Invalid CPU id range: " << online << '.';
    return ret;
  }

  int from, to;
  if (!absl::SimpleAtoi(range[0], &from) || from != 0) {
    ErrorLog() << "Invalid CPU id range, from: ", range[0];
    return ret;
  }

  if (!absl::SimpleAtoi(range[1], &to) || to <= 0) {
    ErrorLog() << "Invalid CPU id range, to: ", range[1];
    return ret;
  }

  // "from" is always 0
  ret.resize(to - from + 1, -1);
  for (int i = from; i <= to; i++) {
    ReadApifsFile<int>(absl::StrCat("cpu", i, "/topology/physical_package_id"),
                       &ret[i])
        .IgnoreError();
  }

  return ret;
}

absl::StatusOr<int> IntelCpuTopology::GetSocketIdForLpu(int lpu) const {
  if (lpu >= (lpu_to_package_id_).size()) {
    return absl::NotFoundError(absl::StrFormat("lpu %d is out of range", lpu));
  }
  return lpu_to_package_id_[lpu];
}

std::vector<int> IntelCpuTopology::GetLpusForSocketId(int socket_id) const {
  std::vector<int> lpus;
  for (int i = 0; i < lpu_to_package_id_.size(); i++) {
    if (lpu_to_package_id_[i] == socket_id) {
      lpus.push_back(i);
    }
  }
  return lpus;
}

}  // namespace ecclesia
