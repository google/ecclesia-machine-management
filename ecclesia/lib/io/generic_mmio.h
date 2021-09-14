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

#ifndef ECCLESIA_LIB_IO_GENERIC_MMIO_H_
#define ECCLESIA_LIB_IO_GENERIC_MMIO_H_

#include <cstdint>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/codec/bits.h"
#include "ecclesia/lib/file/mmap.h"
#include "ecclesia/lib/status/macros.h"

namespace ecclesia {

class MmioAccessInterface {
 public:
  // The type used to specify offsets into the memory range.
  using OffsetType = uint64_t;

  virtual ~MmioAccessInterface() = default;

  size_t virtual Size() const = 0;
  // Read/write function that access within this region
  // Offset is relative to the start of the region.
  virtual absl::StatusOr<uint8_t> Read8(OffsetType offset) const = 0;
  virtual absl::Status Write8(OffsetType offset, uint8_t data) = 0;

  virtual absl::StatusOr<uint16_t> Read16(OffsetType offset) const = 0;
  virtual absl::Status Write16(OffsetType offset, uint16_t data) = 0;

  virtual absl::StatusOr<uint32_t> Read32(OffsetType offset) const = 0;
  virtual absl::Status Write32(OffsetType offset, uint32_t data) = 0;
};

// Creates a generic read-write MMIO accessor into a memory range.
class MmioAccess : public MmioAccessInterface {
 public:
  // Args:
  //   size: size of the range.
  //   first_address: The start address to the range. The exposed byte range
  //   that can be accessed by MmioRegion will be [start_address,
  //   first_address+size).
  //   physical_mem_device: path to physical memory device (e.g. /dev/mem)
  MmioAccess(size_t size, uint64_t first_address,
             absl::string_view physical_mem_device);

  MmioAccess(AddressRange mmap_addr_range,
             absl::string_view physical_mem_device);

  ~MmioAccess() = default;

  absl::StatusOr<uint8_t> Read8(OffsetType offset) const override;
  absl::Status Write8(OffsetType offset, uint8_t value) override;

  absl::StatusOr<uint16_t> Read16(OffsetType offset) const override;
  absl::Status Write16(OffsetType offset, uint16_t value) override;

  absl::StatusOr<uint32_t> Read32(OffsetType offset) const override;
  absl::Status Write32(OffsetType offset, uint32_t value) override;

  // Size of the memory range that can be accessed by the MmioAccess.
  size_t Size() const override { return size_; }

 private:
  // Helper functions for generic-sized reads/writes.
  // The typename T specifies the size (e.g. uint32_t) to enforce aligned
  // reads/writes.
  template <typename T>
  absl::Status Read(OffsetType offset, absl::Span<uint8_t> span) const {
    if (offset % sizeof(T) != 0) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "offset %#x is not aligned with read size %d", offset, sizeof(T)));
    }

    ECCLESIA_RETURN_IF_ERROR(mmio_.status());
    if (offset + span.size() > Size()) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "register access %#x bytes @ %#x", span.size(), offset));
    }

    absl::Span<const uint8_t> mem =
        mmio_.value().MemoryAsReadOnlySpan<uint8_t>();
    // This is the critical part that forces an aligned, specific-size data copy
    // from mmap'd PCI config space.
    const T typed_value =
        *reinterpret_cast<const volatile T *>(mem.begin() + offset);
    std::memcpy(span.data(), &typed_value, sizeof(typed_value));
    return absl::OkStatus();
  }

  template <typename T>
  absl::Status Write(OffsetType offset, absl::Span<const uint8_t> span) {
    if (offset % sizeof(T) != 0) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "offset %#x is not aligned with read size %d", offset, sizeof(T)));
    }

    ECCLESIA_RETURN_IF_ERROR(mmio_.status());
    if (offset + span.size() > Size()) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "register access %#x bytes @ %#x", span.size(), offset));
    }

    absl::Span<uint8_t> mem = mmio_.value().MemoryAsReadWriteSpan<uint8_t>();
    if (mem.empty()) {
      return absl::InternalError("could not get mapped memory for writing");
    }
    // This is the critical part that forces an aligned, specific-size data copy
    // to mmap'd PCI config space.
    volatile T *typed_dest =
        reinterpret_cast<volatile T *>(mem.data() + offset);
    *typed_dest = *reinterpret_cast<const T *>(span.data());
    return absl::OkStatus();
  }

  const size_t size_;
  absl::StatusOr<MappedMemory> mmio_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_IO_GENERIC_MMIO_H_
