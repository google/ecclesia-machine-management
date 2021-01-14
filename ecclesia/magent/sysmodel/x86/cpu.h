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

#ifndef ECCLESIA_MAGENT_SYSMODEL_X86_CPU_H_
#define ECCLESIA_MAGENT_SYSMODEL_X86_CPU_H_

#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "ecclesia/lib/smbios/processor_information.h"
#include "ecclesia/lib/smbios/reader.h"

namespace ecclesia {

struct CpuInfo {
  std::string name;
  bool enabled;
  absl::optional<CpuSignature> cpu_signature;
  int max_speed_mhz;
  std::string serial_number;
  std::string part_number;
  int total_cores;
  int enabled_cores;
  int total_threads;
  int socket_id = -1;  // -1 indicates invalid value.
};

class Cpu {
 public:
  explicit Cpu(const ProcessorInformation &processor);

  // Allow the object to be copyable
  // Make sure that copy construction is relatively light weight.
  // In cases where it is not feasible to copy construct data members,it may
  // make sense to wrap the data member in a shared_ptr.
  Cpu(const Cpu &cpu) = default;
  Cpu &operator=(const Cpu &cpu) = default;

  const CpuInfo &GetCpuInfo() const { return cpu_info_; }

  // Possible additions wil likely be methods to temperature sensors associated
  // with the cpu

 private:
  CpuInfo cpu_info_;
};

std::vector<Cpu> CreateCpus(const SmbiosReader &reader);

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_SYSMODEL_X86_CPU_H_
