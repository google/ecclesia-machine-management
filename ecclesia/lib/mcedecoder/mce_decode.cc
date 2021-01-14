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

#include "ecclesia/lib/mcedecoder/mce_decode.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ecclesia/lib/codec/bits.h"
#include "ecclesia/lib/mcedecoder/cpu_topology.h"
#include "ecclesia/lib/mcedecoder/dimm_translator.h"
#include "ecclesia/lib/mcedecoder/mce_messages.h"
#include "ecclesia/lib/mcedecoder/skylake_mce_decode.h"

namespace ecclesia {
namespace {

// Linux reports thermal throttle events as a fake machine check on bank 128.
constexpr int kLinuxThermalThrottleMceBank = 128;

using AttributeBits = std::pair<MceAttributes::MceAttributeKey, BitRange>;

void DecodeGenericIntelMceAttributes(const MceLogMessage &raw_msg,
                                     MceAttributes *attributes) {
  bool mci_status_valid = ExtractBits(raw_msg.mci_status, BitRange(63));
  attributes->SetAttribute(MceAttributes::kMciStatusValid, mci_status_valid);
  if (mci_status_valid) {
    static const std::vector<AttributeBits> &attribute_bit_map =
        *(new std::vector<AttributeBits>(
            {{MceAttributes::kMciStatusUncorrected, BitRange(61)},
             {MceAttributes::kMciStatusMiscValid, BitRange(59)},
             {MceAttributes::kMciStatusAddrValid, BitRange(58)},
             {MceAttributes::kMciStatusProcessorContexCorrupted, BitRange(57)},
             {MceAttributes::kMciStatusModelSpecificErrorCode,
              BitRange(31, 16)},
             {MceAttributes::kMciStatusMcaErrorCode, BitRange(15, 0)}}));

    for (const auto &attribut_bit : attribute_bit_map) {
      attributes->SetAttribute(
          attribut_bit.first,
          ExtractBits(raw_msg.mci_status, attribut_bit.second));
    }

    if (attributes->GetAttributeWithDefault(MceAttributes::kMciStatusValid,
                                            false)) {
      attributes->SetAttribute(MceAttributes::kMciStatusRegister,
                               raw_msg.mci_status);
    }
    if (attributes->GetAttributeWithDefault(MceAttributes::kMciStatusMiscValid,
                                            false)) {
      attributes->SetAttribute(MceAttributes::kMciMiscRegister,
                               raw_msg.mci_misc);
    }
    if (attributes->GetAttributeWithDefault(MceAttributes::kMciStatusAddrValid,
                                            false)) {
      attributes->SetAttribute(MceAttributes::kMciAddrRegister,
                               raw_msg.mci_address);
    }
    if (!attributes->GetAttributeWithDefault(
            MceAttributes::kMciStatusUncorrected, true)) {
      attributes->SetAttribute(
          MceAttributes::kMciStatusCorrectedErrorCount,
          ExtractBits(raw_msg.mci_status, BitRange(52, 38)));
    }
  }
}

// Parse the decoded Intel MCE attributes and fill in the MCE decoded message.
// Return true upon success; otherwise return false.
bool ParseIntelDecodedMceAttributes(const MceAttributes &attributes,
                                    MceDecodedMessage *decoded_msg) {
  bool flag;
  if (!attributes.GetAttribute(MceAttributes::kMciStatusValid, &flag)) {
    return false;
  }
  if (!flag ||
      (decoded_msg->cpu_errors.empty() && decoded_msg->mem_errors.empty())) {
    decoded_msg->mce_bucket.mce_corrupt = true;
  } else {
    decoded_msg->mce_bucket.mce_corrupt = false;
  }

  int tmp_value;
  if (!attributes.GetAttribute(MceAttributes::kMceBank, &tmp_value)) {
    return false;
  }
  decoded_msg->mce_bucket.bank = tmp_value;
  if (!attributes.GetAttribute(MceAttributes::kSocketId, &tmp_value)) {
    return false;
  }
  decoded_msg->mce_bucket.socket = tmp_value;
  if (attributes.GetAttribute(MceAttributes::kMciStatusUncorrected, &flag)) {
    decoded_msg->mce_bucket.uncorrectable = flag;
  }
  if (attributes.GetAttribute(MceAttributes::kMciStatusProcessorContexCorrupted,
                              &flag)) {
    decoded_msg->mce_bucket.processor_context_corrupted = flag;
  }

  return true;
}

// Decode Intel MCE. Return true upon success; otherwise return false.
absl::StatusOr<MceDecodedMessage> DecodeIntelMce(
    CpuIdentifier cpu_identifier, const MceLogMessage &raw_msg,
    DimmTranslatorInterface *dimm_translator, MceAttributes *attributes) {
  DecodeGenericIntelMceAttributes(raw_msg, attributes);
  MceDecodedMessage decoded_msg;
  // Decode model specific MCE.
  switch (cpu_identifier) {
    case CpuIdentifier::kSkylake:
    case CpuIdentifier::kCascadeLake:
      DecodeSkylakeMce(dimm_translator, attributes, &decoded_msg);
      break;
    default:
      return absl::UnimplementedError("system CPU is not supported");
  }
  if (!ParseIntelDecodedMceAttributes(*attributes, &decoded_msg)) {
    return absl::InvalidArgumentError(
        "MCE attributes are not sufficient to decode it");
  }
  return decoded_msg;
}

}  // namespace

absl::StatusOr<MceDecodedMessage> MceDecoder::DecodeMceMessage(
    const MceLogMessage &raw_msg) {
  MceAttributes mce_attributes;
  // Bypass the thermal throttle which is not a real MCE.
  if (raw_msg.bank == kLinuxThermalThrottleMceBank) {
    return absl::InvalidArgumentError("message is a thermal throttle error");
  }
  mce_attributes.SetAttribute(MceAttributes::kMceBank, raw_msg.bank);
  mce_attributes.SetAttribute(MceAttributes::kLpuId, raw_msg.lpu_id);
  if (cpu_topology_) {
    absl::StatusOr<int> maybe_socket =
        cpu_topology_->GetSocketIdForLpu(raw_msg.lpu_id);
    if (maybe_socket.ok()) {
      mce_attributes.SetAttribute(MceAttributes::kSocketId, *maybe_socket);
    }
  }

  switch (cpu_vendor_) {
    case CpuVendor::kIntel:
      return DecodeIntelMce(cpu_identifier_, raw_msg, dimm_translator_.get(),
                            &mce_attributes);
    default:
      return absl::UnimplementedError("CPU vendor is not supported");
  }
}

}  // namespace ecclesia
