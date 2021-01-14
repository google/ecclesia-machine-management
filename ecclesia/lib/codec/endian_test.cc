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

#include "ecclesia/lib/codec/endian.h"

#include <cstdint>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/numeric/int128.h"

namespace ecclesia {
namespace {

using ::testing::ElementsAre;

// 128 bits of sample data for loading from.
constexpr unsigned char kSampleBytes[] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, /* first 64-bits */
    0x1A, 0x2B, 0x3C, 0x4D, 0x5E, 0x6F, 0x7A, 0x8B, /* second 64-bits */
};

TEST(LittleEndianTest, Loads) {
  EXPECT_EQ(LittleEndian::Load8(kSampleBytes), UINT8_C(0x01));
  EXPECT_EQ(LittleEndian::Load16(kSampleBytes), UINT16_C(0x2301));
  EXPECT_EQ(LittleEndian::Load32(kSampleBytes), UINT32_C(0x67452301));
  EXPECT_EQ(LittleEndian::Load64(kSampleBytes), UINT64_C(0xEFCDAB8967452301));
  EXPECT_EQ(LittleEndian::Load128(kSampleBytes),
            absl::MakeUint128(UINT64_C(0x8B7A6F5E4D3C2B1A),
                              UINT64_C(0xEFCDAB8967452301)));
}

TEST(LittleEndianTest, LoadsSigned) {
  EXPECT_EQ(LittleEndian::Load8(kSampleBytes), UINT8_C(0x01));
  EXPECT_EQ(LittleEndian::Load16(kSampleBytes), UINT16_C(0x2301));
  EXPECT_EQ(LittleEndian::Load32(kSampleBytes), UINT32_C(0x67452301));
  EXPECT_EQ(LittleEndian::Load64(kSampleBytes), UINT64_C(0xEFCDAB8967452301));
  EXPECT_EQ(LittleEndian::Load128(kSampleBytes),
            absl::MakeUint128(UINT64_C(0x8B7A6F5E4D3C2B1A),
                              UINT64_C(0xEFCDAB8967452301)));
}

TEST(LittleEndianTest, Stores) {
  std::vector<unsigned char> buffer;

  buffer.clear();
  buffer.resize(1);
  LittleEndian::Store8(UINT8_C(0x01), buffer.data());
  EXPECT_THAT(buffer, ElementsAre(0x01));

  buffer.clear();
  buffer.resize(2);
  LittleEndian::Store16(UINT16_C(0x2301), buffer.data());
  EXPECT_THAT(buffer, ElementsAre(0x01, 0x23));

  buffer.clear();
  buffer.resize(4);
  LittleEndian::Store32(UINT32_C(0x67452301), buffer.data());
  EXPECT_THAT(buffer, ElementsAre(0x01, 0x23, 0x45, 0x67));

  buffer.clear();
  buffer.resize(8);
  LittleEndian::Store64(UINT64_C(0xEFCDAB8967452301), buffer.data());
  EXPECT_THAT(buffer,
              ElementsAre(0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF));

  buffer.clear();
  buffer.resize(16);
  LittleEndian::Store128(absl::MakeUint128(UINT64_C(0x8B7A6F5E4D3C2B1A),
                                           UINT64_C(0xEFCDAB8967452301)),
                         buffer.data());
  EXPECT_THAT(buffer,
              ElementsAre(0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0x1A,
                          0x2B, 0x3C, 0x4D, 0x5E, 0x6F, 0x7A, 0x8B));
}

TEST(LittleEndianTest, StoresSigned) {
  std::vector<char> buffer;

  buffer.clear();
  buffer.resize(1);
  LittleEndian::Store8(UINT8_C(0x01), buffer.data());
  EXPECT_THAT(buffer, ElementsAre(0x01));

  buffer.clear();
  buffer.resize(2);
  LittleEndian::Store16(UINT16_C(0x2301), buffer.data());
  EXPECT_THAT(buffer, ElementsAre(0x01, 0x23));

  buffer.clear();
  buffer.resize(4);
  LittleEndian::Store32(UINT32_C(0x67452301), buffer.data());
  EXPECT_THAT(buffer, ElementsAre(0x01, 0x23, 0x45, 0x67));

  buffer.clear();
  buffer.resize(8);
  LittleEndian::Store64(UINT64_C(0xEFCDAB8967452301), buffer.data());
  EXPECT_THAT(buffer,
              ElementsAre(0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF));

  buffer.clear();
  buffer.resize(16);
  LittleEndian::Store128(absl::MakeUint128(UINT64_C(0x8B7A6F5E4D3C2B1A),
                                           UINT64_C(0xEFCDAB8967452301)),
                         buffer.data());
  EXPECT_THAT(buffer,
              ElementsAre(0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0x1A,
                          0x2B, 0x3C, 0x4D, 0x5E, 0x6F, 0x7A, 0x8B));
}

TEST(BigEndianTest, Loads) {
  EXPECT_EQ(BigEndian::Load8(kSampleBytes), UINT8_C(0x01));
  EXPECT_EQ(BigEndian::Load16(kSampleBytes), UINT16_C(0x0123));
  EXPECT_EQ(BigEndian::Load32(kSampleBytes), UINT32_C(0x01234567));
  EXPECT_EQ(BigEndian::Load64(kSampleBytes), UINT64_C(0x0123456789ABCDEF));
  EXPECT_EQ(BigEndian::Load128(kSampleBytes),
            absl::MakeUint128(UINT64_C(0x0123456789ABCDEF),
                              UINT64_C(0x1A2B3C4D5E6F7A8B)));
}

TEST(BigEndianTest, LoadsSigned) {
  EXPECT_EQ(BigEndian::Load8(kSampleBytes), UINT8_C(0x01));
  EXPECT_EQ(BigEndian::Load16(kSampleBytes), UINT16_C(0x0123));
  EXPECT_EQ(BigEndian::Load32(kSampleBytes), UINT32_C(0x01234567));
  EXPECT_EQ(BigEndian::Load64(kSampleBytes), UINT64_C(0x0123456789ABCDEF));
  EXPECT_EQ(BigEndian::Load128(kSampleBytes),
            absl::MakeUint128(UINT64_C(0x0123456789ABCDEF),
                              UINT64_C(0x1A2B3C4D5E6F7A8B)));
}

TEST(BigEndianTest, Stores) {
  std::vector<unsigned char> buffer;

  buffer.clear();
  buffer.resize(1);
  BigEndian::Store8(UINT8_C(0x01), buffer.data());
  EXPECT_THAT(buffer, ElementsAre(0x01));

  buffer.clear();
  buffer.resize(2);
  BigEndian::Store16(UINT16_C(0x0123), buffer.data());
  EXPECT_THAT(buffer, ElementsAre(0x01, 0x23));

  buffer.clear();
  buffer.resize(4);
  BigEndian::Store32(UINT32_C(0x01234567), buffer.data());
  EXPECT_THAT(buffer, ElementsAre(0x01, 0x23, 0x45, 0x67));

  buffer.clear();
  buffer.resize(8);
  BigEndian::Store64(UINT64_C(0x0123456789ABCDEF), buffer.data());
  EXPECT_THAT(buffer,
              ElementsAre(0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF));

  buffer.clear();
  buffer.resize(16);
  BigEndian::Store128(absl::MakeUint128(UINT64_C(0x0123456789ABCDEF),
                                        UINT64_C(0x1A2B3C4D5E6F7A8B)),
                      buffer.data());
  EXPECT_THAT(buffer,
              ElementsAre(0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0x1A,
                          0x2B, 0x3C, 0x4D, 0x5E, 0x6F, 0x7A, 0x8B));
}

TEST(BigEndianTest, StoresSigned) {
  std::vector<unsigned char> buffer;

  buffer.clear();
  buffer.resize(1);
  BigEndian::Store8(UINT8_C(0x01), buffer.data());
  EXPECT_THAT(buffer, ElementsAre(0x01));

  buffer.clear();
  buffer.resize(2);
  BigEndian::Store16(UINT16_C(0x0123), buffer.data());
  EXPECT_THAT(buffer, ElementsAre(0x01, 0x23));

  buffer.clear();
  buffer.resize(4);
  BigEndian::Store32(UINT32_C(0x01234567), buffer.data());
  EXPECT_THAT(buffer, ElementsAre(0x01, 0x23, 0x45, 0x67));

  buffer.clear();
  buffer.resize(8);
  BigEndian::Store64(UINT64_C(0x0123456789ABCDEF), buffer.data());
  EXPECT_THAT(buffer,
              ElementsAre(0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF));

  buffer.clear();
  buffer.resize(16);
  BigEndian::Store128(absl::MakeUint128(UINT64_C(0x0123456789ABCDEF),
                                        UINT64_C(0x1A2B3C4D5E6F7A8B)),
                      buffer.data());
  EXPECT_THAT(buffer,
              ElementsAre(0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0x1A,
                          0x2B, 0x3C, 0x4D, 0x5E, 0x6F, 0x7A, 0x8B));
}

}  // namespace
}  // namespace ecclesia
