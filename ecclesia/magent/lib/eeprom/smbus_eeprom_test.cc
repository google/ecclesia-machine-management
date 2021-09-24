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

#include "ecclesia/magent/lib/eeprom/smbus_eeprom.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <type_traits>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/types/span.h"
#include "ecclesia/lib/io/smbus/mocks.h"
#include "ecclesia/lib/io/smbus/smbus.h"
#include "ecclesia/magent/lib/eeprom/eeprom.h"

namespace ecclesia {
namespace {

using ::testing::_;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrictMock;

TEST(SmbusEepromDeviceTest, Methods) {
  StrictMock<MockSmbusAccessInterface> access;
  auto loc = SmbusLocation::Make<37, 0x55>();

  EXPECT_CALL(access, SendByte(_, _)).WillOnce(Return(absl::OkStatus()));

  EXPECT_CALL(access, ReceiveByte(_, _)).WillOnce(Return(absl::OkStatus()));

  EXPECT_CALL(access, ReadBlockI2C(_, _, _, _))
      .WillOnce(DoAll(SetArgPointee<3>(4), Return(absl::OkStatus())));

  EXPECT_CALL(access, WriteBlockI2C(_, _, _))
      .WillOnce(Return(absl::OkStatus()));

  EXPECT_CALL(access, Read8(_, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(1), Return(absl::OkStatus())));

  EXPECT_CALL(access, Read16(_, _, _)).WillRepeatedly(Return(absl::OkStatus()));

  EXPECT_CALL(access, Write8(_, _, _)).WillRepeatedly(Return(absl::OkStatus()));

  EXPECT_CALL(access, Write16(_, _, _))
      .WillRepeatedly(Return(absl::OkStatus()));

  // Create test target
  Eeprom::SizeType eeprom_size = {.type = ecclesia::Eeprom::SizeType::kFixed,
                                  .size = 8 * 1024};
  Eeprom::ModeType eeprom_mode = {.readable = 1, .writable = 1};

  ecclesia::SmbusEeprom2ByteAddr::Option motherboard_eeprom_option{
      .name = "motherboard",
      .size = eeprom_size,
      .mode = eeprom_mode,
      .get_device = [&loc, &access]() { return SmbusDevice(loc, &access); }};

  SmbusEeprom2ByteAddr eeprom(motherboard_eeprom_option);

  auto sz = eeprom.GetSize();
  ASSERT_EQ(eeprom_size.size, sz.size);
  ASSERT_EQ(eeprom_size.type, sz.type);

  auto md = eeprom.GetMode();
  ASSERT_EQ(eeprom_mode.readable, md.readable);
  ASSERT_EQ(eeprom_mode.writable, md.writable);

  size_t len;
  std::vector<unsigned char> data1(6);

  std::optional<SmbusDevice> smbus_device =
      motherboard_eeprom_option.get_device();
  ASSERT_TRUE(smbus_device.has_value());

  EXPECT_TRUE(
      smbus_device
          ->ReadBlockI2C(0, absl::Span<unsigned char>(data1.data(), 4), &len)
          .ok());
  EXPECT_EQ(len, 4);

  std::vector<unsigned char> data2(6);
  EXPECT_TRUE(
      smbus_device
          ->WriteBlockI2C(0, absl::Span<const unsigned char>(data2.data(), 6))
          .ok());

  uint8_t data8;
  EXPECT_TRUE(smbus_device->SendByte(6).ok());
  EXPECT_TRUE(smbus_device->ReceiveByte(&data8).ok());
  EXPECT_TRUE(smbus_device->Read8(0, &data8).ok());

  uint16_t data16;
  EXPECT_TRUE(smbus_device->Read16(0, &data16).ok());

  EXPECT_TRUE(smbus_device->Write8(0, 6).ok());
  EXPECT_TRUE(smbus_device->Write16(0, 16).ok());
}

constexpr SmbusLocation kSmbusLocation = SmbusLocation::Make<37, 0x55>();
constexpr Eeprom::SizeType kEepromSize{
    .type = ecclesia::Eeprom::SizeType::kFixed, .size = 8 * 1024};
constexpr Eeprom::ModeType kEepromMode{.readable = 1, .writable = 1};

class SmbusEeprom2ByteAddrTest : public ::testing::Test {
 public:
  SmbusEeprom2ByteAddrTest()
      : motherboard_eeprom_option_(
            {.name = "motherboard",
             .size = kEepromSize,
             .mode = kEepromMode,
             .get_device =
                 [&]() { return SmbusDevice(kSmbusLocation, &access_); }}),
        eeprom_(motherboard_eeprom_option_) {}

 protected:
  StrictMock<MockSmbusAccessInterface> access_;
  SmbusEeprom2ByteAddr::Option motherboard_eeprom_option_;
  SmbusEeprom2ByteAddr eeprom_;
};

TEST_F(SmbusEeprom2ByteAddrTest, DeviceRead8Success) {
  uint8_t expected = 0xbe;
  EXPECT_CALL(access_, Read8(_, _, _)).WillOnce(SmbusRead8(&expected));

  std::optional<SmbusDevice> device = motherboard_eeprom_option_.get_device();
  ASSERT_TRUE(device.has_value());

  uint8_t data;
  EXPECT_TRUE(device->Read8(0, &data).ok());
  EXPECT_EQ(data, expected);
}

TEST_F(SmbusEeprom2ByteAddrTest, DeviceRead16Success) {
  uint16_t expected = 0xbeef;
  EXPECT_CALL(access_, Read16(_, _, _)).WillOnce(SmbusRead16(&expected));

  std::optional<SmbusDevice> device = motherboard_eeprom_option_.get_device();
  ASSERT_TRUE(device.has_value());

  uint16_t data;
  EXPECT_TRUE(device->Read16(0, &data).ok());
  EXPECT_EQ(data, expected);
}

TEST_F(SmbusEeprom2ByteAddrTest, ReadBlockI2cSuccess) {
  std::vector<unsigned char> expected{1, 2, 3, 4, 5, 6, 7, 8};
  EXPECT_CALL(access_, ReadBlockI2C(_, _, _, _))
      .WillOnce(SmbusReadBlock(expected.data(), expected.size()));

  std::optional<SmbusDevice> device = motherboard_eeprom_option_.get_device();
  ASSERT_TRUE(device.has_value());

  size_t len;
  std::vector<unsigned char> data(expected.size());
  EXPECT_TRUE(device->ReadBlockI2C(0, absl::MakeSpan(data), &len).ok());
  EXPECT_EQ(len, expected.size());
  EXPECT_EQ(data, expected);
}

TEST_F(SmbusEeprom2ByteAddrTest, DeviceReceiveByteSuccess) {
  uint8_t expected = 0xbe;
  EXPECT_CALL(access_, ReceiveByte(_, _)).WillOnce(SmbusReceiveByte(&expected));

  std::optional<SmbusDevice> device = motherboard_eeprom_option_.get_device();
  ASSERT_TRUE(device.has_value());

  uint8_t data;
  EXPECT_TRUE(device->ReceiveByte(&data).ok());
  EXPECT_EQ(data, expected);
}

TEST_F(SmbusEeprom2ByteAddrTest, Eeprom2ByteAddrReadBytesSuccess) {
  uint8_t value = 0xbe;
  EXPECT_CALL(access_, ReceiveByte(_, _))
      .WillRepeatedly(SmbusReceiveByte(&value));
  EXPECT_CALL(access_, Write8(_, _, _))
      .WillRepeatedly(Return(absl::OkStatus()));

  std::vector<unsigned char> expected(8, 0xbe);
  std::vector<unsigned char> data(8);

  EXPECT_TRUE(eeprom_.ReadBytes(0x55, absl::MakeSpan(data)).has_value());
  EXPECT_EQ(data, expected);
}

TEST_F(SmbusEeprom2ByteAddrTest, Eeprom2ByteAddrReadBytesFail) {
  uint8_t value = 0xbe;
  EXPECT_CALL(access_, ReceiveByte(_, _))
      .WillRepeatedly(SmbusReceiveByte(&value));
  EXPECT_CALL(access_, Write8(_, _, _))
      .WillRepeatedly(Return(absl::InternalError("")));

  std::vector<unsigned char> expected(8, 0xbe);
  std::vector<unsigned char> data(8);

  EXPECT_FALSE(eeprom_.ReadBytes(0x55, absl::MakeSpan(data)).has_value());
}

TEST_F(SmbusEeprom2ByteAddrTest, Eeprom2ByteAddrReadBytesFail2) {
  EXPECT_CALL(access_, ReceiveByte(_, _))
      .WillRepeatedly(Return(absl::InternalError("")));
  EXPECT_CALL(access_, Write8(_, _, _))
      .WillRepeatedly(Return(absl::OkStatus()));

  std::vector<unsigned char> expected(8, 0xbe);
  std::vector<unsigned char> data(8);

  EXPECT_FALSE(eeprom_.ReadBytes(0x55, absl::MakeSpan(data)).has_value());
}

TEST_F(SmbusEeprom2ByteAddrTest, Eeprom2ByteAddrWriteBytesNullOpt) {
  std::vector<unsigned char> data(8, 0xbe);

  EXPECT_FALSE(eeprom_.WriteBytes(0x55, absl::MakeConstSpan(data)).has_value());
}

class SmbusEeprom2KTest : public ::testing::Test {
 public:
  SmbusEeprom2KTest()
      : eeprom_({.name = "fru_bar", .mode = kEepromMode, .get_device = [&]() {
                   return SmbusDevice(kSmbusLocation, &access_);
                 }}) {}

 protected:
  StrictMock<MockSmbusAccessInterface> access_;
  SmbusEeprom2K eeprom_;
};

TEST_F(SmbusEeprom2KTest, I2cReadExceedLimit) {
  constexpr int kBufferSize = 260;
  unsigned char read_buffer[kBufferSize];
  EXPECT_THAT(eeprom_.ReadBytes(0, read_buffer), Eq(std::nullopt));
}

TEST_F(SmbusEeprom2KTest, I2cRead1ByteBlockUnsupported) {
  uint16_t expected = 0xFFF0;
  EXPECT_CALL(access_, SupportBlockRead(_)).WillOnce(Return(false));
  EXPECT_CALL(access_, Read16(_, _, _)).WillOnce(SmbusRead16(&expected));

  constexpr int kBufferSize = 1;
  unsigned char read_buffer[kBufferSize];
  EXPECT_EQ(kBufferSize, eeprom_.ReadBytes(0, read_buffer));
  EXPECT_THAT(read_buffer, ElementsAre(0xF0));
}

TEST_F(SmbusEeprom2KTest, I2cRead2BytesBlockUnsupported) {
  uint16_t expected = 0xFFF0;
  EXPECT_CALL(access_, SupportBlockRead(_)).WillOnce(Return(false));
  EXPECT_CALL(access_, Read16(_, _, _)).WillOnce(SmbusRead16(&expected));

  constexpr int kBufferSize = 2;
  unsigned char read_buffer[kBufferSize];
  EXPECT_EQ(kBufferSize, eeprom_.ReadBytes(0, read_buffer));
  EXPECT_THAT(read_buffer, ElementsAre(0xF0, 0xFF));
}

TEST_F(SmbusEeprom2KTest, I2cRead2BytesBlockSupported) {
  constexpr int kBufferSize = 2;
  unsigned char expected[kBufferSize]{0xF0, 0xFF};
  EXPECT_CALL(access_, SupportBlockRead(_)).WillOnce(Return(true));
  EXPECT_CALL(access_, ReadBlockI2C(_, _, _, _))
      .WillOnce(SmbusReadBlock(&expected, kBufferSize));

  unsigned char read_buffer[kBufferSize];
  EXPECT_EQ(kBufferSize, eeprom_.ReadBytes(0, read_buffer));
  EXPECT_THAT(read_buffer, ElementsAre(0xF0, 0xFF));
}

TEST_F(SmbusEeprom2KTest, I2cRead8BytesBlockSupported) {
  unsigned char expected[8]{0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  EXPECT_CALL(access_, SupportBlockRead(_)).WillOnce(Return(true));
  EXPECT_CALL(access_, ReadBlockI2C(_, _, _, _))
      .WillOnce(SmbusReadBlock(&expected, 8));

  constexpr int kBufferSize = 8;
  unsigned char read_buffer[kBufferSize];
  EXPECT_EQ(kBufferSize, eeprom_.ReadBytes(0, read_buffer));
  EXPECT_THAT(read_buffer, ElementsAreArray(expected));
}

TEST_F(SmbusEeprom2KTest, I2cRead10BytesBlockSupported) {
  unsigned char expected[8]{0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  EXPECT_CALL(access_, SupportBlockRead(_)).WillOnce(Return(true));
  EXPECT_CALL(access_, ReadBlockI2C(_, _, _, _))
      .Times(2)
      .WillRepeatedly(SmbusReadBlock(&expected, 8));

  constexpr int kBufferSize = 10;
  unsigned char read_buffer[kBufferSize];
  EXPECT_EQ(kBufferSize, eeprom_.ReadBytes(0, read_buffer));
  EXPECT_THAT(read_buffer, ElementsAre(0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
                                       0x00, 0xFF, 0xFF));
}

TEST_F(SmbusEeprom2KTest, I2cRead10BytesFirstBlockReadUnexpected) {
  unsigned char expected[7]{0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00};
  EXPECT_CALL(access_, SupportBlockRead(_)).WillOnce(Return(true));
  EXPECT_CALL(access_, ReadBlockI2C(_, _, _, _))
      .WillOnce(SmbusReadBlock(&expected, 7));
  EXPECT_CALL(access_, Read16(_, _, _))
      .WillOnce(Return(absl::InternalError("")));

  constexpr int kBufferSize = 10;
  unsigned char read_buffer[kBufferSize];
  EXPECT_THAT(eeprom_.ReadBytes(0, read_buffer), Eq(std::nullopt));
}

TEST_F(SmbusEeprom2KTest, I2cRead10BytesSecondBlockReadUnexpected) {
  unsigned char expected[8]{0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  unsigned char expected2[1]{0xFF};
  EXPECT_CALL(access_, SupportBlockRead(_)).WillOnce(Return(true));
  EXPECT_CALL(access_, ReadBlockI2C(_, _, _, _))
      .WillOnce(SmbusReadBlock(&expected, 8))
      .WillOnce(SmbusReadBlock(&expected2, 1));
  EXPECT_CALL(access_, Read16(_, _, _))
      .WillOnce(Return(absl::InternalError("")));

  constexpr int kBufferSize = 10;
  unsigned char read_buffer[kBufferSize];
  EXPECT_THAT(eeprom_.ReadBytes(0, read_buffer), Eq(std::nullopt));
}

TEST_F(SmbusEeprom2KTest, I2cRead8BytesBlockUnsupported) {
  EXPECT_CALL(access_, SupportBlockRead(_)).WillOnce(Return(false));

  uint16_t expected = 0xFF00;
  EXPECT_CALL(access_, Read16(_, _, _))
      .Times(4)
      .WillRepeatedly(SmbusRead16(&expected));

  constexpr int kBufferSize = 8;
  unsigned char read_buffer[kBufferSize];
  EXPECT_EQ(kBufferSize, eeprom_.ReadBytes(0, read_buffer));
  EXPECT_THAT(read_buffer,
              ElementsAre(0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF));
}

TEST_F(SmbusEeprom2KTest, I2cRead8BytesBlockFailOverWord) {
  EXPECT_CALL(access_, SupportBlockRead(_)).WillOnce(Return(true));
  EXPECT_CALL(access_, ReadBlockI2C(_, _, _, _))
      .WillOnce(Return(absl::InternalError("")));
  uint16_t expected = 0xABCD;
  EXPECT_CALL(access_, Read16(_, _, _))
      .Times(4)
      .WillRepeatedly(SmbusRead16(&expected));

  constexpr int kBufferSize = 8;
  unsigned char read_buffer[kBufferSize];
  EXPECT_EQ(kBufferSize, eeprom_.ReadBytes(0, read_buffer));
  EXPECT_THAT(read_buffer,
              ElementsAre(0xCD, 0xAB, 0xCD, 0xAB, 0xCD, 0xAB, 0xCD, 0xAB));
}

TEST_F(SmbusEeprom2KTest, I2cRead8BytesWordFail) {
  EXPECT_CALL(access_, SupportBlockRead(_)).WillOnce(Return(false));
  EXPECT_CALL(access_, Read16(_, _, _))
      .WillOnce(Return(absl::InternalError("")));

  constexpr int kBufferSize = 8;
  unsigned char read_buffer[kBufferSize];
  EXPECT_THAT(eeprom_.ReadBytes(0, read_buffer), Eq(std::nullopt));
}

TEST_F(SmbusEeprom2KTest, WriteBytesNullOpt) {
  std::vector<unsigned char> data(8, 0xbe);

  EXPECT_THAT(eeprom_.WriteBytes(0x55, absl::MakeConstSpan(data)),
              Eq(std::nullopt));
}

}  // namespace
}  // namespace ecclesia
