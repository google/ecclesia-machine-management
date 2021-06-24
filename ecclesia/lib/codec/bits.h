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

#include <bitset>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <type_traits>

#include "absl/base/attributes.h"
#include "absl/numeric/bits.h"

namespace ecclesia {

// Generates a mask of 'width' 1 bits (for 0 <= width <= 64).  For example:
//     Mask(0)   = 0x0
//     Mask(1)   = 0x1
//     Mask(16)  = 0xffff
//     Mask(63)  = 0x7fffffffffffffff
//     Mask(64)  = 0xffffffffffffffff
// We shift 1 by width/2 twice and do one more shift for odd number.
// The double shift handles Mask(64), which becomes unsigned(-1)
// Intel CPU masks the shift bits. Details at shortn/_gJcWwhuy5H
constexpr uint64_t Mask(int width) {
  return (((uint64_t{1} << (width >> 1)) << (width >> 1)) << (width & 0x01)) -
         1;
}

// Xor's bits of a register with each other.
constexpr uint8_t XorAllBits(uint64_t reg) {
  return absl::popcount(reg) & 0x01;
}

// Define a type to represent bit range from lo to hi inclusive.
struct BitRange {
  constexpr BitRange(uint8_t high, uint8_t low) : hi(high), lo(low) {}
  constexpr BitRange(uint8_t bit) : BitRange(bit, bit) {}

  // Constructs a BitRange by specifying `num_bits` starting from the low bit
  // at `offset`.
  static BitRange FromOffsetAndNumBits(uint8_t offset, uint8_t num_bits) {
    return BitRange(offset + num_bits - 1, offset);
  }

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

// Represents an address where some of the bits have an unknown value.
// Not for performant code. Can be used as an integral type, but some
// operations are deceptively inefficient
class MaskedAddress {
 public:
  MaskedAddress(uint64_t addr, uint64_t mask) : addr_(addr), mask_(mask) {}
  explicit MaskedAddress(uint64_t addr) : addr_(addr), mask_(~0) {}
  MaskedAddress() : addr_(0), mask_(~0) {}

  // We can consider adding arithmetic/bitwise operators to this class in the
  // future when there are use cases for them.

  // Iterate through all possible addresses that match the address and the mask.
  class iterator : public std::iterator<std::input_iterator_tag, uint64_t> {
   public:
    // default-constructed "end" iterator
    iterator() : done_(true) {}
    explicit iterator(const MaskedAddress &ma)
        : addr_(ma.first()), mask_(ma.mask_), done_(false) {}
    iterator &operator++();

    iterator operator++(int);

    bool operator==(const iterator &i) const;

    bool operator!=(const iterator &i) const { return !(*this == i); }

    uint64_t operator*() const { return addr_; }

   private:
    uint64_t addr_;
    uint64_t mask_;
    bool done_;
  };

  // Returns true if the entire set of bits represented by the MaskedAddress is
  // contiguous.
  bool Contiguous() const { return absl::has_single_bit(~mask_ + 1); }

  // The range is resolved to a single address if and only if there are no
  // uncertain bits
  bool Resolved() const { return ~mask_ == 0; }

  uint64_t first() const { return addr_ & mask_; }
  uint64_t last() const { return addr_ | ~mask_; }
  iterator begin() const { return iterator(*this); }
  iterator end() const { return iterator(); }

 private:
  friend std::ostream &operator<<(std::ostream &out, const MaskedAddress &ma);
  uint64_t addr_;
  uint64_t mask_;
};

// Represents a range of addresses.
class AddressRange {
 public:
  // End < begin is a valid condition signifying an empty range.
  // Begin and end are inclusive.
  AddressRange(uint64_t begin, uint64_t end) : begin_(begin), end_(end) {}
  // The empty range.
  AddressRange() : AddressRange(1, 0) {}

  bool operator==(const AddressRange &range) const {
    return begin_ == range.begin_ && end_ == range.end_;
  }
  bool operator!=(const AddressRange &range) const { return !(*this == range); }

  // The range is empty if end < begin.
  bool Empty() const;
  // An address is in the range if begin <= address <= end.
  bool InRange(uint64_t address) const;

  // Check if all the possible addresses within the MaskedAddress fall within
  // the AddressRange.
  bool CoversMask(MaskedAddress address) const;

  // Check if any possible address within the MaskedAddress falls within the
  // AddressRange.
  bool OverlapsMask(MaskedAddress address) const;

 private:
  friend std::ostream &operator<<(std::ostream &out, const AddressRange &addr);
  uint64_t begin_;
  uint64_t end_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_CODEC_BITS_H_
