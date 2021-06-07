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

#include "ecclesia/lib/codec/bits.h"

#include <cstdint>
#include <limits>

#include "gtest/gtest.h"

namespace ecclesia {
namespace {

TEST(BitsTest, MaskWorks) {
  EXPECT_EQ(0x0, Mask(0));
  EXPECT_EQ(0x1, Mask(1));
  EXPECT_EQ(0x3, Mask(2));
  EXPECT_EQ(0x7, Mask(3));
  EXPECT_EQ(0xf, Mask(4));
  EXPECT_EQ(0xff, Mask(8));
  EXPECT_EQ(0xffff, Mask(16));
  EXPECT_EQ(0xffffffff, Mask(32));
  EXPECT_EQ(uint64_t{0xffffffffffffffffu}, Mask(64));
}

TEST(BitsTest, XorAllBitsWorks) {
  EXPECT_EQ(0x0, XorAllBits(0));
  EXPECT_EQ(0x1, XorAllBits(uint64_t{0x00100}));
  EXPECT_EQ(0x1, XorAllBits(uint64_t{0x8000000000000000u}));
  EXPECT_EQ(0x0, XorAllBits(uint64_t{0x8000000000100000u}));
  EXPECT_EQ(0x0, XorAllBits(uint64_t{0xffffffffffffffffu}));
  EXPECT_EQ(0x1, XorAllBits(uint64_t{0x842100a000100000u}));
}

TEST(BitsTest, ExtractBitsUnsignedWorks) {
  uint8_t value_8 = 0x12;
  EXPECT_EQ(0x1, ExtractBits(value_8, BitRange(7, 4)));
  EXPECT_EQ(0x0, ExtractBits(value_8, BitRange(9)));

  uint16_t value_16 = 0x1234;
  EXPECT_EQ(0x1, ExtractBits(value_16, BitRange(15, 12)));
  EXPECT_EQ(0x3, ExtractBits(value_16, BitRange(7, 4)));
  EXPECT_EQ(0x0, ExtractBits(value_16, BitRange(17)));

  uint32_t value_32 = 0x12345678;
  EXPECT_EQ(0x1, ExtractBits(value_32, BitRange(31, 28)));
  EXPECT_EQ(0x2, ExtractBits(value_32, BitRange(27, 24)));
  EXPECT_EQ(0x3, ExtractBits(value_32, BitRange(23, 20)));
  EXPECT_EQ(0x4, ExtractBits(value_32, BitRange(19, 16)));
  EXPECT_EQ(0x5, ExtractBits(value_32, BitRange(15, 12)));
  EXPECT_EQ(0x6, ExtractBits(value_32, BitRange(11, 8)));
  EXPECT_EQ(0x7, ExtractBits(value_32, BitRange(7, 4)));
  EXPECT_EQ(0x8, ExtractBits(value_32, BitRange(3, 0)));
  EXPECT_EQ(0x0, ExtractBits(value_32, BitRange(33)));

  uint64_t value_64 = 0x123456789abcdef0ULL;
  EXPECT_EQ(0x1, ExtractBits(value_64, BitRange(63, 60)));
  EXPECT_EQ(0x2, ExtractBits(value_64, BitRange(59, 56)));
  EXPECT_EQ(0x3, ExtractBits(value_64, BitRange(55, 52)));
  EXPECT_EQ(0x4, ExtractBits(value_64, BitRange(51, 48)));
  EXPECT_EQ(0x5, ExtractBits(value_64, BitRange(47, 44)));
  EXPECT_EQ(0x6, ExtractBits(value_64, BitRange(43, 40)));
  EXPECT_EQ(0x7, ExtractBits(value_64, BitRange(39, 36)));
  EXPECT_EQ(0x8, ExtractBits(value_64, BitRange(35, 32)));
  EXPECT_EQ(0x9, ExtractBits(value_64, BitRange(31, 28)));
  EXPECT_EQ(0xa, ExtractBits(value_64, BitRange(27, 24)));
  EXPECT_EQ(0xb, ExtractBits(value_64, BitRange(23, 20)));
  EXPECT_EQ(0xc, ExtractBits(value_64, BitRange(19, 16)));
  EXPECT_EQ(0xd, ExtractBits(value_64, BitRange(15, 12)));
  EXPECT_EQ(0xe, ExtractBits(value_64, BitRange(11, 8)));
  EXPECT_EQ(0xf, ExtractBits(value_64, BitRange(7, 4)));
  EXPECT_EQ(0x0, ExtractBits(value_64, BitRange(3, 0)));
  EXPECT_EQ(0x0, ExtractBits(value_64, BitRange(65)));
}

TEST(BitsTest, ExtractBitsSignedWorks) {
  int8_t value_8 = 0x12;
  EXPECT_EQ(0x1, ExtractBits(value_8, BitRange(7, 4)));
  EXPECT_EQ(0x0, ExtractBits(value_8, BitRange(8)));
  value_8 = -1;
  EXPECT_EQ(0xf, ExtractBits(value_8, BitRange(7, 4)));
  EXPECT_EQ(0x0, ExtractBits(value_8, BitRange(8)));

  int16_t value_16 = 0x1234;
  EXPECT_EQ(0x1, ExtractBits(value_16, BitRange(15, 12)));
  EXPECT_EQ(0x3, ExtractBits(value_16, BitRange(7, 4)));
  EXPECT_EQ(0x0, ExtractBits(value_16, BitRange(16)));
  value_16 = -1;
  EXPECT_EQ(0xf, ExtractBits(value_16, BitRange(7, 4)));
  EXPECT_EQ(0x0, ExtractBits(value_16, BitRange(16)));

  int32_t value_32 = 0x12345678;
  EXPECT_EQ(0x1, ExtractBits(value_32, BitRange(31, 28)));
  EXPECT_EQ(0x2, ExtractBits(value_32, BitRange(27, 24)));
  EXPECT_EQ(0x3, ExtractBits(value_32, BitRange(23, 20)));
  EXPECT_EQ(0x4, ExtractBits(value_32, BitRange(19, 16)));
  EXPECT_EQ(0x5, ExtractBits(value_32, BitRange(15, 12)));
  EXPECT_EQ(0x6, ExtractBits(value_32, BitRange(11, 8)));
  EXPECT_EQ(0x7, ExtractBits(value_32, BitRange(7, 4)));
  EXPECT_EQ(0x8, ExtractBits(value_32, BitRange(3, 0)));
  EXPECT_EQ(0x0, ExtractBits(value_32, BitRange(32)));
  value_32 = -1;
  EXPECT_EQ(0xf, ExtractBits(value_32, BitRange(7, 4)));
  EXPECT_EQ(0x0, ExtractBits(value_32, BitRange(32)));

  int64_t value_64 = 0x123456789abcdef0LL;
  EXPECT_EQ(0x1, ExtractBits(value_64, BitRange(63, 60)));
  EXPECT_EQ(0x2, ExtractBits(value_64, BitRange(59, 56)));
  EXPECT_EQ(0x3, ExtractBits(value_64, BitRange(55, 52)));
  EXPECT_EQ(0x4, ExtractBits(value_64, BitRange(51, 48)));
  EXPECT_EQ(0x5, ExtractBits(value_64, BitRange(47, 44)));
  EXPECT_EQ(0x6, ExtractBits(value_64, BitRange(43, 40)));
  EXPECT_EQ(0x7, ExtractBits(value_64, BitRange(39, 36)));
  EXPECT_EQ(0x8, ExtractBits(value_64, BitRange(35, 32)));
  EXPECT_EQ(0x9, ExtractBits(value_64, BitRange(31, 28)));
  EXPECT_EQ(0xa, ExtractBits(value_64, BitRange(27, 24)));
  EXPECT_EQ(0xb, ExtractBits(value_64, BitRange(23, 20)));
  EXPECT_EQ(0xc, ExtractBits(value_64, BitRange(19, 16)));
  EXPECT_EQ(0xd, ExtractBits(value_64, BitRange(15, 12)));
  EXPECT_EQ(0xe, ExtractBits(value_64, BitRange(11, 8)));
  EXPECT_EQ(0xf, ExtractBits(value_64, BitRange(7, 4)));
  EXPECT_EQ(0x0, ExtractBits(value_64, BitRange(3, 0)));
  EXPECT_EQ(0x0, ExtractBits(value_64, BitRange(65)));
  value_64 = -1;
  EXPECT_EQ(0xf, ExtractBits(value_64, BitRange(7, 4)));
  EXPECT_EQ(value_64, ExtractBits(value_64, BitRange(100, 0)));
  EXPECT_EQ(0x0, ExtractBits(value_64, BitRange(64)));
  EXPECT_EQ(0x0, ExtractBits(value_64, BitRange(0, 1)));
}

TEST(MaskedAddressTest, Iterator) {
  constexpr int kMaxErrors = 64;
  MaskedAddress addr(0xdeadbeef, ~0x00000fff);

  int num_errors = 0;
  uint64_t expected = 0xdeadb000;
  for (MaskedAddress::iterator iter = addr.begin();
       iter != addr.end() && num_errors < kMaxErrors; ++iter, ++expected) {
    EXPECT_EQ(*iter, expected) << expected;
    if (*iter != expected) {
      ++num_errors;
    }
  }
  if (num_errors < kMaxErrors) {
    EXPECT_EQ(expected, 0xdeadc000);
  }
}

TEST(MaskedAddressTest, Contiguous) {
  EXPECT_TRUE(MaskedAddress(0xdeadbeef, ~0x00000fff).Contiguous());
  EXPECT_FALSE(MaskedAddress(0xdeadbeef, ~0x00000aff).Contiguous());
  EXPECT_TRUE(MaskedAddress(0xdeadbeef, ~0x000007ff).Contiguous());
  EXPECT_TRUE(MaskedAddress(0xdeadbeef, ~0x000000ff).Contiguous());
  EXPECT_FALSE(MaskedAddress(0xdeadbeef, ~0x000000ef).Contiguous());
  EXPECT_FALSE(MaskedAddress(0xdeadbeef, ~0x0000002f).Contiguous());
  EXPECT_TRUE(MaskedAddress(0xdeadbeef, ~0x0000001f).Contiguous());
  EXPECT_FALSE(MaskedAddress(0xdeadbeef, ~0x0000000e).Contiguous());
  EXPECT_FALSE(MaskedAddress(0xdeadbeef, ~0x0000000c).Contiguous());
  EXPECT_FALSE(MaskedAddress(0xdeadbeef, ~0x00000009).Contiguous());
  EXPECT_FALSE(MaskedAddress(0xdeadbeef, ~0x00000008).Contiguous());
  EXPECT_TRUE(MaskedAddress(0xdeadbeef, ~0x00000007).Contiguous());
  EXPECT_FALSE(MaskedAddress(0xdeadbeef, ~0x00000006).Contiguous());
  EXPECT_FALSE(MaskedAddress(0xdeadbeef, ~0x00000005).Contiguous());
  EXPECT_FALSE(MaskedAddress(0xdeadbeef, ~0x00000004).Contiguous());
  EXPECT_TRUE(MaskedAddress(0xdeadbeef, ~0x00000003).Contiguous());
  EXPECT_FALSE(MaskedAddress(0xdeadbeef, ~0x00000002).Contiguous());
  EXPECT_TRUE(MaskedAddress(0xdeadbeef, ~0x00000001).Contiguous());
  EXPECT_TRUE(MaskedAddress(0xdeadbeef, ~0x00000000).Contiguous());
}

TEST(AddressRangeTest, Empty) {
  AddressRange range(0xdeadbe00, 0xdeadbeef);
  EXPECT_FALSE(range.InRange(0x0));
  EXPECT_FALSE(range.InRange(0xf00));
  EXPECT_FALSE(range.InRange(0xdead0000));
  EXPECT_FALSE(range.InRange(0xdeadbdff));
  EXPECT_TRUE(range.InRange(0xdeadbe00));
  EXPECT_TRUE(range.InRange(0xdeadbe80));
  EXPECT_TRUE(range.InRange(0xdeadbec0));
  EXPECT_TRUE(range.InRange(0xdeadbee0));
  EXPECT_TRUE(range.InRange(0xdeadbeef));
  EXPECT_FALSE(range.InRange(0xdeadbef0));
  EXPECT_FALSE(range.InRange(0xdeadbefa));
  EXPECT_FALSE(range.InRange(0xdeadbf00));
  EXPECT_FALSE(range.InRange(0xffffffff));
  EXPECT_FALSE(range.InRange(std::numeric_limits<uint64_t>::max()));
}

TEST(AddressRangeTest, CoversMask) {
  AddressRange range(0xdeadbe00, 0xdeadbeef);
  EXPECT_FALSE(range.CoversMask(MaskedAddress(0x0)));
  EXPECT_FALSE(range.CoversMask(MaskedAddress(0x0, ~0xffff)));
  EXPECT_FALSE(range.CoversMask(MaskedAddress(0x0, ~0xffffffffUL)));
  EXPECT_FALSE(range.CoversMask(MaskedAddress(0xdead0000)));
  EXPECT_FALSE(range.CoversMask(MaskedAddress(0xdead0000, ~0xffff)));
  EXPECT_FALSE(range.CoversMask(MaskedAddress(0xdead0000, ~0xff)));
  EXPECT_FALSE(range.CoversMask(MaskedAddress(0xdeadbe00, ~0xff)));
  EXPECT_TRUE(range.CoversMask(MaskedAddress(0xdeadbe00)));
  EXPECT_TRUE(range.CoversMask(MaskedAddress(0xdeadbe00, ~0x7f)));
  EXPECT_TRUE(range.CoversMask(MaskedAddress(0xdeadbea0)));
  EXPECT_TRUE(range.CoversMask(MaskedAddress(0xdeadbe00, ~0xf)));
  EXPECT_TRUE(range.CoversMask(MaskedAddress(0xdeadbeef)));
  EXPECT_TRUE(range.CoversMask(MaskedAddress(0xdeadbee0, ~0xf)));
  EXPECT_FALSE(range.CoversMask(MaskedAddress(0xdeadbea0, ~0x7f)));
  EXPECT_FALSE(range.CoversMask(MaskedAddress(0xdeadbee0, ~0xff)));
  EXPECT_FALSE(range.CoversMask(MaskedAddress(0xffffffff, ~0xffffff)));
  EXPECT_FALSE(range.CoversMask(MaskedAddress(0xffffffff, ~0xffffffffUL)));
}

TEST(AddressRangeTest, OverlapsMask) {
  AddressRange range(0xdeadbe00, 0xdeadbeef);
  EXPECT_FALSE(range.OverlapsMask(MaskedAddress(0x0)));
  EXPECT_FALSE(range.OverlapsMask(MaskedAddress(0x0, ~0x0)));
  EXPECT_FALSE(range.OverlapsMask(MaskedAddress(0x0, ~0xffff)));
  EXPECT_TRUE(range.OverlapsMask(MaskedAddress(0x0, ~0xffffffffUL)));
  EXPECT_FALSE(range.OverlapsMask(MaskedAddress(0xdead0000)));
  EXPECT_TRUE(range.OverlapsMask(MaskedAddress(0xdead0000, ~0xffff)));
  EXPECT_FALSE(range.OverlapsMask(MaskedAddress(0xdead0000, ~0xff)));
  EXPECT_TRUE(range.OverlapsMask(MaskedAddress(0xdeadbe00)));
  EXPECT_TRUE(range.OverlapsMask(MaskedAddress(0xdeadbe00, ~0xf)));
  EXPECT_TRUE(range.OverlapsMask(MaskedAddress(0xdeadbe00, ~0x7f)));
  EXPECT_TRUE(range.OverlapsMask(MaskedAddress(0xdeadbe00, ~0xff)));
  EXPECT_TRUE(range.OverlapsMask(MaskedAddress(0xdeadbe01)));
  EXPECT_TRUE(range.OverlapsMask(MaskedAddress(0xdeadbea0)));
  EXPECT_TRUE(range.OverlapsMask(MaskedAddress(0xdeadbeef)));
  EXPECT_TRUE(range.OverlapsMask(MaskedAddress(0xdeadbee0, ~0xf)));
  EXPECT_TRUE(range.OverlapsMask(MaskedAddress(0xdeadbea0, ~0x7f)));
  EXPECT_TRUE(range.OverlapsMask(MaskedAddress(0xdeadbee0, ~0xff)));
  EXPECT_FALSE(range.OverlapsMask(MaskedAddress(0xffffffff, ~0xffffff)));
  EXPECT_TRUE(range.OverlapsMask(MaskedAddress(0xffffffff, ~0xffffffffUL)));
}

}  // namespace
}  // namespace ecclesia
