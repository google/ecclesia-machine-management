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

#ifndef ECCLESIA_LIB_MCEDECODER_MCE_MESSAGES_H_
#define ECCLESIA_LIB_MCEDECODER_MCE_MESSAGES_H_

#include <cstddef>
#include <cstdint>
#include <ctime>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ecclesia {

// A structure storing raw information of a MCE entry.
struct MceLogMessage {
  std::time_t time_stamp = 0;
  // logic-processing-unit ID, generally in range of 0~(TotalNumberCores -1)
  int lpu_id = -1;
  int bank = -1;
  uint64_t mcg_status;
  uint64_t mci_status;
  uint64_t mci_address;
  uint64_t mci_misc;

  bool operator==(const MceLogMessage& other) const {
    return std::tie(time_stamp, lpu_id, bank, mcg_status, mci_status,
                    mci_address, mci_misc) ==
           std::tie(other.time_stamp, other.lpu_id, other.bank,
                    other.mcg_status, other.mci_status, other.mci_address,
                    other.mci_misc);
  }

  bool operator!=(const MceLogMessage& other) const {
    return !(*this == other);
  }

  std::string ToString() const {
    return "time_stamp:" + std::to_string(time_stamp) +
           " lpu_id:" + std::to_string(lpu_id) +
           " mcg_status:" + std::to_string(mcg_status) +
           " mci_status:" + std::to_string(mci_status) +
           " mci_address:" + std::to_string(mci_address) +
           " mci_misc:" + std::to_string(mci_misc);
  }
};

// Decoded CPU Error bucket if the MCE event is related to CPU error.
struct CpuErrorBucket {
  int socket = -1;
  int lpu_id = -1;
  bool correctable = false;
  bool whitelisted = false;

  std::string ToString() const {
    return "socket:" + std::to_string(socket) +
           " lpu_id:" + std::to_string(lpu_id) +
           " correctable:" + (correctable ? "true" : "false") +
           " whitelisted:" + (whitelisted ? "true" : "false");
  }
};

struct CpuError {
  CpuErrorBucket cpu_error_bucket;
  uint32_t error_count = 0;

  std::string ToString() const {
    return cpu_error_bucket.ToString() +
           " error_count:" + std::to_string(error_count);
  }
};

// Decoded Memory Error bucket if the MCE event is related to memory error.
struct MemoryErrorBucket {
  // DIMM number by GLDN.
  int gldn = -1;
  bool correctable = false;

  std::string ToString() const {
    return "gldn:" + std::to_string(gldn) +
           " correctable:" + (correctable ? "true" : "false");
  }
};

struct MemoryError {
  MemoryErrorBucket mem_error_bucket;
  uint32_t error_count = 0;

  std::string ToString() const {
    return mem_error_bucket.ToString() +
           " error_count:" + std::to_string(error_count);
  }
};

// First level decoded message of a MCE event. The MCE can be further decoded to
// get detailed CPU errors or memory errors.
struct MceBucket {
  int bank = -1;
  int socket = -1;
  bool mce_corrupt = false;
  bool uncorrectable = false;
  bool processor_context_corrupted = false;

  std::string ToString() const {
    return "bank:" + std::to_string(bank) +
           " socket:" + std::to_string(socket) +
           " mce_corrupt:" + (mce_corrupt ? "true" : "false") +
           " uncorrectable:" + (uncorrectable ? "true" : "false") +
           " processor_context_corrupted:" +
           (processor_context_corrupted ? "true" : "false");
  }
};

// Decoded message of a MCE event.
struct MceDecodedMessage {
  MceBucket mce_bucket;
  std::vector<CpuError> cpu_errors;
  std::vector<MemoryError> mem_errors;

  std::string ToString() const {
    std::string str = "mce_bucket:{" + mce_bucket.ToString() + "}";
    str += " cpu_errors:{";
    for (size_t i = 0; i < cpu_errors.size(); ++i) {
      str += "{" + cpu_errors[i].ToString() + "}";
      if (i != cpu_errors.size() - 1) {
        str += ", ";
      }
    }
    str += "} ";

    str += " mem_errors:{";
    for (size_t i = 0; i < mem_errors.size(); ++i) {
      str += "{" + mem_errors[i].ToString() + "}";
      if (i != mem_errors.size() - 1) {
        str += ", ";
      }
    }
    str += "}";
    return str;
  }
};

// Machine Check Exception Attributes.
class MceAttributes {
 public:
  enum MceAttributeKey {
    kMciStatusValid,
    kMciStatusUncorrected,
    kMciStatusMiscValid,
    kMciStatusAddrValid,
    kMciStatusProcessorContexCorrupted,
    kMciStatusCorrectedErrorCount,
    kMciStatusModelSpecificErrorCode,
    kMciStatusMcaErrorCode,

    kMciStatusRegister,
    kMciMiscRegister,
    kMciAddrRegister,

    kMceBank,
    kLpuId,           // logic processing unit ID
    kSocketId,        // CPU socket ID
    kImcId,           // per CPU socket based IMC ID.
    kImcChannelId,    // per IMC based memory channel ID.
    kMemoryChannel,   // per CPU socket based memory channel.
    kImcChannelSlot,  // per IMC channel based slot ID.
    // In case of IMC error, IMC will report a second error failing rank. This
    // slot corresponds to the second error failing rank.
    kImcChannelSlotSecond,

    // Identify the type of the error. In particularly, categorize memory error
    // and cpu error.
    kUniqueId,
  };

  enum MceUniqueId {
    kIdUnknown,

    kIdImcInternalParityError,
    kIdImcHaWriteDataParityError,
    kIdImcByteEnableParityError,
    kIdImcEccErrorOnScrub,
    kIdImcSparingResilveringError,
    kIdImcHaAnyReadError,
    kIdImcDdr4CaParityError,
    kIdImcDramAddressParityError,
    kIdImcCorrectableParityError,
    kIdImcDdrtLinkRetry,

    kIdM2MMcPartialWriteError,
    kIdM2MMcDataReadError,
    kIdM2MMcTimeoutError,
    kIdM2MMcBucket1Error,
    kIdM2MMcFullWriteError,
  };

  // Get a MCE attribute using key, if there is no such attribute, return false;
  // else overwrite the input value and return true.
  template <typename T>
  bool GetAttribute(MceAttributeKey key, T* value) const {
    auto iter = attributes_.find(key);
    if (iter == attributes_.end()) {
      return false;
    }
    *value = static_cast<T>(iter->second);
    return true;
  }

  // Get a MCE attribute using key. If there is no such attribute, return the
  // default value in the input parameter.
  template <typename T>
  T GetAttributeWithDefault(MceAttributeKey key, T default_val) const {
    T value;
    if (GetAttribute(key, &value)) {
      return value;
    }
    return default_val;
  }

  // Set a MCE attribute. If the attribute already exists, overwrite the
  // existing one.
  void SetAttribute(MceAttributeKey key, uint64_t value) {
    attributes_[key] = value;
  }

 private:
  std::unordered_map<MceAttributeKey, uint64_t> attributes_;
};

}  // namespace ecclesia
#endif  // ECCLESIA_LIB_MCEDECODER_MCE_MESSAGES_H_
