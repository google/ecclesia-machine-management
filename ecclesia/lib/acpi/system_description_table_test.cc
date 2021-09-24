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

#include "ecclesia/lib/acpi/system_description_table.h"

#include <sys/stat.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/base/macros.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/acpi/system_description_table.emb.h"
#include "ecclesia/lib/codec/endian.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/lib/testing/status.h"
#include "runtime/cpp/emboss_cpp_util.h"
#include "runtime/cpp/emboss_prelude.h"

namespace ecclesia {

namespace {

constexpr absl::string_view kTestSysDescTablePath =
    "lib/acpi/test_data/sys_firmware_acpi_tables_SRAT";
constexpr absl::string_view kTestSysDescTableSmallPath =
    "lib/acpi/test_data/sys_firmware_acpi_tables_SRAT_small";
constexpr absl::string_view kTestSysDescTableBadLengthPath =
    "lib/acpi/test_data/sys_firmware_acpi_tables_SRAT_bad_length";
constexpr absl::string_view kTestNonExistantFile =
    "lib/acpi/test_data/THIS_DOES_NOT_EXIST";

std::string GetValidHeaderData() {
  std::string header_data(SystemDescriptionTableHeaderView::SizeInBytes(), 0);
  auto header_view_ = MakeSystemDescriptionTableHeaderView(header_data.data(),
                                                           header_data.size());
  header_view_.signature().Write(('T' << 0) | ('E' << 8) | ('S' << 16) |
                                 ('T' << 24));
  header_view_.length().Write(header_data.size());
  header_view_.revision().Write(2);
  header_view_.checksum().Write(0xf3);
  constexpr absl::string_view kOemId = "GOOGLE";
  constexpr absl::string_view kOemTableId = "TEST1234";
  std::copy(kOemId.begin(), kOemId.end(),
            header_view_.oem_id().BackingStorage().data());
  std::copy(kOemTableId.begin(), kOemTableId.end(),
            header_view_.oem_table_id().BackingStorage().data());
  header_view_.oem_revision().Write(0x89abcdef);
  header_view_.creator_id().Write(0x01234567);
  header_view_.creator_revision().Write(0xabadcafe);
  return header_data;
}

// Test checksum calculation of a structure with a valid checksum.
TEST(SystemDescriptionTableTest, CalculateChecksumValid) {
  std::string header_data = GetValidHeaderData();
  auto table = SystemDescriptionTable::CreateTableFromData(
      header_data.data(), SystemDescriptionTable::kHeaderSize);
  EXPECT_EQ(0, table->CalculateChecksum());
}

// Test checksum calculation of a structure with an invalid checksum.
TEST(SystemDescriptionTableTest, CalculateChecksumInvalid) {
  std::string header_data = GetValidHeaderData();
  auto header_view_ = MakeSystemDescriptionTableHeaderView(header_data.data(),
                                                           header_data.size());
  header_view_.checksum().Write(0x22);
  auto table = SystemDescriptionTable::CreateTableFromData(
      header_data.data(), SystemDescriptionTable::kHeaderSize);
  EXPECT_NE(0, table->CalculateChecksum());
}

// Try reading a system description table from a non-existent file.
TEST(SystemDescriptionTableTest, ReadFromFileNotFound) {
  auto table = SystemDescriptionTable::ReadFromFile(
      GetTestDataDependencyPath(kTestNonExistantFile));
  EXPECT_EQ(table.status().code(), absl::StatusCode::kNotFound)
      << table.status();
}

// Try reading a system description table from a file that is smaller than the
// header structure.
TEST(SystemDescriptionTableTest, ReadFromFileTooSmall) {
  auto table = SystemDescriptionTable::ReadFromFile(
      GetTestDataDependencyPath(kTestSysDescTableSmallPath));
  EXPECT_THAT(table, IsStatusInternal());
}

// Try reading a system description table that specifies a size smaller than
// the file.
TEST(SystemDescriptionTableTest, ReadFromFileBadLength) {
  const std::string sys_desc_table_bad_length_path =
      GetTestDataDependencyPath(kTestSysDescTableBadLengthPath);
  auto table =
      SystemDescriptionTable::ReadFromFile(sys_desc_table_bad_length_path);

  ASSERT_THAT(table, IsOk());

  struct stat stat_buf;
  EXPECT_EQ(0, stat(sys_desc_table_bad_length_path.c_str(), &stat_buf));
  EXPECT_EQ(stat_buf.st_size, table.value()->GetHeaderView().length().Read());
}

// Ensure that it's possible to read a valid system description table from a
// file.
TEST(SystemDescriptionTableTest, ReadFromFileSuccess) {
  auto table = SystemDescriptionTable::ReadFromFile(
      GetTestDataDependencyPath(kTestSysDescTablePath));
  ASSERT_THAT(table, IsOk());
}

// For creating a SraHeader struct and Emboss view, with backing data stored in
// the factory.
class SraHeaderFactory {
 public:
  SraHeaderFactory(uint8_t type, uint8_t length) {
    auto header_view_writeable = MakeSraHeaderView(data_, sizeof(data_));
    header_view_writeable.struct_type().Write(type);
    header_view_writeable.length().Write(length);
    header_view_ = header_view_writeable;
  }

  const SraHeaderView& header_view() const { return header_view_; }

 private:
  char data_[SraHeaderView::SizeInBytes()];
  SraHeaderView header_view_;
};

// Ensure validation of a valid static resource allocation succeeds.
TEST(SraHeaderTest, ValidateMaximumSize) {
  SraHeaderFactory factory(0, 10);
  EXPECT_TRUE(
      SraHeaderDescriptor::ValidateMaximumSize(factory.header_view(), 10));
}

// Ensure validation of a invalid static resource allocation fails.
TEST(SraHeaderTest, ValidateHeaderSizeInvalid) {
  SraHeaderFactory factory(0, 10);
  EXPECT_FALSE(
      SraHeaderDescriptor::ValidateMaximumSize(factory.header_view(), 9));
}

// Test the validation of a valid static resource allocation entry.
TEST(SraHeaderTest, ValidateValid) {
  SraHeaderFactory factory(3, SraHeaderView::SizeInBytes() + 6);
  EXPECT_TRUE(SraHeaderDescriptor::Validate(factory.header_view(), 3, 8,
                                            "Custom Google structure"));
}

// Test the validation of a static resource allocation entry with a bad size.
TEST(SraHeaderTest, ValidateInvalidSize) {
  SraHeaderFactory factory(3, SraHeaderView::SizeInBytes() + 6);
  EXPECT_FALSE(SraHeaderDescriptor::Validate(factory.header_view(), 3, 10,
                                             "Custom Google structure"));
}

// Test the validation of a static resource allocation entry with a bad type.
TEST(SraHeaderTest, ValidateInvalidType) {
  SraHeaderFactory factory(3, SraHeaderView::SizeInBytes() + 6);
  EXPECT_FALSE(SraHeaderDescriptor::Validate(
      factory.header_view(), 2, 8, "Another custom Google structure"));
}

class SystemDescriptionTableReaderTest : public ::testing::Test {
 public:
  // Type used in SraTest struct.
  static constexpr int kSraTestHeaderType = 0x40;

  // Sub-class SystemDescriptionTableReader so that it's possible to test it.
  class Reader : public SystemDescriptionTableReader<SraHeaderDescriptor> {
   public:
    Reader(const char* data, size_t size)
        : SystemDescriptionTableReader(data, size) {}
    explicit Reader(const char* data)
        : SystemDescriptionTableReader(
              data, SystemDescriptionTableHeaderView::SizeInBytes()) {}

    bool ValidateSignature() const override { return true; }
    bool ValidateRevision() const override { return true; }
  };

 protected:
  // Setup the test system description table structure.
  void SetUp() override {
    // Firstly, setup pointers into the buffer used to store the table.
    const size_t header_size = SystemDescriptionTableHeaderView::SizeInBytes();

    // We need separate read-only and writeable views of SraHeaders. The
    // read-only views are required when calling GetNextSraStructure().
    SraHeaderWriter sra_headers_writeable[4];
    static_assert(ABSL_ARRAYSIZE(sra_headers_) ==
                  ABSL_ARRAYSIZE(sra_headers_writeable));

    for (size_t i = 0; i < ABSL_ARRAYSIZE(sra_headers_); ++i) {
      sra_headers_writeable[i] = MakeSraHeaderView(
          table_data_ + header_size + SraHeaderView::SizeInBytes() * i,
          SraHeaderView::SizeInBytes());
      sra_headers_[i] = sra_headers_writeable[i];
    }
    for (size_t i = 0; i < ABSL_ARRAYSIZE(sra_test_headers_); ++i) {
      sra_test_headers_[i] = MakeSraTestView(
          table_data_ + header_size +
              SraHeaderView::SizeInBytes() * ABSL_ARRAYSIZE(sra_headers_) +
              SraTestView::SizeInBytes() * i,
          SraTestView::SizeInBytes());
    }

    memset(table_data_, 0, sizeof(table_data_));

    // Initialize the table header.
    header_view_.signature().Write(0xdeadbeef);
    header_view_.length().Write(sizeof(table_data_));
    header_view_.revision().Write(2);
    header_view_.checksum().Write(0);
    std::copy(kOemId.begin(), kOemId.end(),
              header_view_.oem_id().BackingStorage().data());
    std::copy(kOemTableId.begin(), kOemTableId.end(),
              header_view_.oem_table_id().BackingStorage().data());
    header_view_.oem_revision().Write(kOemRevision);
    header_view_.creator_id().Write(LittleEndian::Load32(kCreatorId.data()));
    static_assert(
        kCreatorId.size() >= sizeof(header_view_.creator_id().Read()));
    header_view_.creator_revision().Write(kCreatorRevision);

    // Initialize the empty static resource allocation structures.
    for (size_t i = 0; i < ABSL_ARRAYSIZE(sra_headers_); i++) {
      sra_headers_writeable[i].struct_type().Write(0x10 + i);
      sra_headers_writeable[i].length().Write(SraHeaderView::SizeInBytes());
    }

    // Initialize all test static resource allocation structures.
    for (size_t i = 0; i < ABSL_ARRAYSIZE(sra_test_headers_); i++) {
      sra_test_headers_[i].header().struct_type().Write(kSraTestHeaderType);
      sra_test_headers_[i].header().length().Write(SraTestView::SizeInBytes());
      sra_test_headers_[i].data().Write(0x40 + i);
      sra_test_headers_[i].flags().Write(0);
    }

    // Calculate the table checksum.
    auto table = SystemDescriptionTable::CreateTableFromData(
        table_data_, sizeof(table_data_));
    header_view_.checksum().Write(table->CalculateChecksum());
  }

  static SraTestView SraHeaderToSraTest(const SraHeaderView& header_view) {
    Check(header_view.BackingStorage().SizeInBytes() >=
              SraTestView::SizeInBytes(),
          "sufficient backing bytes")
        << absl::StrFormat(
               "Not enough bytes backing header view, need at least"
               " %u, actual %u",
               SraTestView::SizeInBytes(),
               header_view.BackingStorage().SizeInBytes());
    return SraTestView(header_view.BackingStorage());
  }

  SystemDescriptionTableHeaderWriter header_view_ =
      MakeSystemDescriptionTableHeaderView(
          reinterpret_cast<char*>(&table_data_[0]),
          SystemDescriptionTableHeaderView::SizeInBytes());

  SraHeaderView sra_headers_[4];
  SraTestWriter sra_test_headers_[3];
  char table_data_[SystemDescriptionTableHeaderView::SizeInBytes() +
                   SraHeaderView::SizeInBytes() * ABSL_ARRAYSIZE(sra_headers_) +
                   SraTestView::SizeInBytes() *
                       ABSL_ARRAYSIZE(sra_test_headers_)];

  // Default values for the test table header.
  static constexpr absl::string_view kOemId = "GOOGLE";
  static constexpr absl::string_view kOemTableId = "TEST1234";
  static constexpr uint32_t kOemRevision = 0xabadcafe;
  static constexpr absl::string_view kCreatorId = "AMAN";
  static constexpr uint32_t kCreatorRevision = 47;
  static constexpr uint8_t kSraTestEnabledFlag = 0x08;
};

// Test the validation of a valid table.
TEST_F(SystemDescriptionTableReaderTest, Validate) {
  auto header_data = GetValidHeaderData();
  Reader reader(header_data.data(), header_data.size());
  EXPECT_TRUE(reader.Validate());
}

// Ensure the checksum of a valid table is 0.
TEST_F(SystemDescriptionTableReaderTest, CalculateChecksum) {
  auto header_data = GetValidHeaderData();
  Reader reader(header_data.data(), header_data.size());
  EXPECT_EQ(0, reader.CalculateChecksum());
}

// Retrieve the first static allocation structure from a valid table.
TEST_F(SystemDescriptionTableReaderTest, GetFirstSraStructureValidTable) {
  Reader reader(table_data_);
  auto sra_structure = reader.GetFirstSraStructure();
  ASSERT_TRUE(sra_structure);
  EXPECT_TRUE(sra_structure->Equals(sra_headers_[0]));
}

// Try to retrieve the first static resource allocation entry structure from a
// invalid table.
TEST_F(SystemDescriptionTableReaderTest, GetFirstSraStructureInvalidTable) {
  header_view_.length().Write(header_view_.SizeInBytes() + 1);
  Reader reader(table_data_);
  EXPECT_FALSE(reader.GetFirstSraStructure());
}

// Test the retrieval of the next static resource allocation entry structure
// from a valid table.
TEST_F(SystemDescriptionTableReaderTest, GetNextSraStructureValid) {
  Reader reader(table_data_);
  auto sra_structure = reader.GetNextSraStructure(sra_headers_[0]);
  ASSERT_TRUE(sra_structure);
  EXPECT_TRUE(sra_structure->Equals(sra_headers_[1]));
}

// Test the retrieval of the next static resource allocation entry structure
// from a valid table given an invalid resource allocation structure.
TEST_F(SystemDescriptionTableReaderTest, GetNextSraStructureInvalidSra) {
  SraHeaderFactory factory(0xab, SraHeaderView::SizeInBytes());
  Reader reader(table_data_);
  EXPECT_FALSE(reader.GetNextSraStructure(factory.header_view()));
}

// Test the retrieval of the next static resource allocation entry structure
// from a table where the given entry exceeds the bounds of the table.
TEST_F(SystemDescriptionTableReaderTest, GetNextSraStructureOutOfBounds) {
  header_view_.length().Write(header_view_.SizeInBytes() + 1);
  Reader reader(table_data_);
  EXPECT_FALSE(reader.GetNextSraStructure(sra_headers_[0]));
}

// Test the retrieval of the next static resource allocation entry structure
// where the next structure exceeds the bounds of the SRAT.
TEST_F(SystemDescriptionTableReaderTest,
       GetNextSraStructureNextSraOutOfBounds) {
  header_view_.length().Write(header_view_.SizeInBytes() +
                              SraHeaderView::SizeInBytes() + 1);
  Reader reader(table_data_);
  EXPECT_FALSE(reader.GetNextSraStructure(sra_headers_[0]));
}

// Retrieve all static resource allocation entry structures.
TEST_F(SystemDescriptionTableReaderTest, GetSraStructures) {
  Reader reader(table_data_);
  SystemDescriptionTableReader<SraHeaderDescriptor>::SraHeaderFilter filter;
  std::vector<SraHeaderView> sra_structures = reader.GetSraStructures(filter);
  ASSERT_EQ(sra_structures.size(),
            ABSL_ARRAYSIZE(sra_headers_) + ABSL_ARRAYSIZE(sra_test_headers_));
  EXPECT_TRUE(sra_structures[0].Equals(sra_headers_[0]));
  EXPECT_TRUE(sra_structures[1].Equals(sra_headers_[1]));
  EXPECT_TRUE(sra_structures[2].Equals(sra_headers_[2]));
  EXPECT_TRUE(sra_structures[3].Equals(sra_headers_[3]));
  EXPECT_TRUE(
      SraHeaderToSraTest(sra_structures[4]).Equals(sra_test_headers_[0]));
  EXPECT_TRUE(
      SraHeaderToSraTest(sra_structures[5]).Equals(sra_test_headers_[1]));
  EXPECT_TRUE(
      SraHeaderToSraTest(sra_structures[6]).Equals(sra_test_headers_[2]));
}

// Test retrieving static resource allocation entries by type.
TEST_F(SystemDescriptionTableReaderTest, GetSraStructuresByType) {
  Reader reader(table_data_);
  SystemDescriptionTableReader<SraHeaderDescriptor>::
      SraHeaderFilterByTypeAndMinSize filter(kSraTestHeaderType,
                                             SraTestView::SizeInBytes());
  std::vector<SraTestView> sra_structures =
      reader.GetSraStructures<SraTestView>(filter);
  EXPECT_EQ(sra_structures.size(), 3);
  EXPECT_TRUE(sra_structures[0].Equals(sra_test_headers_[0]));
  EXPECT_TRUE(sra_structures[1].Equals(sra_test_headers_[1]));
  EXPECT_TRUE(sra_structures[2].Equals(sra_test_headers_[2]));
}

// Test retrieving enabled static resource allocation structures of a specific
// type.
TEST_F(SystemDescriptionTableReaderTest, GetEnabledSraStructuresByType) {
  sra_test_headers_[0].flags().Write(sra_test_headers_[0].flags().Read() |
                                     kSraTestEnabledFlag);
  sra_test_headers_[2].flags().Write(sra_test_headers_[2].flags().Read() |
                                     kSraTestEnabledFlag);

  Reader reader(table_data_);
  SystemDescriptionTableReader<SraHeaderDescriptor>::SraHeaderFilterByEnabled<
      SraTestView>
      filter(kSraTestHeaderType, SraTestView::SizeInBytes(),
             kSraTestEnabledFlag);
  std::vector<SraTestView> sra_structures =
      reader.GetSraStructures<SraTestView>(filter);
  ASSERT_EQ(sra_structures.size(), 2);
  EXPECT_TRUE(sra_structures[0].Equals(sra_test_headers_[0]));
  EXPECT_TRUE(sra_structures[1].Equals(sra_test_headers_[2]));
}

// Test retrieving static resource allocation entries by type using the
// GetSraStructuresByTypeAndMinSize() template function.
TEST_F(SystemDescriptionTableReaderTest, GetSraStructuresByTypeAndMinSize) {
  Reader reader(table_data_);
  std::vector<SraTestView> sra_structures =
      reader
          .GetSraStructuresByTypeAndMinSize<SraTestView, kSraTestHeaderType>();
  ASSERT_EQ(sra_structures.size(), 3);
  EXPECT_TRUE(sra_structures[0].Equals(sra_test_headers_[0]));
  EXPECT_TRUE(sra_structures[1].Equals(sra_test_headers_[1]));
  EXPECT_TRUE(sra_structures[2].Equals(sra_test_headers_[2]));
}

// Test retrieving static resource allocation entries by type using the
// GetSraStructuresByType() template function.
TEST_F(SystemDescriptionTableReaderTest, GetSraStructuresByTypeTemplate) {
  Reader reader(table_data_);
  std::vector<SraTestView> sra_structures =
      reader.GetSraStructuresByType<SraTestView, kSraTestHeaderType>(
          false, kSraTestEnabledFlag);
  ASSERT_EQ(sra_structures.size(), 3);
  EXPECT_TRUE(sra_structures[0].Equals(sra_test_headers_[0]));
  EXPECT_TRUE(sra_structures[1].Equals(sra_test_headers_[1]));
  EXPECT_TRUE(sra_structures[2].Equals(sra_test_headers_[2]));
}

// Test retrieving enabled static resource allocation structures of a specific
// type using the GetSraStructuresByType() template function.
TEST_F(SystemDescriptionTableReaderTest,
       GetEnabledSraStructuresByTypeTemplate) {
  sra_test_headers_[0].flags().Write(sra_test_headers_[0].flags().Read() |
                                     kSraTestEnabledFlag);
  sra_test_headers_[2].flags().Write(sra_test_headers_[2].flags().Read() |
                                     kSraTestEnabledFlag);

  Reader reader(table_data_);
  std::vector<SraTestView> sra_structures =
      reader.GetSraStructuresByType<SraTestView, kSraTestHeaderType>(
          true, kSraTestEnabledFlag);
  ASSERT_EQ(sra_structures.size(), 2);
  EXPECT_TRUE(sra_structures[0].Equals(sra_test_headers_[0]));
  EXPECT_TRUE(sra_structures[1].Equals(sra_test_headers_[2]));
}

TEST_F(SystemDescriptionTableReaderTest, ValidateSignature) {
  std::string header_data = GetValidHeaderData();
  Reader reader(header_data.data(), header_data.size());
  EXPECT_TRUE(reader.ValidateSignature());
}

TEST_F(SystemDescriptionTableReaderTest, ValidateRevision) {
  std::string header_data = GetValidHeaderData();
  Reader reader(header_data.data(), header_data.size());
  EXPECT_TRUE(reader.ValidateRevision());
}

}  // namespace

}  // namespace ecclesia
