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

#include "ecclesia/magent/sysmodel/x86/cpu.h"

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "ecclesia/lib/io/constants.h"
#include "ecclesia/lib/io/msr.h"
#include "ecclesia/lib/smbios/processor_information.h"
#include "ecclesia/lib/smbios/reader.h"
#include "ecclesia/lib/smbios/structures.emb.h"
#include "ecclesia/magent/lib/event_logger/intel_cpu_topology.h"
#include "runtime/cpp/emboss_prelude.h"
#include "re2/re2.h"

namespace ecclesia {
namespace {

const LazyRE2 kCpuRegex = {"^CPU\\d+"};

constexpr int kIntelPPINCapabilityBit = 23;

absl::StatusOr<uint64_t> GetCpuSerialNumberFromMsr(int socket_id) {
  IntelCpuTopology top;
  std::vector<int> lpus = top.GetLpusForSocketId(socket_id);
  if (lpus.empty()) {
    return absl::InternalError(absl::StrFormat(
        "Unable to find any LPUs associated with socket %d", socket_id));
  }

  Msr msr(absl::StrCat("/dev/cpu/", lpus[0], "/msr"));

  // kMsrIa32PlatformInfo Msr bit 23 is set for Intel cpus that have PPIN.
  absl::StatusOr<uint64_t> maybe_platform_info = msr.Read(kMsrIa32PlatformInfo);
  if (!maybe_platform_info.ok()) {
    return maybe_platform_info.status();
  }

  if (!(*maybe_platform_info & (1 << kIntelPPINCapabilityBit))) {
    // This cpu does not have PPIN.
    return absl::UnimplementedError("CPU does not support PPIN");
  }

  //  Write 2 to PPIN_CTL (MSR 0x4e) to enable PPIN read.
  if (absl::Status status = msr.Write(kMsrIa32PpinCtl, 0x2); !status.ok()) {
    return status;
  }

  // Read the PPIN (MSR 0x4f)
  return msr.Read(kMsrIa32Ppin);
}

int GetCpuSocketId(const ProcessorInformation &processor) {
  auto view = processor.GetMessageView();
  std::string socket_id_snum =
      std::string(processor.GetString(view.socket_designation_snum().Read()));
  if (RE2::FullMatch(socket_id_snum, *kCpuRegex)) {
    int socket_id;
    // skip 3 chars("CPU").
    if (absl::SimpleAtoi(socket_id_snum.data() + 3, &socket_id)) {
      return socket_id;
    }
  }

  return -1;
}

std::string GetCpuSerialNumber(const ProcessorInformation &processor) {
  auto view = processor.GetMessageView();
  if (processor.IsIntelProcessor()) {
    int socket_id;
    if ((socket_id = GetCpuSocketId(processor)) != -1) {
      absl::StatusOr<uint64_t> maybe_serial =
          GetCpuSerialNumberFromMsr(socket_id);
      if (maybe_serial.ok()) {
        return absl::StrFormat("0x%016x", *maybe_serial);
      }
    }
  }

  return std::string(processor.GetString(view.serial_number_snum().Read()));
}

std::string GetCpuPartNumber(const ProcessorInformation &processor) {
  if (processor.IsIntelProcessor()) {
    return "cascadelake";
  }

  auto view = processor.GetMessageView();
  return std::string(processor.GetString(view.part_number_snum().Read()));
}

}  // namespace

Cpu::Cpu(const ProcessorInformation &processor) {
  auto view = processor.GetMessageView();
  cpu_info_.name = processor.GetString(view.socket_designation_snum().Read());
  cpu_info_.enabled = processor.IsProcessorEnabled();
  cpu_info_.cpu_signature = processor.GetSignature();
  cpu_info_.max_speed_mhz = view.max_speed_mhz().Read();
  cpu_info_.serial_number = GetCpuSerialNumber(processor);
  cpu_info_.part_number = GetCpuPartNumber(processor);
  cpu_info_.total_cores = processor.GetCoreCount();
  cpu_info_.enabled_cores = processor.GetCoreEnabled();
  cpu_info_.total_threads = processor.GetThreadCount();
  cpu_info_.socket_id = GetCpuSocketId(processor);
}

std::vector<Cpu> CreateCpus(const SmbiosReader &reader) {
  std::vector<Cpu> cpus;
  for (auto &processor : reader.GetAllProcessors()) {
    cpus.emplace_back(processor);
  }
  return cpus;
}

}  // namespace ecclesia
