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

#include "ecclesia/magent/sysmodel/x86/dimm.h"

#include <cstdint>
#include <string>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/smbios/memory_device.h"
#include "ecclesia/lib/smbios/platform_translator.h"
#include "ecclesia/lib/smbios/reader.h"
#include "ecclesia/lib/smbios/structures.emb.h"
#include "runtime/cpp/emboss_cpp_util.h"
#include "runtime/cpp/emboss_prelude.h"

namespace ecclesia {

namespace {

constexpr uint16_t kUnknownSize = 0xffff;
constexpr uint16_t kExtendedSizeValid = 0x7fff;

// Decode the DIMM size from the size and extended_size fields in the smbios
// structure
template <typename View>
uint32_t DecodeDimmSizeMB(View message_view) {
  uint16_t size = message_view.size().Read();
  uint32_t extended_size = message_view.extended_size().Read();
  if (size == 0 || size == kUnknownSize) return 0;
  if (size != kExtendedSizeValid) {
    return size;
  } else {
    return extended_size;
  }
}

}  // namespace

Dimm::Dimm(const MemoryDevice &memory_device,
           const SmbiosFieldTranslator *field_translator) {
  auto message_view = memory_device.GetMessageView();

  dimm_info_.slot_name = field_translator->GetDimmSlotName(
      memory_device.GetString(message_view.device_locator_snum().Read()));
  dimm_info_.size_mb = DecodeDimmSizeMB(message_view);

  if (dimm_info_.size_mb) {
    dimm_info_.present = true;
    dimm_info_.type = TryToGetNameFromEnum(message_view.memory_type().Read());
    dimm_info_.max_speed_mhz = message_view.speed().Read();

    dimm_info_.configured_speed_mhz =
        message_view.configured_memory_clock_speed().Read();

    dimm_info_.manufacturer = std::string(
        memory_device.GetString(message_view.manufacturer_snum().Read()));

    dimm_info_.serial_number = std::string(
        memory_device.GetString(message_view.serial_number_snum().Read()));

    dimm_info_.part_number = std::string(
        memory_device.GetString(message_view.part_number_snum().Read()));
  } else {
    dimm_info_.present = false;
  }
}

std::vector<Dimm> CreateDimms(const SmbiosReader *reader,
                              const SmbiosFieldTranslator *field_translator) {
  std::vector<Dimm> dimms;
  for (auto &memory_device : reader->GetAllMemoryDevices()) {
    dimms.emplace_back(memory_device, field_translator);
  }
  return dimms;
}

}  // namespace ecclesia
