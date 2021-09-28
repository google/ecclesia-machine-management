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

#ifndef ECCLESIA_LIB_IO_MMIO_H_
#define ECCLESIA_LIB_IO_MMIO_H_

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ecclesia/lib/codec/bits.h"
#include "ecclesia/lib/file/mmap.h"
#include "ecclesia/lib/status/macros.h"

namespace ecclesia {

// A type representing access to a range of memory-mapped I/O.
class MmioRange {
 public:
  // The type used to specify offsets into the memory range. We support using
  // 64-bit address spaces and so this is a 64-bit integer.
  using OffsetType = uint64_t;

  virtual ~MmioRange() = default;

  // Read/write function that access within this region
  // Offset is relative to the start of the region.
  virtual absl::StatusOr<uint8_t> Read8(OffsetType offset) const = 0;
  virtual absl::Status Write8(OffsetType offset, uint8_t data) = 0;

  virtual absl::StatusOr<uint16_t> Read16(OffsetType offset) const = 0;
  virtual absl::Status Write16(OffsetType offset, uint16_t data) = 0;

  virtual absl::StatusOr<uint32_t> Read32(OffsetType offset) const = 0;
  virtual absl::Status Write32(OffsetType offset, uint32_t data) = 0;

  size_t virtual Size() const = 0;
};

// An implementation of MmioRange backed by a memory device file.
class MmioRangeFromFile : public MmioRange {
 public:
  // Create a MMIO range backed by a memory device file, covering the specified
  // address range. The range can be specified as either a base_address+size or
  // as a AddressRange.
  //
  // If the file was successfully mapped then an object will be returned with an
  // OK status. Otherwise an error will be returned.
  static absl::StatusOr<MmioRangeFromFile> Create(
      uint64_t base_address, size_t size,
      absl::string_view physical_mem_device);
  static absl::StatusOr<MmioRangeFromFile> Create(
      AddressRange address_range, absl::string_view physical_mem_device);

  // Access to the underlying device cannot be shared between instances of this
  // object so it is not copyable. It can be moved.
  //
  // The moved-from object will be left with an empty range.
  MmioRangeFromFile(const MmioRangeFromFile &) = delete;
  MmioRangeFromFile &operator=(const MmioRangeFromFile &) = delete;
  MmioRangeFromFile(MmioRangeFromFile &&other) noexcept
      : size_(other.size_), mmap_(std::move(other.mmap_)) {
    other.size_ = 0;
  }
  MmioRangeFromFile &operator=(MmioRangeFromFile &&other) noexcept {
    size_ = other.size_;
    other.size_ = 0;
    mmap_ = std::move(other.mmap_);
    return *this;
  }

  absl::StatusOr<uint8_t> Read8(OffsetType offset) const override;
  absl::Status Write8(OffsetType offset, uint8_t value) override;

  absl::StatusOr<uint16_t> Read16(OffsetType offset) const override;
  absl::Status Write16(OffsetType offset, uint16_t value) override;

  absl::StatusOr<uint32_t> Read32(OffsetType offset) const override;
  absl::Status Write32(OffsetType offset, uint32_t value) override;

  // Size of the memory range that can be accessed by the MmioAccess.
  size_t Size() const override { return size_; }

 private:
  // Underlying constructor for the range. The constructor arguments are assumed
  // to be already valid, verified by the factory function(s).
  MmioRangeFromFile(size_t size, MappedMemory mmap)
      : size_(size), mmap_(std::move(mmap)) {}

  // Helper functions for generic-sized reads/writes.
  // The typename T specifies the size (e.g. uint32_t) to enforce aligned
  // reads/writes.
  template <typename T>
  absl::Status Read(OffsetType offset, absl::Span<uint8_t> span) const {
    if (offset % sizeof(T) != 0) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "offset %#x is not aligned with read size %d", offset, sizeof(T)));
    }

    if (offset + span.size() > Size()) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "register access %#x bytes @ %#x", span.size(), offset));
    }

    absl::Span<const uint8_t> mem = mmap_.MemoryAsReadOnlySpan<uint8_t>();
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

    if (offset + span.size() > Size()) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "register access %#x bytes @ %#x", span.size(), offset));
    }

    absl::Span<uint8_t> mem = mmap_.MemoryAsReadWriteSpan<uint8_t>();
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

  size_t size_;
  MappedMemory mmap_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_IO_MMIO_H_
