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

#ifndef ECCLESIA_LIB_SMBIOS_BIOS_H_
#define ECCLESIA_LIB_SMBIOS_BIOS_H_

#include <cstddef>

#include "absl/strings/string_view.h"
#include "ecclesia/lib/smbios/internal.h"
#include "ecclesia/lib/smbios/structures.emb.h"

namespace ecclesia {

// Type 0 structure
class BiosInformation {
 public:
  // The constructor takes in a pointer to a smbios structure of type 0 (Bios
  // Information) and provides an emboss view to access the structure fields.
  // table_entry outlives this object
  explicit BiosInformation(const TableEntry *table_entry)
      : table_entry_(table_entry) {}

  // Given a string number found in the smbios structure, return the
  // corresponding string
  absl::string_view GetString(std::size_t num) const {
    return table_entry_->GetString(num);
  }

  // Get a message view that represents the BiosInformationStructure defined in
  // smbios_structures.emb
  BiosInformationStructureView GetMessageView() const {
    return table_entry_->GetSmbiosStructureView().bios_information();
  }

 private:
  const TableEntry *table_entry_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_SMBIOS_BIOS_H_
