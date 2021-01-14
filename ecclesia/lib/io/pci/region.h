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

// Defines a generic interface for interaction with a generic PCI memory region.
// This can be used to both represent a configuration space or a BAR space. A
// region has a fixed size, and allows various sized read and write operations.

#ifndef ECCLESIA_LIB_IO_PCI_REGION_H_
#define ECCLESIA_LIB_IO_PCI_REGION_H_

#include <cstddef>
#include <cstdint>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace ecclesia {

class PciRegion {
 public:
  // The type used to specify offsets into the region. We use a 64-bit offset
  // because a 64-bit BAR space can be larger than 4GiB.
  using OffsetType = uint64_t;

  explicit PciRegion(size_t size) : size_(size) {}
  virtual ~PciRegion() = default;

  // Size of this region.
  size_t Size() const { return size_; }

  // Read/write function that access within this region
  // Offset is relative to the start of the region.
  virtual absl::StatusOr<uint8_t> Read8(OffsetType offset) const = 0;
  virtual absl::Status Write8(OffsetType offset, uint8_t data) = 0;

  virtual absl::StatusOr<uint16_t> Read16(OffsetType offset) const = 0;
  virtual absl::Status Write16(OffsetType offset, uint16_t data) = 0;

  virtual absl::StatusOr<uint32_t> Read32(OffsetType offset) const = 0;
  virtual absl::Status Write32(OffsetType offset, uint32_t data) = 0;

 private:
  const size_t size_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_IO_PCI_REGION_H_
