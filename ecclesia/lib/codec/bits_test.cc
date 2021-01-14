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

}  // namespace
}  // namespace ecclesia
