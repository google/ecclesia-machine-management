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

#include "ecclesia/magent/lib/event_logger/system_event_visitors.h"

#include <cstdint>
#include <memory>
#include <type_traits>

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/lib/mcedecoder/mce_decode.h"
#include "ecclesia/lib/mcedecoder/mce_messages.h"
#include "ecclesia/magent/lib/event_reader/elog.emb.h"
#include "ecclesia/magent/lib/event_reader/event_reader.h"
#include "runtime/cpp/emboss_cpp_util.h"
#include "runtime/cpp/emboss_prelude.h"

namespace ecclesia {

namespace {

// Transform a BIOS Elog machine check message to MachineCheck
MachineCheck TransformElogToMachineCheck(
    MachineCheckExceptionView elog_mce_view) {
  MachineCheck result;
  if (elog_mce_view.Ok()) {
    result.boot = elog_mce_view.bootnum().Read();
    result.cpu = elog_mce_view.cpu().Read();
    result.bank = elog_mce_view.bank().Read();
    result.mci_status = elog_mce_view.mci_status().Read();
    result.mci_address = elog_mce_view.mci_address().Read();
    result.mci_misc = elog_mce_view.mci_misc().Read();
  }
  return result;
}

// A functor to visit absl::variant<MachineCheck, Elog> and generate cpu error
// counts
class DecodeAndUpdateCpuCounts {
 public:
  DecodeAndUpdateCpuCounts(
      MceDecoderAdapter *mce_decoder,
      absl::flat_hash_map<int, CpuErrorCount> *error_counts)
      : mce_decoder_(mce_decoder), error_counts_(error_counts) {}

  void operator()(const MachineCheck &mce) {
    if (auto decoded_mce = mce_decoder_->Decode(mce)) {
      int socket;
      for (const auto &cpu_error : decoded_mce->cpu_errors) {
        if ((socket = cpu_error.cpu_error_bucket.socket) != -1) {
          // The counter should only account for errors that are not
          // whitelisted. This is to avoid whitelisted errors to be propagated
          // through these counters which may cause "false" bad-cpu symptoms
          // (b/172862755).
          if (cpu_error.cpu_error_bucket.whitelisted) {
            continue;
          }
          if (cpu_error.cpu_error_bucket.correctable) {
            (*error_counts_)[socket].correctable += cpu_error.error_count;
          } else {
            (*error_counts_)[socket].uncorrectable += cpu_error.error_count;
          }
        }
      }
    }
  }

  void operator()(const Elog &elog) {
    auto elog_record_view = elog.GetElogRecordView();
    switch (elog_record_view.id().Read()) {
      case EventType::MACHINE_CHECK: {
        MachineCheck mce = TransformElogToMachineCheck(
            elog_record_view.machine_check_exception());
        this->operator()(mce);
        break;
      }
      default:
        break;
    }
  }

 private:
  MceDecoderAdapter *mce_decoder_;
  absl::flat_hash_map<int, CpuErrorCount> *error_counts_;
};

// A functor to visit absl::variant<MachineCheck, Elog> and generate dimm error
// counts
class DecodeAndUpdateDimmCounts {
 public:
  DecodeAndUpdateDimmCounts(
      MceDecoderAdapter *mce_decoder,
      absl::flat_hash_map<int, DimmErrorCount> *error_counts)
      : mce_decoder_(mce_decoder), error_counts_(error_counts) {}

  void operator()(const MachineCheck &mce) {
    if (auto decoded_mce = mce_decoder_->Decode(mce)) {
      int dimm_number;
      for (const auto &mem_error : decoded_mce->mem_errors) {
        if ((dimm_number = mem_error.mem_error_bucket.gldn) != -1) {
          if (mem_error.mem_error_bucket.correctable) {
            (*error_counts_)[dimm_number].correctable += mem_error.error_count;
          } else {
            (*error_counts_)[dimm_number].uncorrectable +=
                mem_error.error_count;
          }
        }
      }
    }
  }

  void operator()(const Elog &elog) {
    auto elog_record_view = elog.GetElogRecordView();
    // We only need to count uncorrectible errors, since correctible ones that
    // should get counted via machine check reported from mcedaemon
    switch (elog_record_view.id().Read()) {
      case EventType::MULTI_BIT_ECC_ERROR:
        if (!elog_record_view.multi_bit_ecc_error().Ok()) return;
        (*error_counts_)
            [elog_record_view.multi_bit_ecc_error().dimm_number().Read()]
                .uncorrectable++;
        break;
      case EventType::MACHINE_CHECK: {
        MachineCheck mce = TransformElogToMachineCheck(
            elog_record_view.machine_check_exception());
        this->operator()(mce);
        break;
      }
      default:
        break;
    }
  }

 private:
  MceDecoderAdapter *mce_decoder_;
  absl::flat_hash_map<int, DimmErrorCount> *error_counts_;
};

}  // namespace

absl::optional<MceDecodedMessage> MceDecoderAdapter::Decode(
    const MachineCheck &mce) {
  MceLogMessage raw_mce;
  if (mce.cpu) raw_mce.lpu_id = mce.cpu.value();
  if (mce.bank) raw_mce.bank = mce.bank.value();
  if (mce.mcg_status) raw_mce.mcg_status = mce.mcg_status.value();
  if (mce.mci_status) raw_mce.mci_status = mce.mci_status.value();
  if (mce.mci_address) raw_mce.mci_address = mce.mci_address.value();
  if (mce.mci_misc) raw_mce.mci_misc = mce.mci_misc.value();

  auto maybe_decoded_mce = mce_decoder_->DecodeMceMessage(raw_mce);
  if (maybe_decoded_mce.ok()) {
    return *maybe_decoded_mce;
  } else {
    return absl::nullopt;
  }
}

bool CpuErrorCountingVisitor::Visit(const SystemEventRecord &record) {
  if (record.timestamp <= lower_bound_) return false;
  // Since we are visiting the events from last to first, we just need to set
  // this once
  if (!last_record_timestamp_) {
    last_record_timestamp_ = record.timestamp;
  }
  absl::visit(DecodeAndUpdateCpuCounts(mce_decoder_.get(), &cpu_error_counts_),
              record.record);
  return true;
}

bool DimmErrorCountingVisitor::Visit(const SystemEventRecord &record) {
  if (record.timestamp <= lower_bound_) return false;
  // Since we are visiting the events from last to first, we just need to set
  // this once
  if (!last_record_timestamp_) {
    last_record_timestamp_ = record.timestamp;
  }
  absl::visit(
      DecodeAndUpdateDimmCounts(mce_decoder_.get(), &dimm_error_counts_),
      record.record);
  return true;
}

}  // namespace ecclesia
