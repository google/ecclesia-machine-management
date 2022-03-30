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

#include "ecclesia/magent/lib/eeprom/ipmi_eeprom.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/types/span.h"
#include "ecclesia/magent/lib/eeprom/eeprom.h"
#include "ecclesia/magent/lib/ipmi/ipmi_mock.h"

namespace ecclesia {
namespace {

using ::testing::_;
using ::testing::ContainerEq;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::Unused;

const Eeprom::ModeType kReadMode = {true, false};

TEST(IpmiEeprom, FailedToReadSize) {
  MockIpmiInterface ipmi;
  EXPECT_CALL(ipmi, GetFruSize)
      .WillOnce(Return(absl::UnknownError("test error")));

  IpmiEeprom eeprom(&ipmi, kReadMode, 3);

  EXPECT_TRUE(eeprom.GetMode().readable);
  EXPECT_FALSE(eeprom.GetMode().writable);
  EXPECT_THAT(eeprom.GetSize().size, Eq(0));
}

TEST(IpmiEeprom, WriteUnimplemented) {
  MockIpmiInterface ipmi;
  Eeprom::ModeType read_write_mode = {true, true};
  IpmiEeprom eeprom(&ipmi, read_write_mode, 3);

  std::vector<unsigned char> data;
  EXPECT_TRUE(eeprom.GetMode().readable);
  EXPECT_TRUE(eeprom.GetMode().writable);
  EXPECT_THAT(eeprom.WriteBytes(0, absl::MakeSpan(data)), Eq(std::nullopt));
}

TEST(IpmiEeprom, FailedToReadData) {
  MockIpmiInterface ipmi;
  EXPECT_CALL(ipmi, GetFruSize)
      .WillOnce(DoAll(SetArgPointee<1>(4), Return(absl::OkStatus())));
  EXPECT_CALL(ipmi, ReadFru).WillOnce(Return(absl::UnknownError("test error")));

  IpmiEeprom eeprom(&ipmi, kReadMode, 3);

  EXPECT_THAT(eeprom.GetSize().size, Eq(0));
}

TEST(IpmiEeprom, EmptyData) {
  MockIpmiInterface ipmi;
  EXPECT_CALL(ipmi, GetFruSize)
      .WillOnce(DoAll(SetArgPointee<1>(0), Return(absl::OkStatus())));

  IpmiEeprom eeprom(&ipmi, kReadMode, 3);

  std::vector<unsigned char> data;
  EXPECT_THAT(eeprom.GetSize().size, Eq(0));
  EXPECT_THAT(eeprom.ReadBytes(/*offset*/ 0, absl::MakeSpan(data)),
              Eq(std::nullopt));
}

TEST(IpmiEeprom, ReadSuccess) {
  MockIpmiInterface ipmi;
  std::array<uint8_t, 4> mock_data = {4, 3, 2, 1};

  EXPECT_CALL(ipmi, GetFruSize)
      .WillOnce(
          DoAll(SetArgPointee<1>(sizeof(mock_data)), Return(absl::OkStatus())));
  EXPECT_CALL(ipmi, ReadFru(_, 0, _))
      .WillOnce(
          [&mock_data](Unused, size_t offset, absl::Span<unsigned char> data) {
            std::copy(mock_data.begin(), mock_data.end(), data.begin());
            return absl::OkStatus();
          });

  IpmiEeprom eeprom(&ipmi, kReadMode, 3);

  std::array<uint8_t, 4> data = {0};
  EXPECT_THAT(eeprom.GetSize().size, Eq(4));
  EXPECT_THAT(eeprom.ReadBytes(/*offset*/ 0, absl::MakeSpan(data)), Eq(4));
  EXPECT_THAT(data, ContainerEq(mock_data));
}

TEST(IpmiEeprom, OffsetReadSuccess) {
  MockIpmiInterface ipmi;

  EXPECT_CALL(ipmi, GetFruSize)
      .WillOnce(DoAll(SetArgPointee<1>(5), Return(absl::OkStatus())));
  EXPECT_CALL(ipmi, ReadFru(_, 0, _))
      .WillOnce([](Unused, size_t offset, absl::Span<unsigned char> data) {
        constexpr std::array<uint8_t, 5> mock_data = {5, 4, 3, 2, 1};
        std::copy(mock_data.begin(), mock_data.end(), data.begin());
        return absl::OkStatus();
      });

  IpmiEeprom eeprom(&ipmi, kReadMode, 3);

  std::array<uint8_t, 3> data = {0};
  EXPECT_THAT(eeprom.GetSize().size, Eq(5));
  EXPECT_THAT(eeprom.ReadBytes(/*offset*/ 2, absl::MakeSpan(data)), Eq(3));
  EXPECT_THAT(data, testing::ElementsAre(3, 2, 1));
}

TEST(IpmiEeprom, BadOffset) {
  MockIpmiInterface ipmi;

  EXPECT_CALL(ipmi, GetFruSize)
      .WillOnce(DoAll(SetArgPointee<1>(4), Return(absl::OkStatus())));
  EXPECT_CALL(ipmi, ReadFru(_, 0, _))
      .WillOnce([](Unused, size_t offset, absl::Span<unsigned char> data) {
        constexpr std::array<uint8_t, 4> mock_data = {4, 3, 2, 1};
        std::copy(mock_data.begin(), mock_data.end(), data.begin());
        return absl::OkStatus();
      });

  IpmiEeprom eeprom(&ipmi, kReadMode, 3);

  std::array<uint8_t, 4> data = {0};
  EXPECT_THAT(eeprom.GetSize().size, Eq(4));
  EXPECT_THAT(eeprom.ReadBytes(/*offset*/ 4, absl::MakeSpan(data)),
              Eq(std::nullopt));
}

TEST(IpmiEeprom, BadLen) {
  MockIpmiInterface ipmi;

  EXPECT_CALL(ipmi, GetFruSize)
      .WillOnce(DoAll(SetArgPointee<1>(4), Return(absl::OkStatus())));
  EXPECT_CALL(ipmi, ReadFru(_, 0, _))
      .WillOnce([](Unused, size_t offset, absl::Span<unsigned char> data) {
        constexpr std::array<uint8_t, 4> mock_data = {4, 3, 2, 1};
        std::copy(mock_data.begin(), mock_data.end(), data.begin());
        return absl::OkStatus();
      });

  IpmiEeprom eeprom(&ipmi, kReadMode, 3);

  std::array<uint8_t, 5> data = {0};
  EXPECT_THAT(eeprom.GetSize().size, Eq(4));
  EXPECT_THAT(eeprom.ReadBytes(/*offset*/ 0, absl::MakeSpan(data)),
              Eq(std::nullopt));
}

TEST(IpmiEeprom, OffsetPlusLenOverflows) {
  MockIpmiInterface ipmi;

  EXPECT_CALL(ipmi, GetFruSize)
      .WillOnce(DoAll(SetArgPointee<1>(4), Return(absl::OkStatus())));
  EXPECT_CALL(ipmi, ReadFru(_, 0, _))
      .WillOnce([](Unused, size_t offset, absl::Span<unsigned char> data) {
        constexpr std::array<uint8_t, 4> mock_data = {4, 3, 2, 1};
        std::copy(mock_data.begin(), mock_data.end(), data.begin());
        return absl::OkStatus();
      });

  IpmiEeprom eeprom(&ipmi, kReadMode, 3);

  std::array<uint8_t, 4> data = {0};
  EXPECT_THAT(eeprom.GetSize().size, Eq(4));
  EXPECT_THAT(eeprom.ReadBytes(/*offset*/ 1, absl::MakeSpan(data)),
              Eq(std::nullopt));
}

}  // namespace
}  // namespace ecclesia
