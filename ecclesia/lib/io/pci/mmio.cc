/*
 * Copyright 2021 Google LLC
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

#include "ecclesia/lib/io/pci/mmio.h"

#include <cstdint>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ecclesia/lib/codec/endian.h"
#include "ecclesia/lib/file/mmap.h"
#include "ecclesia/lib/io/pci/location.h"
#include "ecclesia/lib/status/macros.h"

namespace ecclesia {

PciMmioRegion::PciMmioRegion(absl::string_view physical_mem_device,
                             uint64_t pci_mmconfig_base,
                             const PciDbdfLocation &location)
    : PciRegion(kPciConfigSpaceSize),
      mmio_(MappedMemory::Create(std::string(physical_mem_device),
                                 pci_mmconfig_base + LocationToOffset(location),
                                 Size(), MappedMemory::Type::kReadWrite)) {}

absl::StatusOr<uint8_t> PciMmioRegion::Read8(OffsetType offset) const {
  ECCLESIA_RETURN_IF_ERROR(mmio_.status());
  uint8_t result;
  ECCLESIA_RETURN_IF_ERROR(
      ReadConfig<uint8_t>(offset, absl::MakeSpan(&result, sizeof(result))));
  return result;
}

absl::Status PciMmioRegion::Write8(OffsetType offset, uint8_t value) {
  ECCLESIA_RETURN_IF_ERROR(mmio_.status());
  ECCLESIA_RETURN_IF_ERROR(
      WriteConfig<uint8_t>(offset, absl::MakeSpan(&value, sizeof(value))));
  return absl::OkStatus();
}

absl::StatusOr<uint16_t> PciMmioRegion::Read16(OffsetType offset) const {
  ECCLESIA_RETURN_IF_ERROR(mmio_.status());
  uint8_t data[sizeof(uint16_t)];
  ECCLESIA_RETURN_IF_ERROR(
      ReadConfig<uint16_t>(offset, absl::MakeSpan(data, sizeof(data))));
  return LittleEndian::Load16(data);
}

absl::Status PciMmioRegion::Write16(OffsetType offset, uint16_t value) {
  ECCLESIA_RETURN_IF_ERROR(mmio_.status());
  uint8_t data[sizeof(uint16_t)];
  LittleEndian::Store16(value, data);
  return WriteConfig<uint16_t>(offset, absl::MakeSpan(data, sizeof(data)));
}

absl::StatusOr<uint32_t> PciMmioRegion::Read32(OffsetType offset) const {
  ECCLESIA_RETURN_IF_ERROR(mmio_.status());
  uint8_t data[sizeof(uint32_t)];
  ECCLESIA_RETURN_IF_ERROR(
      ReadConfig<uint32_t>(offset, absl::MakeSpan(data, sizeof(data))));
  return LittleEndian::Load32(data);
}

absl::Status PciMmioRegion::Write32(OffsetType offset, uint32_t value) {
  ECCLESIA_RETURN_IF_ERROR(mmio_.status());
  uint8_t data[sizeof(uint32_t)];
  LittleEndian::Store32(value, data);
  return WriteConfig<uint32_t>(offset, absl::MakeSpan(data, sizeof(data)));
}

}  // namespace ecclesia
