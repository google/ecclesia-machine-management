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

#include "ecclesia/magent/lib/fru/fru.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/base/macros.h"
#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "ecclesia/magent/lib/fru/common_header.emb.h"
#include "runtime/cpp/emboss_prelude.h"

namespace ecclesia {
namespace {

using ::testing::HasSubstr;

constexpr uint8_t kFormatVersion = 1;

// This is handcrafted FRU data based off of a real system.
static uint8_t prefab_frudata[] = {
    0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xfe, 0x01, 0x07, 0x00,
    0xb3, 0x1e, 0x63, 0xc7, 0x67, 0x6d, 0x61, 0x6b, 0x65, 0x72, 0x00,
    0xc7, 0x67, 0x70, 0x72, 0x6f, 0x64, 0x73, 0x00, 0xcb, 0x30, 0x38,
    0x31, 0x39, 0x30, 0x30, 0x30, 0x30, 0x34, 0x33, 0x00, 0xcb, 0x47,
    0x4b, 0x30, 0x38, 0x43, 0x33, 0x41, 0x30, 0x36, 0x36, 0x00, 0x00,
    0xc1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x93,
};

// This is a handcrafted problematic FRU where the board area is not terminated
// by a 0xc1 field.
static uint8_t no_area_termination_frudata[] = {
    0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xfe, 0x01, 0x07, 0x19,
    0x31, 0x7e, 0x86, 0xc7, 0x67, 0x6d, 0x61, 0x6b, 0x65, 0x72, 0x00,
    0xc9, 0x4c, 0x65, 0x61, 0x66, 0x4c, 0x6f, 0x63, 0x6b, 0x00, 0xd0,
    0x50, 0x46, 0x4c, 0x51, 0x53, 0x48, 0x31, 0x32, 0x33, 0x39, 0x30,
    0x30, 0x30, 0x31, 0x31, 0x00, 0xcc, 0x34, 0x32, 0x30, 0x30, 0x30,
    0x33, 0x31, 0x31, 0x2d, 0x30, 0x31, 0x00, 0x00, 0x5e,
};

// Helper functions to convert FRU time format to time_t and back.
static const time_t kFruEpoch = 820454400;
static time_t TimeFromFruTime(uint32_t fru_time) {
  return (fru_time * 60) + kFruEpoch;
}

// Helper function to calculate a rolling 8-bit checksum.
static uint8_t Rolling8Csum(const uint8_t *image, size_t len) {
  uint8_t sum = 0;
  for (size_t i = 0; i < len; ++i) {
    sum += *(image + i);
  }
  return sum;
}

// Helper function to make a FruField from a string.
static FruField FruFieldFromString(const std::string &str) {
  FruField f;
  f.SetData(str);
  f.SetType(FruField::kTypeLanguageBased);
  return f;
}

// Helper function to make a FruField from a raw data and type
static FruField FruFieldFromRawData(absl::Span<unsigned char> data,
                                    const FruField::Type &type) {
  FruField f;
  f.SetData(data);
  f.SetType(type);
  return f;
}

// Helper to check if two field locations are equal.
static bool CompareFruFieldLocations(const FruArea::FieldLocation &a,
                                     const FruArea::FieldLocation &b) {
  return (a.offset == b.offset) && (a.size == b.size);
}

// Test that ChassisInfoArea::GetImage works properly when empty.
TEST(FruTest, ChassisInfoAreaGetImageEmpty) {
  ChassisInfoArea cia;
  std::vector<unsigned char> cia_image;
  cia.GetImage(&cia_image);

  // Empty chassis info area has size 8.
  EXPECT_EQ(8, cia_image.size());

  // Verify contents of required chassis info area fields.
  EXPECT_EQ(1, cia_image[0]);  // Format version
  EXPECT_EQ(1, cia_image[1]);  // Length (in multiples of 8 bytes)
  EXPECT_EQ(ChassisInfoArea::kUnknown, cia_image[2]);    // Default chassis type
  EXPECT_EQ(FruTypelenByteFromLength(0), cia_image[3]);  // No part number
  EXPECT_EQ(FruTypelenByteFromLength(0), cia_image[4]);  // No serial number
  EXPECT_EQ(kFruNoMoreFields, cia_image[5]);  // No custom fields indicator
  EXPECT_EQ(0, cia_image[6]);                 // Pad byte should be zero
  EXPECT_EQ(0, Rolling8Csum(&cia_image[0], cia_image.size()));  // Valid csum
}

// Test that ChassisInfoArea::GetImage works properly with some mock data.
TEST(FruTest, ChassisInfoAreaGetImageWithData) {
  FruField part_number = FruFieldFromString("Part 123");
  FruField serial_number = FruFieldFromString("Serial ABC");

  ChassisInfoArea cia;
  cia.set_type(ChassisInfoArea::kDesktop);
  cia.set_part_number(part_number);
  cia.set_serial_number(serial_number);
  std::vector<unsigned char> cia_image;
  cia.GetImage(&cia_image);

  // The info area should be large enough to fit all required bytes plus
  // bytes of the given strings, padded to the next multiple of 8 bytes.
  size_t contents_size = 7  // 7 bytes for required fields
                         + part_number.GetData().size() +
                         serial_number.GetData().size();
  EXPECT_EQ(contents_size + (8 - (contents_size % 8)), cia_image.size());

  // Make sure strings came through okay.
  // Part_number string should start at index 4.
  std::string image_part_number(reinterpret_cast<const char *>(&cia_image[4]),
                                part_number.GetData().size());
  EXPECT_EQ(part_number.GetDataAsString(), image_part_number);
  // Serial_number string should start at index 13.
  std::string image_serial_number(
      reinterpret_cast<const char *>(&cia_image[13]),
      serial_number.GetData().size());
  EXPECT_EQ(serial_number.GetDataAsString(), image_serial_number);

  // Verify checksum.
  EXPECT_EQ(0, Rolling8Csum(&cia_image[0], cia_image.size()));
}

// Test that ChassisInfoArea::FillFromImage can read from a FRU image generated
// by ChassisInfoArea::GetImage.
TEST(FruTest, ChassisInfoAreaFillFromImage) {
  FruField part_number = FruFieldFromString("Part 123");
  FruField serial_number = FruFieldFromString("Serial ABC");

  ChassisInfoArea cia;
  cia.set_type(ChassisInfoArea::kDesktop);
  cia.set_part_number(part_number);
  cia.set_serial_number(serial_number);
  std::vector<unsigned char> cia_image;
  cia.GetImage(&cia_image);

  ChassisInfoArea cia2;
  cia2.FillFromImage(VectorFruImageSource(absl::MakeSpan(cia_image)), 0);

  EXPECT_EQ(cia.type(), cia2.type());
  EXPECT_TRUE(cia.part_number().Equals(cia2.part_number()));
  EXPECT_TRUE(cia.serial_number().Equals(cia2.serial_number()));
  // Verify location of variable fields.
  FruArea::FieldLocation serial_number_location = {
      13, serial_number.GetData().size()};
  FruArea::FieldLocation checksum_location = {22, 1};
  std::vector<FruArea::FieldLocation> expected_variable_fields_location = {
      serial_number_location, checksum_location};
  std::vector<FruArea::FieldLocation> actual_variable_fields_location =
      cia2.GetVariableFieldsLocation();
  EXPECT_EQ(expected_variable_fields_location.size(),
            actual_variable_fields_location.size());
  EXPECT_TRUE(std::is_permutation(actual_variable_fields_location.begin(),
                                  actual_variable_fields_location.end(),
                                  expected_variable_fields_location.begin(),
                                  CompareFruFieldLocations));
  const size_t expected_cia_size = cia_image.size();
  cia_image.resize(cia_image.size() * 2);
  size_t cia_end =
      cia2.FillFromImage(VectorFruImageSource(absl::MakeSpan(cia_image)), 0);
  EXPECT_EQ(expected_cia_size, cia_end);
}

// Test that BoardInfoArea::GetImage works properly when empty.
TEST(FruTest, BoardInfoAreaGetImageEmpty) {
  BoardInfoArea bia;
  std::vector<unsigned char> bia_image;
  bia.GetImage(&bia_image);

  // Empty board info area has size 16.
  EXPECT_EQ(16, bia_image.size());

  // Verify contents of required board info area fields.
  EXPECT_EQ(1, bia_image[0]);  // Format version
  EXPECT_EQ(2, bia_image[1]);  // Length (in multiples of 8 bytes)
  EXPECT_EQ(0, bia_image[2]);  // Language code
  // Manufacture date
  EXPECT_EQ(kFruEpoch, TimeFromFruTime(bia_image[3] | bia_image[4] << 8 |
                                       bia_image[5] << 16));
  EXPECT_EQ(FruTypelenByteFromLength(0), bia_image[6]);   // No manufacturer
  EXPECT_EQ(FruTypelenByteFromLength(0), bia_image[7]);   // No product name
  EXPECT_EQ(FruTypelenByteFromLength(0), bia_image[8]);   // No serial number
  EXPECT_EQ(FruTypelenByteFromLength(0), bia_image[9]);   // No part number
  EXPECT_EQ(FruTypelenByteFromLength(0), bia_image[10]);  // No file ID
  EXPECT_EQ(kFruNoMoreFields, bia_image[11]);  // No custom fields indicator
  // Pad bytes should be zero.
  for (size_t i = 12; i < 15; ++i) {
    EXPECT_EQ(0, bia_image[i]);
  }
  EXPECT_EQ(0, Rolling8Csum(&bia_image[0], bia_image.size()));  // Valid csum
}

// Test that BoardInfoArea::GetImage works properly with some mock data.
TEST(FruTest, BoardInfoAreaGetImageWithData) {
  FruField manufacturer = FruFieldFromString("Manufacturer ABC");
  FruField product_name = FruFieldFromString("Product XYZ");
  FruField serial_number = FruFieldFromString("Serial ABC");
  FruField part_number = FruFieldFromString("Part PPP");
  FruField file_id = FruFieldFromString("File 123");

  BoardInfoArea bia;
  bia.set_manufacturer(manufacturer);
  bia.set_product_name(product_name);
  bia.set_serial_number(serial_number);
  bia.set_part_number(part_number);
  bia.set_file_id(file_id);
  std::vector<unsigned char> bia_image;
  bia.GetImage(&bia_image);

  // The info area should be large enough to fit all required bytes plus
  // bytes of the given strings, padded to the next multiple of 8 bytes.
  size_t contents_size =
      13  // 13 bytes for required fields
      + manufacturer.GetData().size() + product_name.GetData().size() +
      serial_number.GetData().size() + part_number.GetData().size() +
      file_id.GetData().size();
  EXPECT_EQ(contents_size + (8 - (contents_size % 8)), bia_image.size());

  // Make sure strings came through okay.
  // Manufacturer string should start at index 7.
  std::string image_manufacturer(reinterpret_cast<const char *>(&bia_image[7]),
                                 manufacturer.GetData().size());
  EXPECT_EQ(manufacturer.GetDataAsString(), image_manufacturer);
  // Product name string should start at index 24.
  std::string image_product_name(reinterpret_cast<const char *>(&bia_image[24]),
                                 product_name.GetData().size());
  EXPECT_EQ(product_name.GetDataAsString(), image_product_name);
  // Serial number string should start at index 36.
  std::string image_serial_number(
      reinterpret_cast<const char *>(&bia_image[36]),
      serial_number.GetData().size());
  EXPECT_EQ(serial_number.GetDataAsString(), image_serial_number);
  // Part number string should start at index 47.
  std::string image_part_number(reinterpret_cast<const char *>(&bia_image[47]),
                                part_number.GetData().size());
  EXPECT_EQ(part_number.GetDataAsString(), image_part_number);
  // File ID should start at index 56.
  std::string image_file_id(reinterpret_cast<const char *>(&bia_image[56]),
                            file_id.GetData().size());
  EXPECT_EQ(file_id.GetDataAsString(), image_file_id);

  // Verify checksum
  EXPECT_EQ(0, Rolling8Csum(&bia_image[0], bia_image.size()));
}

// Test that BoardInfoArea::FillFromImage can read from a FRU image generated
// by BoardInfoArea::GetImage.
TEST(FruTest, BoardInfoAreaFillFromImage) {
  FruField manufacturer = FruFieldFromString("Manufacturer ABC");
  FruField product_name = FruFieldFromString("Product XYZ");
  FruField serial_number = FruFieldFromString("Serial 123");
  FruField part_number = FruFieldFromString("Part PPP");
  FruField file_id = FruFieldFromString("File 123");

  BoardInfoArea bia;
  bia.set_manufacturer(manufacturer);
  bia.set_product_name(product_name);
  bia.set_serial_number(serial_number);
  bia.set_part_number(part_number);
  bia.set_file_id(file_id);
  std::vector<unsigned char> bia_image;
  bia.GetImage(&bia_image);

  BoardInfoArea bia2;
  bia2.FillFromImage(VectorFruImageSource(absl::MakeSpan(bia_image)), 0);

  EXPECT_EQ(bia.language_code(), bia2.language_code());
  EXPECT_EQ(bia.manufacture_date(), bia2.manufacture_date());
  EXPECT_TRUE(bia.manufacturer().Equals(bia2.manufacturer()));
  EXPECT_TRUE(bia.product_name().Equals(bia2.product_name()));
  EXPECT_TRUE(bia.serial_number().Equals(bia2.serial_number()));
  EXPECT_TRUE(bia.part_number().Equals(bia2.part_number()));
  EXPECT_TRUE(bia.file_id().Equals(bia2.file_id()));
  // Verify location of variable fields.
  FruArea::FieldLocation serial_number_location = {
      36, serial_number.GetData().size()};
  FruArea::FieldLocation manufacture_date_location = {3, 3};
  FruArea::FieldLocation checksum_location = {63, 1};
  std::vector<FruArea::FieldLocation> expected_variable_fields_location = {
      serial_number_location, manufacture_date_location, checksum_location};
  std::vector<FruArea::FieldLocation> actual_variable_fields_location =
      bia2.GetVariableFieldsLocation();
  EXPECT_EQ(expected_variable_fields_location.size(),
            actual_variable_fields_location.size());
  EXPECT_TRUE(std::is_permutation(actual_variable_fields_location.begin(),
                                  actual_variable_fields_location.end(),
                                  expected_variable_fields_location.begin(),
                                  CompareFruFieldLocations));
  const size_t expected_bia_size = bia_image.size();
  bia_image.resize(bia_image.size() * 2);
  size_t bia_end =
      bia2.FillFromImage(VectorFruImageSource(absl::MakeSpan(bia_image)), 0);
  EXPECT_EQ(expected_bia_size, bia_end);
}

// Test that ProductInfoArea::GetImage works properly when empty.
TEST(FruTest, ProductInfoAreaGetImageEmpty) {
  ProductInfoArea pia;
  std::vector<unsigned char> pia_image;
  pia.GetImage(&pia_image);

  // Empty product info area has size 16.
  EXPECT_EQ(16, pia_image.size());

  // Verify contents of required product info area fields.
  EXPECT_EQ(1, pia_image[0]);  // Format version
  EXPECT_EQ(2, pia_image[1]);  // Length (in multiples of 8 bytes)
  EXPECT_EQ(0, pia_image[2]);  // Language code
  // Manufacture date
  EXPECT_EQ(FruTypelenByteFromLength(0), pia_image[3]);  // No manufacturer
  EXPECT_EQ(FruTypelenByteFromLength(0), pia_image[4]);  // No product name
  EXPECT_EQ(FruTypelenByteFromLength(0), pia_image[5]);  // No part number
  EXPECT_EQ(FruTypelenByteFromLength(0), pia_image[6]);  // No product version
  EXPECT_EQ(FruTypelenByteFromLength(0), pia_image[7]);  // No serial number
  EXPECT_EQ(FruTypelenByteFromLength(0), pia_image[8]);  // No asset tag
  EXPECT_EQ(FruTypelenByteFromLength(0), pia_image[9]);  // No file ID
  EXPECT_EQ(kFruNoMoreFields, pia_image[10]);  // No custom fields indicator
  // Pad bytes should be zero.
  for (size_t i = 11; i < 15; ++i) {
    EXPECT_EQ(0, pia_image[i]);
  }
  EXPECT_EQ(0, Rolling8Csum(&pia_image[0], pia_image.size()));  // Valid csum
}

// Test that ProductInfoArea::GetImage works properly with some mock data.
TEST(FruTest, ProductInfoAreaGetImageWithData) {
  FruField manufacturer = FruFieldFromString("Manufacturer ABC");
  FruField product_name = FruFieldFromString("Product XYZ");
  FruField part_number = FruFieldFromString("Part PPP");
  FruField product_version = FruFieldFromString("v1.0");
  FruField serial_number = FruFieldFromString("Serial ABC");
  FruField asset_tag = FruFieldFromString("Asset Tag 555");
  FruField file_id = FruFieldFromString("File 123");

  ProductInfoArea pia;
  pia.set_manufacturer(manufacturer);
  pia.set_product_name(product_name);
  pia.set_part_number(part_number);
  pia.set_product_version(product_version);
  pia.set_serial_number(serial_number);
  pia.set_asset_tag(asset_tag);
  pia.set_file_id(file_id);
  std::vector<unsigned char> pia_image;
  pia.GetImage(&pia_image);

  // The info area should be large enough to fit all required bytes plus
  // bytes of the given strings, padded to the next multiple of 8 bytes.
  size_t contents_size =
      12  // 12 bytes for required fields
      + manufacturer.GetData().size() + product_name.GetData().size() +
      part_number.GetData().size() + product_version.GetData().size() +
      serial_number.GetData().size() + asset_tag.GetData().size() +
      file_id.GetData().size();
  EXPECT_EQ(contents_size + (8 - (contents_size % 8)), pia_image.size());

  // Make sure strings came through okay.
  // Manufacturer string should start at index 4.
  std::string image_manufacturer(reinterpret_cast<const char *>(&pia_image[4]),
                                 manufacturer.GetData().size());
  EXPECT_EQ(manufacturer.GetDataAsString(), image_manufacturer);
  // Product name string should start at index 21.
  std::string image_product_name(reinterpret_cast<const char *>(&pia_image[21]),
                                 product_name.GetData().size());
  EXPECT_EQ(product_name.GetDataAsString(), image_product_name);
  // Part number string should start at index 33.
  std::string image_part_number(reinterpret_cast<const char *>(&pia_image[33]),
                                part_number.GetData().size());
  EXPECT_EQ(part_number.GetDataAsString(), image_part_number);
  // Product version string should start at index 42.
  std::string image_product_version(
      reinterpret_cast<const char *>(&pia_image[42]),
      product_version.GetData().size());
  EXPECT_EQ(product_version.GetDataAsString(), image_product_version);
  // Serial number string should start at index 47.
  std::string image_serial_number(
      reinterpret_cast<const char *>(&pia_image[47]),
      serial_number.GetData().size());
  EXPECT_EQ(serial_number.GetDataAsString(), image_serial_number);
  // Asset tag should start at index 58.
  std::string image_asset_tag(reinterpret_cast<const char *>(&pia_image[58]),
                              asset_tag.GetData().size());
  EXPECT_EQ(asset_tag.GetDataAsString(), image_asset_tag);
  // File ID should start at index 72.
  std::string image_file_id(reinterpret_cast<const char *>(&pia_image[72]),
                            file_id.GetData().size());
  EXPECT_EQ(file_id.GetDataAsString(), image_file_id);

  // Verify checksum
  EXPECT_EQ(0, Rolling8Csum(&pia_image[0], pia_image.size()));
}

// Test that ProductInfoArea::FillFromImage can read from a FRU image generated
// by ProductInfoArea::GetImage.
TEST(FruTest, ProductInfoAreaFillFromImage) {
  FruField manufacturer = FruFieldFromString("Manufacturer ABC");
  FruField product_name = FruFieldFromString("Product XYZ");
  FruField part_number = FruFieldFromString("Part PPP");
  FruField product_version = FruFieldFromString("v1.0");
  FruField serial_number = FruFieldFromString("Serial ABC");
  FruField asset_tag = FruFieldFromString("Asset Tag 555");
  FruField file_id = FruFieldFromString("File 123");

  ProductInfoArea pia;
  pia.set_manufacturer(manufacturer);
  pia.set_product_name(product_name);
  pia.set_part_number(part_number);
  pia.set_product_version(product_version);
  pia.set_serial_number(serial_number);
  pia.set_asset_tag(asset_tag);
  pia.set_file_id(file_id);

  std::vector<unsigned char> pia_image;
  pia.GetImage(&pia_image);

  ProductInfoArea pia2;
  pia2.FillFromImage(VectorFruImageSource(absl::MakeSpan(pia_image)), 0);

  EXPECT_EQ(pia.language_code(), pia2.language_code());
  EXPECT_TRUE(pia.manufacturer().Equals(pia2.manufacturer()));
  EXPECT_TRUE(pia.product_name().Equals(pia2.product_name()));
  EXPECT_TRUE(pia.part_number().Equals(pia2.part_number()));
  EXPECT_TRUE(pia.product_version().Equals(pia2.product_version()));
  EXPECT_TRUE(pia.serial_number().Equals(pia2.serial_number()));
  EXPECT_TRUE(pia.asset_tag().Equals(pia2.asset_tag()));
  EXPECT_TRUE(pia.file_id().Equals(pia2.file_id()));
  // Verify location of variable fields.
  FruArea::FieldLocation serial_number_location = {
      47, serial_number.GetData().size()};
  FruArea::FieldLocation checksum_location = {79, 1};
  std::vector<FruArea::FieldLocation> expected_variable_fields_location = {
      serial_number_location, checksum_location};
  std::vector<FruArea::FieldLocation> actual_variable_fields_location =
      pia2.GetVariableFieldsLocation();
  EXPECT_EQ(expected_variable_fields_location.size(),
            actual_variable_fields_location.size());
  EXPECT_TRUE(std::is_permutation(actual_variable_fields_location.begin(),
                                  actual_variable_fields_location.end(),
                                  expected_variable_fields_location.begin(),
                                  CompareFruFieldLocations));
  const size_t expected_pia_size = pia_image.size();
  pia_image.resize(pia_image.size() * 2);
  size_t pia_end =
      pia2.FillFromImage(VectorFruImageSource(absl::MakeSpan(pia_image)), 0);
  EXPECT_EQ(expected_pia_size, pia_end);
}

TEST(FruTest, MultiRecordGetSetTest) {
  MultiRecord multi_record;
  EXPECT_EQ(0, multi_record.get_type());
  multi_record.set_type(4);
  EXPECT_EQ(4, multi_record.get_type());
  EXPECT_EQ(5, multi_record.get_size());
  std::vector<unsigned char> data;
  const int kDataSize = 8;
  for (int i = 0; i < kDataSize; ++i) {
    data.push_back(i);
  }
  multi_record.set_data(absl::MakeSpan(data));
  EXPECT_EQ(5 + kDataSize, multi_record.get_size());

  std::vector<unsigned char> data2;
  multi_record.get_data(&data2);
  EXPECT_EQ(kDataSize, data2.size());
  for (int i = 0; i < kDataSize; ++i) {
    EXPECT_EQ(data[i], data2[i]) << "Index: " << i;
  }
}

TEST(FruTest, MultiRecordImageTooSmall) {
  // Some nonsense data that shouldn't be read.
  std::vector<unsigned char> fru_image;
  fru_image.push_back(0x00);
  fru_image.push_back(0x01);
  MultiRecord multirecord;
  bool end_of_list;
  EXPECT_FALSE(multirecord.FillFromImage(
      VectorFruImageSource(absl::MakeSpan(fru_image)), 0, &end_of_list));
  fru_image.clear();
  const int kDataSize = 8;
  for (int i = 0; i < kDataSize; ++i) {
    fru_image.push_back(i);
  }
  EXPECT_FALSE(
      multirecord.FillFromImage(VectorFruImageSource(absl::MakeSpan(fru_image)),
                                kDataSize - 4, &end_of_list));
}

TEST(FruTest, MultiRecordHeaderChecksum) {
  std::vector<unsigned char> good_header = {0x1, 0x82, 0x0, 0x0, 0x7d};
  MultiRecord multirecord;
  bool end_of_list;
  EXPECT_TRUE(multirecord.FillFromImage(
      VectorFruImageSource(absl::MakeSpan(good_header)), 0, &end_of_list));
  EXPECT_TRUE(end_of_list);
  EXPECT_EQ(1, multirecord.get_type());
  std::vector<unsigned char> got_data;
  multirecord.get_data(&got_data);
  EXPECT_EQ(0, got_data.size());
  std::vector<unsigned char> bad_header = {0x1, 0x82, 0x0, 0x0, 0x7e};
  EXPECT_FALSE(multirecord.FillFromImage(
      VectorFruImageSource(absl::MakeSpan(bad_header)), 0, &end_of_list));
}

TEST(FruTest, MultiRecordFillFromImage) {
  std::vector<unsigned char> expected = {0x1, 0x82, 0x0, 0x0, 0x7d};
  std::vector<unsigned char> bytes;
  MultiRecord record;
  record.set_type(1);

  record.GetImage(true, &bytes);
  EXPECT_EQ(expected.size(), bytes.size());
  for (int i = 0; i < bytes.size(); ++i) {
    EXPECT_EQ(expected[i], bytes[i]) << "Index: " << i;
  }
  MultiRecord got_record;
  bool end_of_list;
  EXPECT_TRUE(got_record.FillFromImage(
      VectorFruImageSource(absl::MakeSpan(bytes)), 0, &end_of_list));
  EXPECT_TRUE(end_of_list);
  EXPECT_EQ(1, got_record.get_type());
}

TEST(FruTest, MultiRecordFillFromImageWithData) {
  std::vector<unsigned char> expected = {0x1,  0x82, 0x3, 0xF2,
                                         0x88, 0x2,  0x4, 0x8};
  std::vector<unsigned char> bytes;
  MultiRecord record;
  record.set_type(1);
  std::vector<unsigned char> expected_data({0x2, 0x4, 0x8});
  record.set_data(absl::MakeSpan(expected_data));

  record.GetImage(true, &bytes);
  EXPECT_EQ(expected.size(), bytes.size());
  for (int i = 0; i < bytes.size(); ++i) {
    EXPECT_EQ(expected[i], bytes[i]) << "Index: " << i;
  }
  MultiRecord got_record;
  bool end_of_list;
  EXPECT_TRUE(got_record.FillFromImage(
      VectorFruImageSource(absl::MakeSpan(bytes)), 0, &end_of_list));
  EXPECT_TRUE(end_of_list);
  EXPECT_EQ(1, got_record.get_type());
  std::vector<unsigned char> got_data;
  got_record.get_data(&got_data);
  EXPECT_EQ(expected_data.size(), got_data.size());
  for (int i = 0; i < got_data.size(); ++i) {
    EXPECT_EQ(expected_data[i], got_data[i]) << "Index: " << i;
  }
}

TEST(FruTest, MultiRecordAreaEndToEndMulti) {
  std::vector<unsigned char> expected = {0x1, 0x02, 0x3,  0xF2, 0x08, 0x2, 0x4,
                                         0x8, 0x1,  0x82, 0x0,  0x0,  0x7d};
  std::vector<unsigned char> bytes;
  MultiRecordArea record_area;
  MultiRecord record0;
  record0.set_type(1);
  std::vector<unsigned char> expected_data({0x2, 0x4, 0x8});
  record0.set_data(absl::MakeSpan(expected_data));
  record_area.AddMultiRecord(record0);

  MultiRecord record1;
  record1.set_type(1);
  record_area.AddMultiRecord(record1);

  record_area.GetImage(&bytes);
  EXPECT_EQ(expected.size(), bytes.size());
  for (int i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(expected[i], bytes[i]) << "Index: " << i;
  }
  MultiRecordArea got_record_area;
  EXPECT_TRUE(got_record_area.FillFromImage(
      VectorFruImageSource(absl::MakeSpan(bytes)), 0));
  MultiRecord got_record0;
  EXPECT_TRUE(got_record_area.GetMultiRecord(0, &got_record0));
  EXPECT_EQ(1, got_record0.get_type());
  std::vector<unsigned char> got_data;
  got_record0.get_data(&got_data);
  EXPECT_EQ(expected_data.size(), got_data.size());
  for (int i = 0; i < expected_data.size(); ++i) {
    EXPECT_EQ(expected_data[i], got_data[i]) << "Index: " << i;
  }
  MultiRecord got_record1;
  EXPECT_TRUE(got_record_area.GetMultiRecord(1, &got_record1));
  EXPECT_EQ(1, got_record1.get_type());
  got_record1.get_data(&got_data);
  EXPECT_EQ(0, got_data.size());
  EXPECT_FALSE(got_record_area.GetMultiRecord(2, &got_record0));
  const size_t expected_mra_size = bytes.size();
  bytes.resize(bytes.size() * 2);
  size_t mra_end = got_record_area.FillFromImage(
      VectorFruImageSource(absl::MakeSpan(bytes)), 0);
  EXPECT_EQ(expected_mra_size, mra_end);
}

TEST(FruTest, MultiRecordAreaEndToEndSingle) {
  std::vector<unsigned char> expected = {0x1, 0x82, 0x0, 0x0, 0x7d};
  std::vector<unsigned char> bytes;
  MultiRecordArea record_area;
  MultiRecord record0;
  record0.set_type(1);
  record_area.AddMultiRecord(record0);
  record_area.GetImage(&bytes);
  EXPECT_EQ(expected.size(), bytes.size());
  for (int i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(expected[i], bytes[i]) << "Index: " << i;
  }
  MultiRecordArea got_record_area;
  EXPECT_TRUE(got_record_area.FillFromImage(
      VectorFruImageSource(absl::MakeSpan(bytes)), 0));
  MultiRecord got_record0;
  EXPECT_TRUE(got_record_area.GetMultiRecord(0, &got_record0));
  EXPECT_EQ(1, got_record0.get_type());
  std::vector<unsigned char> got_data;
  got_record0.get_data(&got_data);
  EXPECT_EQ(0, got_data.size());
  EXPECT_FALSE(got_record_area.GetMultiRecord(1, &got_record0));
  const size_t expected_mra_size = bytes.size();
  bytes.resize(bytes.size() * 2);
  size_t mra_end = got_record_area.FillFromImage(
      VectorFruImageSource(absl::MakeSpan(bytes)), 0);
  EXPECT_EQ(expected_mra_size, mra_end);
}

TEST(FruTest, MultiRecordAreaEndToEndEmpty) {
  std::vector<unsigned char> bytes;
  MultiRecordArea record_area;
  record_area.GetImage(&bytes);
  EXPECT_EQ(0, bytes.size());
  MultiRecordArea got_record_area;
  EXPECT_FALSE(got_record_area.FillFromImage(
      VectorFruImageSource(absl::MakeSpan(bytes)), 0));
}

TEST(FruTest, MultiRecordAreaUpdate) {
  MultiRecordArea record_area;
  MultiRecord record;
  const uint32_t kRecordCount = 8;
  for (uint8_t i = 0; i < kRecordCount; ++i) {
    record.set_type(i);
    EXPECT_TRUE(record_area.AddMultiRecord(record));
  }
  EXPECT_EQ(kRecordCount, record_area.GetMultiRecordCount());
  for (uint8_t i = 0; i < kRecordCount; ++i) {
    EXPECT_TRUE(record_area.GetMultiRecord(i, &record));
    EXPECT_EQ(i, record.get_type());
  }
  for (uint8_t i = 0; i < kRecordCount; ++i) {
    record.set_type(i + kRecordCount);
    EXPECT_TRUE(record_area.UpdateMultiRecord(i, record));
  }
  EXPECT_EQ(kRecordCount, record_area.GetMultiRecordCount());
  for (uint8_t i = 0; i < kRecordCount; ++i) {
    EXPECT_TRUE(record_area.GetMultiRecord(i, &record));
    EXPECT_EQ(i + kRecordCount, record.get_type());
  }
  for (uint8_t i = 0; i < record_area.GetMultiRecordCount(); ++i) {
    EXPECT_TRUE(record_area.GetMultiRecord(i, &record));
    if (record.get_type() % 2) {
      EXPECT_TRUE(record_area.RemoveMultiRecord(i));
    }
  }
  EXPECT_EQ(kRecordCount / 2, record_area.GetMultiRecordCount());
  for (uint8_t i = 0; i < record_area.GetMultiRecordCount(); ++i) {
    EXPECT_TRUE(record_area.GetMultiRecord(i, &record));
    EXPECT_EQ(2 * i + kRecordCount, record.get_type());
  }
}

// Test that Fru::FillFromImage properly fails with a buffer that's too small.
TEST(FruTest, FruFillImageTooSmall) {
  // Some nonsense data that shouldn't be read.
  std::vector<unsigned char> fru_image;
  fru_image.push_back(0x00);
  fru_image.push_back(0x01);

  Fru fru;
  size_t size;
  absl::Status result =
      fru.FillFromImage(VectorFruImageSource(absl::MakeSpan(fru_image)), &size);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.code(), absl::StatusCode::kInternal);
  EXPECT_THAT(result.ToString(), HasSubstr("FRU image is too small"));
}

// Test that FruField::GetDataAsString decode the data as a string correctly.
TEST(FruTest, FruFieldGetDataAsString) {
  std::vector<unsigned char> dataBcdPlus({0x87, 0x6B, 0x9A, 0x3C, 0x01});
  FruField manufacturerBcdPlus =
      FruFieldFromRawData(absl::MakeSpan(dataBcdPlus), FruField::kTypeBcdPlus);
  EXPECT_EQ(std::string("876-9 3.01"), manufacturerBcdPlus.GetDataAsString());

  std::vector<unsigned char> data6BitAscii3Char({0x64, 0xC9, 0xB2});
  FruField manufacturer6BitAscii3Char = FruFieldFromRawData(
      absl::MakeSpan(data6BitAscii3Char), FruField::kType6BitAscii);
  EXPECT_EQ(std::string("DELL"), manufacturer6BitAscii3Char.GetDataAsString());

  std::vector<unsigned char> data6BitAscii4Char({0xA9, 0x4B, 0x97, 0x2C});
  FruField manufacturer6BitAscii4Char = FruFieldFromRawData(
      absl::MakeSpan(data6BitAscii4Char), FruField::kType6BitAscii);
  EXPECT_EQ(std::string("INTEL"), manufacturer6BitAscii4Char.GetDataAsString());

  std::vector<unsigned char> dataEnglishAscii({0x50, 0x6F, 0x77, 0x65, 0x72,
                                               0x45, 0x64, 0x67, 0x65, 0x20,
                                               0x52, 0x37, 0x32, 0x30});
  FruField product_name = FruFieldFromRawData(absl::MakeSpan(dataEnglishAscii),
                                              FruField::kTypeLanguageBased);
  EXPECT_EQ(std::string("PowerEdge R720"), product_name.GetDataAsString());
}

// Test that Fru::FillFromImage works with the golden image above.
TEST(FruTest, FruFillFromGoldenImage) {
  std::vector<unsigned char> image;
  for (size_t i = 0; i < ABSL_ARRAYSIZE(prefab_frudata); ++i) {
    image.push_back(prefab_frudata[i]);
  }

  Fru fru;
  size_t size;
  EXPECT_TRUE(
      fru.FillFromImage(VectorFruImageSource(absl::MakeSpan(image)), &size)
          .ok());

  // Above image has only a board area.
  EXPECT_FALSE(fru.GetChassisInfoArea());
  EXPECT_FALSE(fru.GetProductInfoArea());

  // Verify contents of BoardInfoArea.
  BoardInfoArea *bia = fru.GetBoardInfoArea();
  EXPECT_EQ(std::string("gmaker\0", 7), bia->manufacturer().GetDataAsString());
  EXPECT_EQ(std::string("gprods\0", 7), bia->product_name().GetDataAsString());
  EXPECT_EQ(std::string("0819000043\0", 11),
            bia->serial_number().GetDataAsString());
  EXPECT_EQ(std::string("GK08C3A066\0", 11),
            bia->part_number().GetDataAsString());
  EXPECT_EQ(TimeFromFruTime(0x631eb3), bia->manufacture_date());
}

// Test that Fru::FillFromImage works with the no area termination image above.
TEST(FruTest, FruFillFromNoAreaTerminationImage) {
  std::vector<unsigned char> image;
  for (size_t i = 0; i < ABSL_ARRAYSIZE(no_area_termination_frudata); ++i) {
    image.push_back(no_area_termination_frudata[i]);
  }

  // The 0xff padding at the end ensures that the end of the field area does
  // not coincide with the end of the EEPROM. It is the minimum number of FRU
  // chunks in padding to ensure that the missing area terminator causes
  // ReadFieldsFromFruAreaImage to overrun the field area.
  for (size_t i = 0; i < FruChunksToBytes(6); ++i) {
    image.push_back(0xff);
  }

  Fru fru;
  size_t size;
  EXPECT_TRUE(
      fru.FillFromImage(VectorFruImageSource(absl::MakeSpan(image)), &size)
          .ok());

  // Above image has only a board area.
  EXPECT_FALSE(fru.GetChassisInfoArea());
  EXPECT_FALSE(fru.GetProductInfoArea());

  // Verify contents of BoardInfoArea.
  BoardInfoArea *bia = fru.GetBoardInfoArea();
  EXPECT_EQ(std::string("gmaker\0", 7), bia->manufacturer().GetDataAsString());
  EXPECT_EQ(std::string("LeafLock\0", 9),
            bia->product_name().GetDataAsString());
  EXPECT_EQ(std::string("PFLQSH123900011\0", 16),
            bia->serial_number().GetDataAsString());
  EXPECT_EQ(std::string("42000311-01\0", 12),
            bia->part_number().GetDataAsString());
  EXPECT_EQ(TimeFromFruTime(0x867e31), bia->manufacture_date());
}

// Test that Fru::FillFromImage can read from a FRU image generated
// by Fru::GetImage.
TEST(FruTest, FruFillFromGetImage) {
  // Get the current time rounded to a 60-second increment
  time_t now = time(nullptr);
  time_t now_rounded = (now / 60) * 60;

  Fru fru;
  ChassisInfoArea *fru_cia = new ChassisInfoArea;
  fru_cia->set_type(ChassisInfoArea::kLaptop);
  fru_cia->set_part_number(FruFieldFromString("Part ABCXYA"));
  fru_cia->set_serial_number(FruFieldFromString("SN 0/123/456"));
  BoardInfoArea *fru_bia = new BoardInfoArea;
  fru_bia->set_manufacture_date(now_rounded);
  fru_bia->set_manufacturer(FruFieldFromString("Manufacturer Alpha"));
  fru_bia->set_product_name(FruFieldFromString("Product Beta"));
  fru_bia->set_serial_number(FruFieldFromString("SN AAABBBCCC"));
  fru_bia->set_part_number(FruFieldFromString("Part r0flr0fl"));
  fru_bia->set_file_id(FruFieldFromString("\5\6\7file"));
  ProductInfoArea *fru_pia = new ProductInfoArea;
  fru_pia->set_manufacturer(FruFieldFromString("Manufacturer Omicron"));
  fru_pia->set_product_name(FruFieldFromString("Delta"));
  fru_pia->set_part_number(FruFieldFromString("Epsilon"));
  fru_pia->set_product_version(FruFieldFromString("Gamma"));
  fru_pia->set_serial_number(FruFieldFromString("12512516"));
  fru_pia->set_asset_tag(FruFieldFromString("XXXAAB0691"));
  auto *fru_mra = new MultiRecordArea;
  MultiRecord record;
  std::vector<unsigned char> record_data = {};
  const size_t kRecordCount = 8;
  for (uint8_t i = 0; i < kRecordCount; ++i) {
    record.set_type(i);
    record_data.push_back(i);
    record.set_data(absl::MakeSpan(record_data));
    EXPECT_TRUE(fru_mra->AddMultiRecord(record));
  }

  fru.SetChassisInfoArea(fru_cia);
  fru.SetBoardInfoArea(fru_bia);
  fru.SetProductInfoArea(fru_pia);
  fru.SetMultiRecordArea(fru_mra);

  // Generate FRU image.
  std::vector<unsigned char> image;
  fru.GetImage(&image);

  // Read it back into a new FRU.
  Fru fru2;
  size_t size;
  EXPECT_TRUE(
      fru2.FillFromImage(VectorFruImageSource(absl::MakeSpan(image)), &size)
          .ok());

  // Verify all values are still the same.
  ChassisInfoArea *fru2_cia = fru2.GetChassisInfoArea();
  BoardInfoArea *fru2_bia = fru2.GetBoardInfoArea();
  ProductInfoArea *fru2_pia = fru2.GetProductInfoArea();
  MultiRecordArea *fru2_mra = fru2.GetMultiRecordArea();

  EXPECT_EQ(fru_cia->type(), fru2_cia->type());
  EXPECT_TRUE(fru_cia->part_number().Equals(fru2_cia->part_number()));
  EXPECT_TRUE(fru_cia->serial_number().Equals(fru2_cia->serial_number()));
  // Verify location of variable fields in the chassis area.
  FruArea::FieldLocation chassis_serial_number_location = {
      24, fru_cia->serial_number().GetData().size()};
  FruArea::FieldLocation chassis_checksum_location = {35, 1};
  std::vector<FruArea::FieldLocation> expected_chassis_fields_location = {
      chassis_serial_number_location, chassis_checksum_location};
  std::vector<FruArea::FieldLocation> actual_chassis_fields_location =
      fru2_cia->GetVariableFieldsLocation();
  EXPECT_EQ(expected_chassis_fields_location.size(),
            actual_chassis_fields_location.size());
  EXPECT_TRUE(std::is_permutation(actual_chassis_fields_location.begin(),
                                  actual_chassis_fields_location.end(),
                                  expected_chassis_fields_location.begin(),
                                  CompareFruFieldLocations));

  EXPECT_EQ(fru_bia->language_code(), fru2_bia->language_code());
  EXPECT_EQ(fru_bia->manufacture_date(), fru2_bia->manufacture_date());
  EXPECT_TRUE(fru_bia->manufacturer().Equals(fru2_bia->manufacturer()));
  EXPECT_TRUE(fru_bia->product_name().Equals(fru2_bia->product_name()));
  EXPECT_TRUE(fru_bia->serial_number().Equals(fru2_bia->serial_number()));
  EXPECT_TRUE(fru_bia->part_number().Equals(fru2_bia->part_number()));
  EXPECT_TRUE(fru_bia->file_id().Equals(fru2_bia->file_id()));
  // Verify location of variable fields in the board area.
  FruArea::FieldLocation board_serial_number_location = {
      79, fru_cia->serial_number().GetData().size()};
  FruArea::FieldLocation board_manufacture_date_location = {43, 3};
  FruArea::FieldLocation board_checksum_location = {112, 1};
  std::vector<FruArea::FieldLocation> expected_board_fields_location = {
      board_serial_number_location, board_manufacture_date_location,
      board_checksum_location};
  std::vector<FruArea::FieldLocation> actual_board_fields_location =
      fru2_bia->GetVariableFieldsLocation();
  EXPECT_EQ(expected_board_fields_location.size(),
            actual_board_fields_location.size());
  EXPECT_TRUE(std::is_permutation(
      actual_board_fields_location.begin(), actual_board_fields_location.end(),
      expected_board_fields_location.begin(), CompareFruFieldLocations));

  EXPECT_EQ(fru_pia->language_code(), fru2_pia->language_code());
  EXPECT_TRUE(fru_pia->manufacturer().Equals(fru2_pia->manufacturer()));
  EXPECT_TRUE(fru_pia->product_name().Equals(fru2_pia->product_name()));
  EXPECT_TRUE(fru_pia->part_number().Equals(fru2_pia->part_number()));
  EXPECT_TRUE(fru_pia->product_version().Equals(fru2_pia->product_version()));
  EXPECT_TRUE(fru_pia->serial_number().Equals(fru2_pia->serial_number()));
  EXPECT_TRUE(fru_pia->asset_tag().Equals(fru2_pia->asset_tag()));
  EXPECT_TRUE(fru_pia->file_id().Equals(fru2_pia->file_id()));
  // Verify location of variable fields in the product area.
  FruArea::FieldLocation product_serial_number_location = {
      165, fru_pia->serial_number().GetData().size()};
  FruArea::FieldLocation product_checksum_location = {184, 1};
  std::vector<FruArea::FieldLocation> expected_product_fields_location = {
      product_serial_number_location, product_checksum_location};
  std::vector<FruArea::FieldLocation> actual_product_fields_location =
      fru2_pia->GetVariableFieldsLocation();
  EXPECT_EQ(expected_product_fields_location.size(),
            actual_product_fields_location.size());
  EXPECT_TRUE(std::is_permutation(actual_product_fields_location.begin(),
                                  actual_product_fields_location.end(),
                                  expected_product_fields_location.begin(),
                                  CompareFruFieldLocations));
  // Verify multirecord area.
  EXPECT_EQ(kRecordCount, fru2_mra->GetMultiRecordCount());
  MultiRecord record2;
  std::vector<unsigned char> record2_data;
  for (uint8_t i = 0; i < kRecordCount; ++i) {
    EXPECT_TRUE(fru_mra->GetMultiRecord(i, &record));
    EXPECT_TRUE(fru2_mra->GetMultiRecord(i, &record2));
    record.get_data(&record_data);
    record2.get_data(&record2_data);
    EXPECT_EQ(record_data.size(), record2_data.size());
    EXPECT_EQ(record.get_type(), record2.get_type());
    for (size_t j = 0; j < record2_data.size(); ++j) {
      EXPECT_EQ(record_data[j], record2_data[j]);
    }
  }
  const size_t expected_fru_size = image.size();
  image.resize(image.size() * 2);

  absl::Status result =
      fru2.FillFromImage(VectorFruImageSource(absl::MakeSpan(image)), &size);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(size, expected_fru_size);
}

// Test that Fru::GetImage works as expected (generating the same bytes as
// our golden image above).
TEST(FruTest, FruGetImageGolden) {
  Fru fru;
  BoardInfoArea *bia = new BoardInfoArea;
  fru.SetBoardInfoArea(bia);  // Takes ownership.
  bia->set_manufacture_date(TimeFromFruTime(0x631eb3));
  bia->set_manufacturer(FruFieldFromString(std::string("gmaker\0", 7)));
  bia->set_product_name(FruFieldFromString(std::string("gprods\0", 7)));
  bia->set_serial_number(FruFieldFromString(std::string("0819000043\0", 11)));
  bia->set_part_number(FruFieldFromString(std::string("GK08C3A066\0", 11)));

  std::vector<unsigned char> image;
  fru.GetImage(&image);

  EXPECT_EQ(ABSL_ARRAYSIZE(prefab_frudata), image.size());
  for (size_t i = 0; i < image.size(); ++i) {
    EXPECT_EQ(prefab_frudata[i], image[i]);
  }
}

// Test that custom fields can be filled and parsed correctly.
TEST(FruTest, FillAndReadFruCustomField) {
  Fru fru;
  ChassisInfoArea *cia = new ChassisInfoArea;
  fru.SetChassisInfoArea(cia);
  BoardInfoArea *bia = new BoardInfoArea;
  fru.SetBoardInfoArea(bia);
  ProductInfoArea *pia = new ProductInfoArea;
  fru.SetProductInfoArea(pia);

  // Fills CIA custom fields with plain strings.
  cia->set_custom_fields({FruFieldFromString("A"), FruFieldFromString("ABC")});
  // Fills BIA custom fields with unknown field type.
  bia->set_custom_fields({FruFieldFromString(absl::StrFormat(
      "%c%s", CustomFieldType::kUnknownCustomFieldType, "AB"))});
  // Fills PIA custom fields with valid fields.
  pia->set_custom_fields({FruFieldFromString(absl::StrFormat(
                              "%c%s", CustomFieldType::kFabId, "ABCD")),
                          FruFieldFromString(absl::StrFormat(
                              "%c%s", CustomFieldType::kFirmwareId, "PL00"))});

  FruCustomField fru_custom_field;
  EXPECT_FALSE(
      DecodeCustomField(cia->custom_fields()[0], &fru_custom_field).ok());
  EXPECT_FALSE(
      DecodeCustomField(cia->custom_fields()[1], &fru_custom_field).ok());

  std::string value;
  EXPECT_FALSE(cia->GetCustomField(CustomFieldType::kFabId, &value).ok());
  EXPECT_FALSE(cia->GetCustomField(CustomFieldType::kFirmwareId, &value).ok());

  ASSERT_TRUE(
      DecodeCustomField(bia->custom_fields()[0], &fru_custom_field).ok());

  EXPECT_EQ(fru_custom_field.type, CustomFieldType::kUnknownCustomFieldType);
  EXPECT_EQ(fru_custom_field.value, "AB");
  EXPECT_FALSE(bia->GetCustomField(CustomFieldType::kFabId, &value).ok());
  EXPECT_FALSE(bia->GetCustomField(CustomFieldType::kFirmwareId, &value).ok());

  ASSERT_TRUE(
      DecodeCustomField(pia->custom_fields()[0], &fru_custom_field).ok());
  EXPECT_EQ(fru_custom_field.type, CustomFieldType::kFabId);
  EXPECT_EQ(fru_custom_field.value, "ABCD");

  ASSERT_TRUE(
      DecodeCustomField(pia->custom_fields()[1], &fru_custom_field).ok());
  EXPECT_EQ(fru_custom_field.type, CustomFieldType::kFirmwareId);
  EXPECT_EQ(fru_custom_field.value, "PL00");

  ASSERT_TRUE(pia->GetCustomField(CustomFieldType::kFabId, &value).ok());
  EXPECT_EQ(value, "ABCD");
  EXPECT_TRUE(pia->GetCustomField(CustomFieldType::kFirmwareId, &value).ok());
  EXPECT_EQ(value, "PL00");
}

TEST(FruTest, ValidateFruCommonHeader) {
  std::vector<uint8_t> valid_header_bytes(CommonHeader::IntrinsicSizeInBytes());
  auto valid_header = MakeCommonHeaderView(&valid_header_bytes);

  valid_header.format_version().Write(kFormatVersion);
  valid_header.internal_use_area_starting_offset().Write(42);
  valid_header.chassis_info_area_starting_offset().Write(12);
  valid_header.board_info_area_starting_offset().Write(34);
  uint8_t checksum = -FruChecksumFromBytes(valid_header_bytes.data(), 0, 7);

  valid_header.checksum().Write(checksum);
  EXPECT_TRUE(Fru::ValidateFruCommonHeader(valid_header_bytes));

  std::vector<uint8_t> invalid_version_bytes(valid_header_bytes);
  auto invalid_version = MakeCommonHeaderView(&invalid_version_bytes);

  invalid_version.format_version().Write(kFormatVersion + 1);
  checksum = -FruChecksumFromBytes(invalid_version_bytes.data(), 0, 7);
  invalid_version.checksum().Write(checksum);

  EXPECT_FALSE(Fru::ValidateFruCommonHeader(invalid_version_bytes));

  std::vector<uint8_t> invalid_checksum_bytes(valid_header_bytes);
  auto invalid_checksum = MakeCommonHeaderView(&invalid_checksum_bytes);

  checksum = invalid_checksum.checksum().Read();
  invalid_checksum.checksum().Write(checksum + 1);

  EXPECT_FALSE(Fru::ValidateFruCommonHeader(invalid_checksum_bytes));
}

}  // namespace
}  // namespace ecclesia
