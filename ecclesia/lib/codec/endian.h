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

// This library provides portable functions for reading bytes into integers and
// for storing integers into bytes. The design is based around the expectation
// that you know the endianness of the underlying byte format and need to
// translate to/from integer types.
//
// It does not attempt to provide an equivalent to "host order" operations along
// the lines of what POSIX functions like hton[sl] and ntoh[sl] do since these
// operations are non-portable and encourage viewing integer objects as having
// an inherent endianness that can be changed by swizzling their bytes.
//
// Rather than trying to use conditional compilation to try and do different
// "optimized" versions of loads and stores on little and big endian machines,
// this code takes the simpler approach of implementing the transformations
// using bitwise shifts and ors. This makes the logic straightforward and the
// implementation portable, including making the tests portable. In practice
// it's also not much of an optimization hit either since the forms used here
// are (mostly) recognized by the standard modern C++ compilers (e.g. Clang will
// reduce these to a single move or a move+bswap on x86).

#ifndef ECCLESIA_LIB_CODEC_ENDIAN_H_
#define ECCLESIA_LIB_CODEC_ENDIAN_H_

#include <cstdint>

#include "absl/numeric/int128.h"

namespace ecclesia {

// Provide loads and stores for integers encoded in a little endian format. That
// means that the bytes are ordered from least to most significant.
//
// Note that all of the functions here operate on pointer buffers without a size
// type. The caller is required to ensure that the supplied data is large
// enough. The required buffer size is implied by the size the function
// operates on; it should be fairly obvious that trying to do a 32-bit load
// from a buffer that is smaller than four bytes is undefined behavior.
class LittleEndian {
 public:
  // Load integers from bytes. For an 8-bit value this is obviously a simple
  // copy. For larger sizes we split the value into two halves and delegate to
  // the next smallest function.
  static uint8_t Load8(const unsigned char *bytes) { return *bytes; }
  static uint16_t Load16(const unsigned char *bytes) {
    return uint16_t{Load8(bytes)} | uint16_t{Load8(bytes + 1)} << 8;
  }
  static uint32_t Load32(const unsigned char *bytes) {
    return uint32_t{Load16(bytes)} | uint32_t{Load16(bytes + 2)} << 16;
  }
  static uint64_t Load64(const unsigned char *bytes) {
    return uint64_t{Load32(bytes)} | uint64_t{Load32(bytes + 4)} << 32;
  }
  static absl::uint128 Load128(const unsigned char *bytes) {
    return absl::MakeUint128(Load64(bytes + 8), Load64(bytes));
  }

  // Overloads of the load functions that take a char*. Since many low-level
  // APIs use char* for accessing the object representation and not
  // unsigned char* it is useful to support both. Since the underlying bytes are
  // the same either way these functions just delegate to the unsigned char*
  // versions.
  static uint8_t Load8(const char *bytes) {
    return Load8(reinterpret_cast<const unsigned char *>(bytes));
  }
  static uint16_t Load16(const char *bytes) {
    return Load16(reinterpret_cast<const unsigned char *>(bytes));
  }
  static uint32_t Load32(const char *bytes) {
    return Load32(reinterpret_cast<const unsigned char *>(bytes));
  }
  static uint64_t Load64(const char *bytes) {
    return Load64(reinterpret_cast<const unsigned char *>(bytes));
  }
  static absl::uint128 Load128(const char *bytes) {
    return Load128(reinterpret_cast<const unsigned char *>(bytes));
  }

  // Store integers to bytes. Like with loading, we can implement the 8-bit
  // case trivially and then implement all of the larger sizes by doing two
  // stores of the next smallest size.
  static void Store8(uint8_t value, unsigned char *bytes) { *bytes = value; }
  static void Store16(uint16_t value, unsigned char *bytes) {
    Store8(value & 0xFF, bytes);
    Store8(value >> 8 & 0xFF, bytes + 1);
  }
  static void Store32(uint32_t value, unsigned char *bytes) {
    Store16(value & 0xFFFF, bytes);
    Store16(value >> 16 & 0xFFFF, bytes + 2);
  }
  static void Store64(uint64_t value, unsigned char *bytes) {
    Store32(value & 0xFFFFFFFF, bytes);
    Store32(value >> 32 & 0xFFFFFFFF, bytes + 4);
  }
  static void Store128(absl::uint128 value, unsigned char *bytes) {
    Store64(absl::Uint128Low64(value), bytes);
    Store64(absl::Uint128High64(value), bytes + 8);
  }

  // Like with the load functions, we provide char* overloads.
  static void Store8(uint8_t value, char *bytes) {
    Store8(value, reinterpret_cast<unsigned char *>(bytes));
  }
  static void Store16(uint16_t value, char *bytes) {
    Store16(value, reinterpret_cast<unsigned char *>(bytes));
  }
  static void Store32(uint32_t value, char *bytes) {
    Store32(value, reinterpret_cast<unsigned char *>(bytes));
  }
  static void Store64(uint64_t value, char *bytes) {
    Store64(value, reinterpret_cast<unsigned char *>(bytes));
  }
  static void Store128(absl::uint128 value, char *bytes) {
    Store128(value, reinterpret_cast<unsigned char *>(bytes));
  }
};

// This class works exactly the same way as LittleEndian, except that of course
// it uses a big endian format: bytes are ordered from most to least
// significant. For function comments refer to the matching corresponding
// comments in LittleEndian.
class BigEndian {
 public:
  static uint8_t Load8(const unsigned char *bytes) { return *bytes; }
  static uint16_t Load16(const unsigned char *bytes) {
    return uint16_t{Load8(bytes)} << 8 | uint16_t{Load8(bytes + 1)};
  }
  static uint32_t Load32(const unsigned char *bytes) {
    return uint32_t{Load16(bytes)} << 16 | uint32_t{Load16(bytes + 2)};
  }
  static uint64_t Load64(const unsigned char *bytes) {
    return uint64_t{Load32(bytes)} << 32 | uint64_t{Load32(bytes + 4)};
  }
  static absl::uint128 Load128(const unsigned char *bytes) {
    return absl::MakeUint128(Load64(bytes), Load64(bytes + 8));
  }

  static uint8_t Load8(const char *bytes) {
    return Load8(reinterpret_cast<const unsigned char *>(bytes));
  }
  static uint16_t Load16(const char *bytes) {
    return Load16(reinterpret_cast<const unsigned char *>(bytes));
  }
  static uint32_t Load32(const char *bytes) {
    return Load32(reinterpret_cast<const unsigned char *>(bytes));
  }
  static uint64_t Load64(const char *bytes) {
    return Load64(reinterpret_cast<const unsigned char *>(bytes));
  }
  static absl::uint128 Load128(const char *bytes) {
    return Load128(reinterpret_cast<const unsigned char *>(bytes));
  }

  static void Store8(uint8_t value, unsigned char *bytes) { *bytes = value; }
  static void Store16(uint16_t value, unsigned char *bytes) {
    Store8(value >> 8 & 0xFF, bytes);
    Store8(value & 0xFF, bytes + 1);
  }
  static void Store32(uint32_t value, unsigned char *bytes) {
    Store16(value >> 16 & 0xFFFF, bytes);
    Store16(value & 0xFFFF, bytes + 2);
  }
  static void Store64(uint64_t value, unsigned char *bytes) {
    Store32(value >> 32 & 0xFFFFFFFF, bytes);
    Store32(value & 0xFFFFFFFF, bytes + 4);
  }
  static void Store128(absl::uint128 value, unsigned char *bytes) {
    Store64(absl::Uint128High64(value), bytes);
    Store64(absl::Uint128Low64(value), bytes + 8);
  }

  static void Store8(uint8_t value, char *bytes) {
    Store8(value, reinterpret_cast<unsigned char *>(bytes));
  }
  static void Store16(uint16_t value, char *bytes) {
    Store16(value, reinterpret_cast<unsigned char *>(bytes));
  }
  static void Store32(uint32_t value, char *bytes) {
    Store32(value, reinterpret_cast<unsigned char *>(bytes));
  }
  static void Store64(uint64_t value, char *bytes) {
    Store64(value, reinterpret_cast<unsigned char *>(bytes));
  }
  static void Store128(absl::uint128 value, char *bytes) {
    Store128(value, reinterpret_cast<unsigned char *>(bytes));
  }
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_CODEC_ENDIAN_H_
