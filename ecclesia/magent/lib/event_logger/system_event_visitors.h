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

#ifndef ECCLESIA_MAGENT_LIB_EVENT_LOGGER_SYSTEM_EVENT_VISITORS_H_
#define ECCLESIA_MAGENT_LIB_EVENT_LOGGER_SYSTEM_EVENT_VISITORS_H_

#include <cstdint>
#include <memory>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "ecclesia/lib/mcedecoder/mce_decode.h"
#include "ecclesia/lib/mcedecoder/mce_messages.h"
#include "ecclesia/magent/lib/event_logger/event_logger.h"
#include "ecclesia/magent/lib/event_reader/event_reader.h"

namespace ecclesia {
// struct to keep track of the count of correctable and uncorrectable errors per
// cpu / socket
struct CpuErrorCount {
  int64_t correctable;
  int64_t uncorrectable;

  bool operator==(const CpuErrorCount &rhs) const {
    return this->correctable == rhs.correctable &&
           this->uncorrectable == rhs.uncorrectable;
  }

  CpuErrorCount &operator+=(const CpuErrorCount &rhs) {
    this->correctable += rhs.correctable;
    this->uncorrectable += rhs.uncorrectable;
    return *this;
  }
};

// struct to keep track of the count of correctable and uncorrectable errors per
// dimm
struct DimmErrorCount {
  int64_t correctable;
  int64_t uncorrectable;

  bool operator==(const DimmErrorCount &rhs) const {
    return this->correctable == rhs.correctable &&
           this->uncorrectable == rhs.uncorrectable;
  }

  DimmErrorCount &operator+=(const DimmErrorCount &rhs) {
    this->correctable += rhs.correctable;
    this->uncorrectable += rhs.uncorrectable;
    return *this;
  }
};

// The MceDecoder expects different interface for providing the raw
// mces. This adapter allows us to translate the ecclesia::MachineCheck into
// MceLogMessage and perform the decoding.
class MceDecoderAdapter {
 public:
  MceDecoderAdapter(std::unique_ptr<MceDecoderInterface> mce_decoder)
      : mce_decoder_(std::move(mce_decoder)) {}

  absl::optional<MceDecodedMessage> Decode(const MachineCheck &mce);

 private:
  std::unique_ptr<MceDecoderInterface> mce_decoder_;
};

// A system event visitor for deriving cpu error counts
class CpuErrorCountingVisitor : public SystemEventVisitor {
 public:
  CpuErrorCountingVisitor(absl::Time lower_bound,
                          std::unique_ptr<MceDecoderAdapter> mce_decoder)
      : SystemEventVisitor(SystemEventVisitor::VisitDirection::FROM_END),
        lower_bound_(lower_bound),
        mce_decoder_(std::move(mce_decoder)) {}

  bool Visit(const SystemEventRecord &record) override;

  absl::optional<absl::Time> GetLatestRecordTimeStamp() const {
    return last_record_timestamp_;
  }

  absl::flat_hash_map<int, CpuErrorCount> GetCpuErrorCounts() const {
    return cpu_error_counts_;
  }

 private:
  absl::Time lower_bound_;
  std::unique_ptr<MceDecoderAdapter> mce_decoder_;
  // Time stamp of the latest(first, since visit direction is from end) record
  // that was visited. Providing this information to the client will allow for
  // incremental accumulation of error counts
  absl::optional<absl::Time> last_record_timestamp_;

  // The visitor constructs this map as it visits each system event record.
  // Key is the cpu / socket number
  absl::flat_hash_map<int, CpuErrorCount> cpu_error_counts_;
};

// A system event visitor for deriving memory error counts
class DimmErrorCountingVisitor : public SystemEventVisitor {
 public:
  DimmErrorCountingVisitor(absl::Time lower_bound,
                           std::unique_ptr<MceDecoderAdapter> mce_decoder)
      : SystemEventVisitor(SystemEventVisitor::VisitDirection::FROM_END),
        lower_bound_(lower_bound),
        mce_decoder_(std::move(mce_decoder)) {}

  bool Visit(const SystemEventRecord &record) override;

  absl::optional<absl::Time> GetLatestRecordTimeStamp() const {
    return last_record_timestamp_;
  }

  absl::flat_hash_map<int, DimmErrorCount> GetDimmErrorCounts() const {
    return dimm_error_counts_;
  }

 private:
  absl::Time lower_bound_;
  std::unique_ptr<MceDecoderAdapter> mce_decoder_;
  // Time stamp of the latest(first, since visit direction is from end) record
  // that was visited. Providing this information to the client will allow for
  // incremental accumulation of error counts
  absl::optional<absl::Time> last_record_timestamp_;

  // The visitor constructs this map as it visits each system event record.
  // Key is the dimm number
  absl::flat_hash_map<int, DimmErrorCount> dimm_error_counts_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_LIB_EVENT_LOGGER_SYSTEM_EVENT_VISITORS_H_
