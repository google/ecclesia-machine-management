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

// This library provides capabilities to get indus cpu topology,
// it uses an ApifsDirectory object to access a sysfs.

#ifndef ECCLESIA_MAGENT_LIB_EVENT_LOGGER_INDUS_CPU_TOPOLOGY_H_
#define ECCLESIA_MAGENT_LIB_EVENT_LOGGER_INDUS_CPU_TOPOLOGY_H_

#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/apifs/apifs.h"
#include "ecclesia/lib/mcedecoder/cpu_topology.h"

namespace ecclesia {

class IntelCpuTopology : public CpuTopologyInterface {
 public:
  // Used to initialize ApifsDirectory object.
  // Defines the filesystem path where CPU attributes will be exported.
  // Defaults to the standard Linux sysfs path for this information.
  struct Options {
    std::string apifs_path = "/sys/devices/system/cpu/";
  };

  IntelCpuTopology();

  explicit IntelCpuTopology(const Options &options);

  // Return physical package id based on lpu.
  absl::StatusOr<int> GetSocketIdForLpu(int lpu) const override;

  std::vector<int> GetLpusForSocketId(int socket_id) const;

  // Typed read using a ApifsFile object.
  // Parse the string to proper type.
  template <typename T>
  absl::Status ReadApifsFile(const std::string &path, T *value) {
    ApifsFile apifs_file(apifs_, path);
    absl::StatusOr<std::string> maybe_str = apifs_file.Read();
    if (!maybe_str.ok()) {
      return maybe_str.status();
    }
    if (!FromString(*maybe_str, value)) {
      return absl::Status(absl::StatusCode::kInvalidArgument,
                          absl::StrCat("Unable to parse ", *maybe_str));
    }
    return absl::OkStatus();
  }

 private:
  // Return Cpu topology mapping, return empty vector if error.
  std::vector<int> GetCpuTopology();

  static bool FromString(std::string str, std::string *value) {
    *value = std::move(str);
    return true;
  }

  static bool FromString(absl::string_view str, float *value) {
    return absl::SimpleAtof(str, value);
  }

  static bool FromString(absl::string_view str, double *value) {
    return absl::SimpleAtod(str, value);
  }

  static bool FromString(absl::string_view str, bool *value) {
    return absl::SimpleAtob(str, value);
  }

  template <typename int_type>
  static bool FromString(absl::string_view str, int_type *value) {
    return absl::SimpleAtoi<int_type>(str, value);
  }

  ApifsDirectory apifs_;

  // index -> physical package id.
  // it will get initialized with -1, if there are error when parsing cpu's
  // physical package id, the -1 remains, so there could be gaps with -1 in it.
  const std::vector<int> lpu_to_package_id_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_LIB_EVENT_LOGGER_INDUS_CPU_TOPOLOGY_H_
