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

#ifndef ECCLESIA_MAGENT_LIB_EVENT_READER_EVENT_READER_H_
#define ECCLESIA_MAGENT_LIB_EVENT_READER_EVENT_READER_H_

#include <assert.h>

#include <cstdint>
#include <vector>

#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "ecclesia/magent/lib/event_reader/elog.emb.h"
#include "runtime/cpp/emboss_cpp_util.h"

namespace ecclesia {

// Raw machine check exception as read from mcedaemon
struct MachineCheck {
  absl::optional<uint64_t> mci_status;  /* MCi_STATUS */
  absl::optional<uint64_t> mci_address; /* MCi_ADDR */
  absl::optional<uint64_t> mci_misc;    /* MCi_MISC */
  absl::optional<uint64_t> mci_synd;    /* MCi_SYND (Syndrome; SMCA-only) */
  /* MCi_IPID (IP Identification; SMCA-only) */
  absl::optional<uint64_t> mci_ipid;
  absl::optional<uint64_t> mcg_status; /* MCG_STATUS */
  absl::optional<uint64_t> tsc;        /* CPU timestamp counter */
  absl::optional<absl::Time> time;     /* MCED timestamp */
  absl::optional<uint64_t> ip;         /* CPU instruction pointer */
  absl::optional<int32_t> boot;        /* boot number (-1 for unknown) */
  absl::optional<int32_t> cpu;         /* excepting CPU */
  absl::optional<uint32_t> cpuid_eax;  /* CPUID 1, EAX (0 for unknown) */
  /* CPU initial APIC ID (-1UL for unknown) */
  absl::optional<uint32_t> init_apic_id;
  absl::optional<int32_t> socket;   /* CPU socket number (-1 for unknown) */
  absl::optional<uint32_t> mcg_cap; /* MCG_CAP (0 for unknown) */
  absl::optional<uint16_t> cs;      /* CPU code segment */
  absl::optional<uint8_t> bank;     /* MC bank */
  absl::optional<int8_t> vendor;    /* CPU vendor (enum cpu_vendor) */
};

// Google BIOS Event log record
class Elog {
 public:
  explicit Elog(ElogRecordView view) {
    assert(view.Ok());
    data_ = std::vector<uint8_t>(view.BackingStorage().begin(),
                                 view.BackingStorage().end());
    assert(GetElogRecordView().Ok());
  }

  ElogRecordView GetElogRecordView() const {
    // return a const view
    return MakeElogRecordView(reinterpret_cast<const uint8_t *>(data_.data()),
                              data_.size());
  }

 private:
  std::vector<uint8_t> data_;
};

// This is the form in raw system events will be maintained by a logger
struct SystemEventRecord {
  // Time at which the event was logged by SystemEventReader
  absl::Time timestamp;
  absl::variant<MachineCheck, Elog> record;
};

// Abstract class to represent a reader for system events.
class SystemEventReader {
 public:
  SystemEventReader() = default;
  virtual ~SystemEventReader() = default;
  // Read a single system event. If there are no outstanding events, the method
  // returns no value. The client is expected to periodically poll the reader
  // for system events.
  virtual absl::optional<SystemEventRecord> ReadEvent() = 0;
};

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_LIB_EVENT_READER_EVENT_READER_H_
