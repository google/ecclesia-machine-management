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

// Some templated helper routines to support ExtractBits from bitRange.

#ifndef ECCLESIA_LIB_CODEC_BITS_H_
#define ECCLESIA_LIB_CODEC_BITS_H_

#include <climits>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "absl/base/attributes.h"

namespace ecclesia {

// Generates a mask of 'width' 1 bits (for 0 <= width <= 64).  For example:
//     Mask(0)   = 0x0
//     Mask(1)   = 0x1
//     Mask(16)  = 0xffff
//     Mask(63)  = 0x7fffffffffffffff
//     Mask(64)  = 0xffffffffffffffff
inline uint64_t Mask(int width) {
  return (((uint64_t{1} << (width / 2)) << (width / 2)) << (width % 2)) - 1;
}

// Xor's bits of a register with each other.
inline uint8_t XorAllBits(uint64_t reg) {
  uint64_t tmp = 0;
  for (size_t b = 0; b < 8 * sizeof(reg); b++) {
    tmp ^= (reg >> b) & 0x1;
  }
  return tmp;
}

// Define a type to represent bit range from lo to hi inclusive.
struct BitRange {
  constexpr BitRange(uint8_t high, uint8_t low) : hi(high), lo(low) {}
  constexpr BitRange(uint8_t bit) : BitRange(bit, bit) {}

  uint8_t hi;
  uint8_t lo;
};

// Extracts a range of bits from a value and right-justifies them.  For
// example:
//     ExtractBits(0x12345678, BitRange(15, 8))  = 0x56
//     ExtractBits(0x12345678, BitRange(8, 15))  = 0x0
template <typename T>
ABSL_MUST_USE_RESULT T ExtractBits(const T &value, const BitRange &bits) {
  // Prevent right-shift of signed numbers.
  static_assert(std::is_integral<T>::value, "Integral required.");
  using unsignedT = std::make_unsigned_t<T>;
  unsignedT unsigned_value = value;

  // An Invalid BitRange implies extract 0 bit.
  if (bits.hi < bits.lo) {
    return 0;
  }

  // Prevent shift by more than bits in value, to prevent undefined behavior
  if (bits.lo < sizeof(T) * CHAR_BIT) {
    unsigned_value >>= bits.lo;
  } else {
    unsigned_value = 0;
  }

  unsignedT mask(1);

  // Prevent shift by more than bits in value
  if (bits.hi - bits.lo + 1 < sizeof(T) * CHAR_BIT) {
    mask <<= bits.hi - bits.lo + 1;
  } else {
    mask = 0;
  }

  mask -= 1;

  return unsigned_value & mask;
}

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_CODEC_BITS_H_
