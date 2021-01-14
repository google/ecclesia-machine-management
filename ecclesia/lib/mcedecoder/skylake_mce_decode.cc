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

#include "ecclesia/lib/mcedecoder/skylake_mce_decode.h"

#include <cstdint>
#include <set>
#include <vector>

#include "absl/status/statusor.h"
#include "ecclesia/lib/codec/bits.h"
#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/lib/mcedecoder/dimm_translator.h"
#include "ecclesia/lib/mcedecoder/mce_messages.h"

namespace ecclesia {
namespace {

constexpr int kSkylakeNumChannelPerImc = 3;
constexpr int kSkylakeNumRankPerSlot = 4;

constexpr int kAddressParityError = 0x0001;
constexpr int kHaWrDataParityError = 0x0002;
constexpr int kHaWrBEParityError = 0x0004;
constexpr int kCorrPatrolScrubError = 0x0008;
constexpr int kUnCorrPatrolScrubError = 0x0010;
constexpr int kCorrSpareError = 0x0020;
constexpr int kUnCorrSpareError = 0x0040;
constexpr int kAnyHaRdError = 0x0080;
constexpr int kWdbReadParityError = 0x0100;
constexpr int kDdr4CaParity = 0x0200;
constexpr int kUnCorrAddressParityError = 0x0400;
constexpr int kCorrectableParityError = 0x0806;
constexpr int kDdrtLinkRetry = 0x080d;

enum SkylakeMceBank {
  kIfuBank = 0,
  kDcuBank = 1,
  kDtlbBank = 2,
  kMclBank = 3,
  kPcuBank = 4,
  kUpi0Bank = 5,
  kIioBank = 6,
  kM2m0Bank = 7,
  kM2m1Bank = 8,
  kCha0Bank = 9,
  kCha1Bank = 10,
  kCha2Bank = 11,
  kUpi1Bank = 12,
  kImc0Ch0Bank = 13,
  kImc0Ch1Bank = 14,
  kImc1Ch0Bank = 15,
  kImc1Ch1Bank = 16,
  kImc0Ch2Bank = 17,
  kImc1Ch2Bank = 18,
  kUpi2Bank = 19,
};

bool DecodeSkylakeM2MBank(int imc_id, MceAttributes *attributes) {
  attributes->SetAttribute(MceAttributes::kImcId, imc_id);
  uint64_t misc_reg;
  if (!attributes->GetAttribute(MceAttributes::kMciMiscRegister, &misc_reg)) {
    ErrorLog() << "Failed to get mci misc register to decode M2M bank.";
    return false;
  }

  // IA32_MC7_MISC Bits(51:50): Physical memory channel which had the error.
  static constexpr BitRange kMciMiscMcCmdChannelBits(51, 50);
  unsigned int imc_channel_id = ExtractBits(misc_reg, kMciMiscMcCmdChannelBits);
  attributes->SetAttribute(MceAttributes::kImcChannelId, imc_channel_id);
  attributes->SetAttribute(MceAttributes::kMemoryChannel,
                           imc_id * kSkylakeNumChannelPerImc + imc_channel_id);
  // Assume the DIMM is always populated at slot 0 only. This assumption is
  // generally valid for Indus machine. For other configuration, we will need to
  // implement the physical memory address decoding.
  attributes->SetAttribute(MceAttributes::kImcChannelSlot, 0);

  MceAttributes::MceUniqueId unique_id = MceAttributes::kIdUnknown;
  uint64_t mci_status;
  if (attributes->GetAttribute(MceAttributes::kMciStatusRegister,
                               &mci_status)) {
    if (ExtractBits(mci_status, BitRange(23))) {
      unique_id = MceAttributes::kIdM2MMcBucket1Error;
    }
    if (ExtractBits(mci_status, BitRange(21))) {
      unique_id = MceAttributes::kIdM2MMcTimeoutError;
    }
    if (ExtractBits(mci_status, BitRange(19))) {
      unique_id = MceAttributes::kIdM2MMcFullWriteError;
    }
    if (ExtractBits(mci_status, BitRange(18))) {
      unique_id = MceAttributes::kIdM2MMcPartialWriteError;
    }
    if (ExtractBits(mci_status, BitRange(16))) {
      unique_id = MceAttributes::kIdM2MMcDataReadError;
    }
  }
  attributes->SetAttribute(MceAttributes::kUniqueId, unique_id);
  return true;
}

bool DecodeSkylakeImcBank(int imc_id, int imc_channel_id,
                          MceAttributes *attributes) {
  attributes->SetAttribute(MceAttributes::kImcId, imc_id);
  attributes->SetAttribute(MceAttributes::kImcChannelId, imc_channel_id);
  attributes->SetAttribute(MceAttributes::kMemoryChannel,
                           imc_id * kSkylakeNumChannelPerImc + imc_channel_id);
  uint64_t misc_reg;
  if (!attributes->GetAttribute(MceAttributes::kMciMiscRegister, &misc_reg)) {
    ErrorLog() << "Failed to get mci misc register to decode IMC bank.";
    return false;
  }

  static constexpr BitRange kMciMiscFirstErrRankBits(50, 46);
  static constexpr BitRange kMciMiscSecondErrRankBits(55, 51);
  uint64_t first_rank = ExtractBits(misc_reg, kMciMiscFirstErrRankBits);
  attributes->SetAttribute(MceAttributes::kImcChannelSlot,
                           first_rank / kSkylakeNumRankPerSlot);
  uint64_t second_rank = ExtractBits(misc_reg, kMciMiscSecondErrRankBits);
  attributes->SetAttribute(MceAttributes::kImcChannelSlotSecond,
                           second_rank / kSkylakeNumRankPerSlot);
  MceAttributes::MceUniqueId unique_id = MceAttributes::kIdUnknown;
  uint32_t model_specific_error_code;
  if (attributes->GetAttributeWithDefault(MceAttributes::kMciStatusValid,
                                          false) &&
      attributes->GetAttribute(MceAttributes::kMciStatusModelSpecificErrorCode,
                               &model_specific_error_code)) {
    switch (model_specific_error_code) {
      case kAddressParityError:
        unique_id = MceAttributes::kIdImcInternalParityError;
        break;
      case kHaWrDataParityError:
        unique_id = MceAttributes::kIdImcHaWriteDataParityError;
        break;
      case kHaWrBEParityError:
        unique_id = MceAttributes::kIdImcByteEnableParityError;
        break;
      case kCorrPatrolScrubError:
        unique_id = MceAttributes::kIdImcEccErrorOnScrub;
        break;
      case kUnCorrPatrolScrubError:
        unique_id = MceAttributes::kIdImcEccErrorOnScrub;
        break;
      case kCorrSpareError:
        unique_id = MceAttributes::kIdImcSparingResilveringError;
        break;
      case kUnCorrSpareError:
        unique_id = MceAttributes::kIdImcSparingResilveringError;
        break;
      case kAnyHaRdError:
        unique_id = MceAttributes::kIdImcHaAnyReadError;
        break;
      case kWdbReadParityError:
        unique_id = MceAttributes::kIdImcInternalParityError;
        break;
      case kDdr4CaParity:
        unique_id = MceAttributes::kIdImcDdr4CaParityError;
        break;
      case kUnCorrAddressParityError:
        unique_id = MceAttributes::kIdImcDramAddressParityError;
        break;
      case kCorrectableParityError:
        unique_id = MceAttributes::kIdImcCorrectableParityError;
        break;
      case kDdrtLinkRetry:
        unique_id = MceAttributes::kIdImcDdrtLinkRetry;
        break;
    }
  }
  attributes->SetAttribute(MceAttributes::kUniqueId, unique_id);
  return true;
}

// Decode Skylake attributes based on the bank. Modify the input attributes
// pointer and return true upon successful decoding; otherwise return false.
bool DecodeSkylakeBankAttributes(MceAttributes *attributes) {
  uint64_t bank_id;
  if (!attributes->GetAttribute(MceAttributes::kMceBank, &bank_id)) {
    ErrorLog() << "Failed to get MCE bank to decode bank attribute.";
    return false;
  }
  SkylakeMceBank bank = static_cast<SkylakeMceBank>(bank_id);

  // Only decode memory related banks for now. May need to add decoding to other
  // banks.
  switch (bank) {
    case kM2m0Bank:
      return DecodeSkylakeM2MBank(0, attributes);
    case kM2m1Bank:
      return DecodeSkylakeM2MBank(1, attributes);
    case kImc0Ch0Bank:
      return DecodeSkylakeImcBank(0, 0, attributes);
    case kImc0Ch1Bank:
      return DecodeSkylakeImcBank(0, 1, attributes);
    case kImc1Ch0Bank:
      return DecodeSkylakeImcBank(1, 0, attributes);
    case kImc1Ch1Bank:
      return DecodeSkylakeImcBank(1, 1, attributes);
    case kImc0Ch2Bank:
      return DecodeSkylakeImcBank(0, 2, attributes);
    case kImc1Ch2Bank:
      return DecodeSkylakeImcBank(1, 2, attributes);
    default:
      return true;
  }
}

// Decode Skylake memory error. Modify the input decoded message pointer and
// return true upon successful decoding; otherwise return false.
bool DecodeSkylakeMemoryError(const MceAttributes &attributes,
                              DimmTranslatorInterface *dimm_translator,
                              MceDecodedMessage *decoded_msg) {
  // For uncorrected error, kMciStatusCorrectedErrorCount will not be available
  // and the error count will be set to 1.
  uint32_t total_error_count = attributes.GetAttributeWithDefault(
      MceAttributes::kMciStatusCorrectedErrorCount, 1);

  int accounted_mem_error_count = 0;
  bool uncorrected;
  if (!attributes.GetAttribute(MceAttributes::kMciStatusUncorrected,
                               &uncorrected)) {
    ErrorLog()
        << "Failed to get 'uncorrected' attribute to decode memory error.";
    return false;
  }
  // Decode first memory error reported in the MCE.
  MemoryError first_mem_error;
  first_mem_error.error_count = 1;
  first_mem_error.mem_error_bucket.correctable = !uncorrected;
  int socket_id, imc_channel, channel_slot;
  if (!attributes.GetAttribute(MceAttributes::kSocketId, &socket_id) ||
      !attributes.GetAttribute(MceAttributes::kMemoryChannel, &imc_channel) ||
      !attributes.GetAttribute(MceAttributes::kImcChannelSlot, &channel_slot)) {
    return false;
  }
  absl::StatusOr<int> maybe_gldn =
      dimm_translator->GetGldn({.socket_id = socket_id,
                                .imc_channel = imc_channel,
                                .channel_slot = channel_slot});
  if (!maybe_gldn.ok()) return false;
  first_mem_error.mem_error_bucket.gldn = *maybe_gldn;
  decoded_msg->mem_errors.push_back(first_mem_error);
  accounted_mem_error_count++;

  // Decode the second memory error provided by the IMC.
  if (total_error_count > accounted_mem_error_count) {
    MemoryError second_mem_error;
    second_mem_error.error_count = 1;
    second_mem_error.mem_error_bucket.correctable = !uncorrected;
    int second_error_channel_slot;
    if (!attributes.GetAttribute(MceAttributes::kImcChannelSlotSecond,
                                 &second_error_channel_slot)) {
      return false;
    }
    absl::StatusOr<int> maybe_second_gldn =
        dimm_translator->GetGldn({.socket_id = socket_id,
                                  .imc_channel = imc_channel,
                                  .channel_slot = second_error_channel_slot});
    if (!maybe_second_gldn.ok()) {
      return false;
    }
    second_mem_error.mem_error_bucket.gldn = *maybe_second_gldn;
    decoded_msg->mem_errors.push_back(second_mem_error);
    accounted_mem_error_count++;
  }

  // Handle the remaining errors.
  if (total_error_count > accounted_mem_error_count) {
    MemoryError remaining_mem_error;
    remaining_mem_error.mem_error_bucket.correctable = !uncorrected;
    remaining_mem_error.mem_error_bucket.gldn = *maybe_gldn;
    remaining_mem_error.error_count =
        total_error_count - accounted_mem_error_count;
    decoded_msg->mem_errors.push_back(remaining_mem_error);
  }
  return true;
}

// Decode Skylake CPU error. Modify the input decoded message pointer and return
// true upon successful decoding; otherwise return false.
bool DecodeSkylakeCpuError(const MceAttributes &attributes,
                           MceDecodedMessage *decoded_msg) {
  CpuError cpu_error;
  int tmp_value;
  if (!attributes.GetAttribute(MceAttributes::kSocketId, &tmp_value)) {
    ErrorLog() << "Failed to get socket ID to decode CPU error.";
    return false;
  }
  cpu_error.cpu_error_bucket.socket = tmp_value;
  if (!attributes.GetAttribute(MceAttributes::kLpuId, &tmp_value)) {
    ErrorLog() << "Failed to get LPU ID to decode CPU error.";
    return false;
  }
  cpu_error.cpu_error_bucket.lpu_id = tmp_value;
  bool uncorrected;
  if (!attributes.GetAttribute(MceAttributes::kMciStatusUncorrected,
                               &uncorrected)) {
    ErrorLog() << "Failed to get 'uncorrected' attribute to decode CPU error.";
    return false;
  }
  cpu_error.cpu_error_bucket.correctable = !uncorrected;
  // Skylake has no whitelist cpu error. Hardcode this field to false.
  cpu_error.cpu_error_bucket.whitelisted = false;

  // For uncorrected error, kMciStatusCorrectedErrorCount will not be available
  // and the error count will be set to 1.
  cpu_error.error_count = attributes.GetAttributeWithDefault(
      MceAttributes::kMciStatusCorrectedErrorCount, 1);

  decoded_msg->cpu_errors.push_back(cpu_error);
  return true;
}

}  // namespace

bool DecodeSkylakeMce(DimmTranslatorInterface *dimm_translator,
                      MceAttributes *attributes,
                      MceDecodedMessage *decoded_msg) {
  if (!DecodeSkylakeBankAttributes(attributes)) {
    return false;
  }

  static const std::set<MceAttributes::MceUniqueId> &memory_unique_ids =
      *(new std::set<MceAttributes::MceUniqueId>(
          {MceAttributes::kIdImcEccErrorOnScrub,
           MceAttributes::kIdImcHaAnyReadError,
           MceAttributes::kIdImcSparingResilveringError,
           MceAttributes::kIdImcDdr4CaParityError,
           MceAttributes::kIdImcCorrectableParityError,
           MceAttributes::kIdImcDdrtLinkRetry,
           MceAttributes::kIdM2MMcPartialWriteError,
           MceAttributes::kIdM2MMcDataReadError,
           MceAttributes::kIdM2MMcFullWriteError}));

  MceAttributes::MceUniqueId unique_id = attributes->GetAttributeWithDefault(
      MceAttributes::kUniqueId, MceAttributes::kIdUnknown);

  if (memory_unique_ids.count(unique_id)) {
    return DecodeSkylakeMemoryError(*attributes, dimm_translator, decoded_msg);
  } else {
    return DecodeSkylakeCpuError(*attributes, decoded_msg);
  }
}
}  // namespace ecclesia
