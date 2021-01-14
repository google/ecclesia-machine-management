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

#include "ecclesia/magent/sysmodel/x86/fru.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "ecclesia/magent/lib/eeprom/smbus_eeprom.h"
#include "ecclesia/magent/lib/eeprom/smbus_eeprom_mock.h"
#include "ecclesia/magent/lib/ipmi/ipmi_mock.h"

namespace ecclesia {
namespace {

using ::testing::_;
using ::testing::Eq;
using ::testing::Return;

TEST(IpmiSysmodelFruReaderTest, ReadFailure) {
  MockIpmiInterface ipmi_intf;
  uint16_t fru_id = 1;
  IpmiSysmodelFruReader ipmi_fru_reader(&ipmi_intf, fru_id);
  EXPECT_CALL(ipmi_intf, ReadFru(fru_id, 0, _))
      .WillOnce(Return(absl::NotFoundError("not found!")));
  EXPECT_FALSE(ipmi_fru_reader.Read());
}

TEST(IpmiSysmodelFruReaderTest, ReadFruSuccess) {
  MockIpmiInterface ipmi_intf;
  uint16_t fru_id = 1;
  IpmiSysmodelFruReader ipmi_fru_reader(&ipmi_intf, fru_id);
  // This is handcrafted FRU data based off of a real system.
  std::vector<uint8_t> data = {
      0x1,  0x0,  0x0,  0x1,  0x0,  0x0,  0x0,  0xfe, 0x1,  0x8,  0x0,  0x3a,
      0x86, 0xbd, 0xc6, 0x67, 0x6d, 0x61, 0x6b, 0x65, 0x72, 0xcc, 0x53, 0x6c,
      0x65, 0x69, 0x70, 0x6e, 0x69, 0x72, 0x20, 0x42, 0x4d, 0x43, 0xcf, 0x53,
      0x52, 0x43, 0x51, 0x54, 0x57, 0x31, 0x39, 0x33, 0x31, 0x30, 0x30, 0x31,
      0x32, 0x35, 0xca, 0x31, 0x30, 0x35, 0x33, 0x39, 0x34, 0x38, 0x2d, 0x30,
      0x32, 0x0,  0x0,  0xc1, 0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x38};
  auto expect_data = absl::MakeSpan(data);
  EXPECT_CALL(ipmi_intf, ReadFru(fru_id, 0, _))
      .WillOnce(IpmiReadFru(expect_data.data()));
  auto optional_fru_reader = ipmi_fru_reader.Read();
  ASSERT_TRUE(optional_fru_reader.has_value());
  auto fru_reader = optional_fru_reader.value();
  EXPECT_EQ(fru_reader.GetManufacturer(), "gmaker");
  EXPECT_EQ(fru_reader.GetPartNumber(), "1053948-02");
  EXPECT_EQ(fru_reader.GetSerialNumber(), "SRCQTW193100125");
}

TEST(SmbusEepromFruReaderTest, ReadFailure) {
  SmbusEeprom::Option eeprom_option_;
  std::unique_ptr<MockSmbusEeprom> smbus_eeprom =
      std::make_unique<MockSmbusEeprom>(eeprom_option_);
  EXPECT_CALL(*smbus_eeprom, ReadBytes(_, _)).WillOnce(Return(absl::nullopt));

  SmbusEepromFruReader fru_reader(std::move(smbus_eeprom));

  EXPECT_THAT(fru_reader.Read(), Eq(absl::nullopt));
}

TEST(SmbusEepromFruReaderTest, ReadFruSuccess) {
  SmbusEeprom::Option eeprom_option_;
  std::unique_ptr<MockSmbusEeprom> smbus_eeprom =
      std::make_unique<MockSmbusEeprom>(eeprom_option_);
  unsigned char expcect1[1]{0x07};
  // This is handcrafted FRU data based off of a real system.
  unsigned char expcect2[64]{
      0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xfe, 0x01, 0x07, 0x19,
      0xa9, 0xd2, 0xbf, 0xc7, 0x67, 0x6d, 0x61, 0x6b, 0x65, 0x72, 0x00,
      0xc8, 0x53, 0x70, 0x69, 0x63, 0x79, 0x31, 0x36, 0x00, 0xd0, 0x50,
      0x41, 0x50, 0x51, 0x54, 0x57, 0x31, 0x39, 0x34, 0x37, 0x30, 0x30,
      0x31, 0x30, 0x38, 0x00, 0xcb, 0x31, 0x30, 0x35, 0x31, 0x35, 0x37,
      0x38, 0x2d, 0x30, 0x33, 0x00, 0xc0, 0xc1, 0x00, 0x6e};

  EXPECT_CALL(*smbus_eeprom, ReadBytes(_, _))
      .WillOnce(SmbusEepromReadBytes(expcect1))
      .WillOnce(SmbusEepromReadBytes(expcect2));

  SmbusEepromFruReader fru_reader(std::move(smbus_eeprom));

  auto optional_fru = fru_reader.Read();
  ASSERT_TRUE(optional_fru.has_value());
  const auto &fru = optional_fru.value();
  EXPECT_EQ(fru.GetManufacturer(), "gmaker");
  EXPECT_EQ(fru.GetPartNumber(), "1051578-03");
  EXPECT_EQ(fru.GetSerialNumber(), "PAPQTW194700108");
}

}  // namespace
}  // namespace ecclesia
