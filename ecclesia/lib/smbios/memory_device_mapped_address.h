/*
 * Copyright 2023 Google LLC
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

#ifndef ECCLESIA_LIB_SMBIOS_MEMORY_DEVICE_MAPPED_ADDRESS_H_
#define ECCLESIA_LIB_SMBIOS_MEMORY_DEVICE_MAPPED_ADDRESS_H_

#include "ecclesia/lib/smbios/internal.h"
#include "ecclesia/lib/smbios/structures.emb.h"

namespace ecclesia {

// Type 20 structure
class MemoryDeviceMappedAddress {
 public:
  // The constructor takes in a pointer to a smbios structure of type 20 (Memory
  // Device Mapped Address) and provides an emboss view to access the structure
  // fields. table_entry outlives this object.
  explicit MemoryDeviceMappedAddress(const TableEntry *table_entry)
      : table_entry_(table_entry) {}

  // Get a message view that represents the MemoryDeviceMappedAddressStructure
  // defined in structures.emb
  MemoryDeviceMappedAddressStructureView GetMessageView() const {
    return table_entry_->GetSmbiosStructureView()
        .memory_device_mapped_address();
  }

 private:
  const TableEntry *table_entry_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_SMBIOS_MEMORY_DEVICE_MAPPED_ADDRESS_H_
