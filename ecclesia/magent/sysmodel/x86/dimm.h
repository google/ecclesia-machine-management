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

#ifndef ECCLESIA_MAGENT_X86_DIMM_H_
#define ECCLESIA_MAGENT_X86_DIMM_H_

#include <cstdint>
#include <string>
#include <vector>

#include "ecclesia/lib/smbios/memory_device.h"
#include "ecclesia/lib/smbios/platform_translator.h"
#include "ecclesia/lib/smbios/reader.h"

namespace ecclesia {

struct DimmInfo {
  std::string slot_name;
  bool present;
  uint32_t size_mb;
  std::string type;
  uint32_t max_speed_mhz;
  uint32_t configured_speed_mhz;
  std::string manufacturer;
  std::string serial_number;
  std::string part_number;
};

class Dimm {
 public:
  Dimm(const MemoryDevice &memory_device,
       const SmbiosFieldTranslator *field_translator);

  // Allow the object to be copyable
  // Make sure that copy construction is relatively light weight.
  // In cases where it is not feasible to copy construct data members,it may
  // make sense to wrap the data member in a shared_ptr.
  Dimm(const Dimm &dimm) = default;
  Dimm &operator=(const Dimm &dimm) = default;

  const DimmInfo &GetDimmInfo() const { return dimm_info_; }

  // Possible additions will likely be methods to get dimm temperature, error
  // information, clearing logged errors etc.

 private:
  DimmInfo dimm_info_;
};

// Factory function to create Dimms
std::vector<Dimm> CreateDimms(const SmbiosReader *reader,
                              const SmbiosFieldTranslator *field_translator);

}  // namespace ecclesia
#endif  // ECCLESIA_MAGENT_X86_DIMM_H_
