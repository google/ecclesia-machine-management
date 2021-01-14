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

// Provides a standard convenient implementation for opening a file for memory
// mapped file access, particularly for file-backed memory mapped I/O.

#ifndef ECCLESIA_LIB_FILE_MMAP_H_
#define ECCLESIA_LIB_FILE_MMAP_H_

#include <cstddef>
#include <cstdint>
#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"

namespace ecclesia {

class MappedMemory {
 private:
  // Helper used by the span functions to verify that their type parameter is
  // one of the supported character/byte types.
  template <typename T>
  static inline constexpr bool IsAllowedSpanType =
      std::is_same_v<T, char> || std::is_same_v<T, unsigned char> ||
      std::is_same_v<T, uint8_t> || std::is_same_v<T, std::byte>;

 public:
  // Given a filename, attempt to open it and create a memory mapping of a range
  // of the file. The exposed byte range will be [offset, offset+size). Returns
  // a null object if opening the file or creating the mapping fails.
  //
  // There are two options, a read-only mapping and a read-write one. The
  // read-only version will create a private mapping and only the accessors
  // which provide const buffers will return usable values. The read-write
  // version will create a shared mapping and support the full set of accessors.
  // There is no mechanism to create a private mapping which is writable.
  enum class Type { kReadOnly, kReadWrite };
  static absl::StatusOr<MappedMemory> Create(const std::string &path,
                                             size_t offset, size_t size,
                                             Type type);

  // This object cannot be shared so copying is not allowed. It can be moved
  // with ownership of the underlying mapping moving along with the file.
  //
  // Using a moved-from mapping will return empty spans and views.
  MappedMemory(const MappedMemory &) = delete;
  MappedMemory &operator=(const MappedMemory &) = delete;
  MappedMemory(MappedMemory &&other) : mapping_(other.mapping_) {
    other.mapping_ = {nullptr, 0, 0, 0, false};
  }
  MappedMemory &operator=(MappedMemory &&other) {
    mapping_ = other.mapping_;
    other.mapping_ = {nullptr, 0, 0, 0, false};
    return *this;
  }

  // Destorying the object will release the underlying mapping.
  ~MappedMemory();

  // Expose the mapped memory as a string view.
  absl::string_view MemoryAsStringView() const;

  // Exposed the mapped memory as a span. You can request either a read-only
  // span or a read-write span, but if the underlying mapping is not writable
  // then the read-write span will be empty.
  //
  // Note that these functions do not allow you to specify the span as wrapping
  // non-byte types. If you need to transform the buffer into a larger structure
  // then either use the ecclesia/lib/codec/endian.h (for integer access) or an
  // emboss view (for more complex types).
  template <typename CharType = char>
  absl::Span<const CharType> MemoryAsReadOnlySpan() const {
    static_assert(IsAllowedSpanType<CharType>);
    return absl::MakeConstSpan(
        static_cast<CharType *>(mapping_.addr) + mapping_.user_offset,
        mapping_.user_size);
  }
  template <typename CharType = char>
  absl::Span<CharType> MemoryAsReadWriteSpan() const {
    static_assert(IsAllowedSpanType<CharType>);
    if (mapping_.writable) {
      return absl::MakeSpan(
          static_cast<CharType *>(mapping_.addr) + mapping_.user_offset,
          mapping_.user_size);
    } else {
      return absl::Span<CharType>();
    }
  }

 private:
  // Construct a mapping object. This constructor is only used by the factory
  // functions for this object, which do all the validation of parameters.
  struct MmapInfo {
    // The address and size of the mapping. Set to null in moved-from mappings.
    void *addr;
    size_t size;
    // The offset and size of the subset of the mapping exposed to the user. The
    // actually underlying mapping will generally be larger (due to page
    // alignment) than the mapping requested by the user.
    size_t user_offset;
    size_t user_size;
    // Indicates if the mapping is writable or not.
    bool writable;
  };
  explicit MappedMemory(MmapInfo mapping);

  // All the information about the stored mapping.
  MmapInfo mapping_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_FILE_MMAP_H_
