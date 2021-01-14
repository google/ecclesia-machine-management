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

#ifndef ECCLESIA_LIB_SMBIOS_PROCESSOR_INFORMATION_H_
#define ECCLESIA_LIB_SMBIOS_PROCESSOR_INFORMATION_H_

#include <cstddef>
#include <cstdint>
#include <string>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "ecclesia/lib/smbios/internal.h"
#include "ecclesia/lib/smbios/structures.emb.h"
#include "runtime/cpp/emboss_cpp_util.h"
#include "runtime/cpp/emboss_prelude.h"

namespace ecclesia {

struct CpuSignature {
  std::string vendor;
  int type;
  int family;
  int model;
  int stepping;
};

// SMBIOS Type 4 structure
class ProcessorInformation {
 public:
  // The constructor takes in a pointer to a smbios structure of type 4
  // (Processor Information) and provides an emboss view to access the structure
  // fields. table_entry outlives this object
  explicit ProcessorInformation(const TableEntry *table_entry)
      : table_entry_(table_entry) {}

  // Given a string number found in the smbios structure, return the
  // corresponding string
  absl::string_view GetString(size_t num) const {
    return table_entry_->GetString(num);
  }

  // Get a message view that represents the ProcessorInformationStructure
  // defined in smbios_structures.emb
  ProcessorInformationStructureView GetMessageView() const {
    return table_entry_->GetSmbiosStructureView().processor_information();
  }

  // Few helper methods to avoid having to interpret the raw message view

  bool IsProcessorEnabled() const {
    uint8_t status = this->GetMessageView().status().Read();
    // Bit 6 = 1 (socket populated) Bit 2:0 = 1 (enabled)
    return status == 0x41;
  }

  bool IsIntelProcessor() const;

  uint16_t GetCoreCount() const {
    if (this->GetMessageView().has_core_count2().Value()) {
      return this->GetMessageView().core_count2().Read();
    }
    return this->GetMessageView().core_count().Read();
  }

  uint16_t GetCoreEnabled() const {
    if (this->GetMessageView().has_core_enabled2().Value()) {
      return this->GetMessageView().core_enabled2().Read();
    }
    return this->GetMessageView().core_enabled().Read();
  }

  uint16_t GetThreadCount() const {
    if (this->GetMessageView().has_thread_count2().Value()) {
      return this->GetMessageView().thread_count2().Read();
    }
    return this->GetMessageView().thread_count().Read();
  }

  int GetProcessorFamily() const {
    if (this->GetMessageView().has_processor_family2().Value()) {
      return this->GetMessageView().processor_family2().Read();
    }
    return this->GetMessageView().processor_family().Read();
  }

  absl::optional<CpuSignature> GetSignature() const;

 private:
  CpuSignature GetSignaturex86() const;
  const TableEntry *table_entry_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_SMBIOS_PROCESSOR_INFORMATION_H_
