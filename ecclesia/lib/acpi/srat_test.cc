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

#include "ecclesia/lib/acpi/srat.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/base/macros.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/acpi/srat.emb.h"
#include "ecclesia/lib/acpi/system_description_table.emb.h"
#include "ecclesia/lib/acpi/system_description_table.h"
#include "ecclesia/lib/codec/endian.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/testing/status.h"
#include "runtime/cpp/emboss_cpp_util.h"
#include "runtime/cpp/emboss_prelude.h"

namespace ecclesia {

namespace {

class SratTest : public ::testing::Test {
 public:
  // Address range of a SratMemoryAffinity structure.
  struct MemoryRange {
    uint64_t base_address;
    uint64_t length;
  };

  // Ensure the SRAT static resource allocation entries referenced by
  // sra_header are SratMemoryAffinity structures that are in the
  // proximity_domain and match memory_ranges. Returns a vector containing the
  // remainder of the structures following the validated structures.
  std::vector<SratMemoryAffinityView> ValidateMemoryAffinityStructures(
      const std::vector<SratMemoryAffinityView> &memory_affinities,
      const uint32_t proximity_domain,
      const std::vector<MemoryRange> &memory_ranges) {
    EXPECT_GE(memory_affinities.size(), memory_ranges.size());
    for (size_t i = 0; i < memory_ranges.size(); ++i) {
      const uint64_t base_address = memory_ranges[i].base_address;
      const uint64_t length = memory_ranges[i].length;

      const auto &memory_affinity = memory_affinities[i];
      EXPECT_EQ(SRAT_SRA_HEADER_MEMORY_AFFINITY,
                memory_affinity.header().struct_type().Read());
      EXPECT_EQ(SratMemoryAffinityView::SizeInBytes(),
                memory_affinity.header().length().Read());

      EXPECT_GE(memory_affinity.BackingStorage().SizeInBytes(),
                SratMemoryAffinityView::SizeInBytes());
      EXPECT_EQ(proximity_domain, memory_affinity.proximity_domain().Read());
      EXPECT_EQ(base_address, memory_affinity.base_address().Read());
      EXPECT_EQ(length, memory_affinity.length().Read());
      EXPECT_EQ(SRAT_MEMORY_AFFINITY_FLAGS_ENABLED,
                memory_affinity.flags().Read());
    }
    return std::vector<SratMemoryAffinityView>(
        memory_affinities.begin() + memory_ranges.size(),
        memory_affinities.end());
  }

  // Ensure the SRAT static resource allocation entries referenced by
  // sra_header are SratProcessorApicAffinity structures that are in the
  // proximity_domain and match apic_ids. Returns a vector containing the
  // remainder of the structures following the validated structures.
  std::vector<SratProcessorApicAffinityView>
  ValidateProcessorApicAffinityStructures(
      const std::vector<SratProcessorApicAffinityView>
          &processor_apic_affinities,
      const uint32_t proximity_domain, const std::vector<uint8_t> &apic_ids) {
    const uint8_t proximity_domain_bytes[] = {
        static_cast<uint8_t>(proximity_domain),
        static_cast<uint8_t>(proximity_domain >> 8),
        static_cast<uint8_t>(proximity_domain >> 16),
        static_cast<uint8_t>(proximity_domain >> 24)};

    EXPECT_GE(processor_apic_affinities.size(), apic_ids.size());
    for (size_t i = 0; i < apic_ids.size(); ++i) {
      const auto &processor_apic_affinity = processor_apic_affinities[i];
      EXPECT_EQ(SRAT_SRA_HEADER_PROCESSOR_APIC_AFFINITY,
                processor_apic_affinity.header().struct_type().Read());
      EXPECT_EQ(SratProcessorApicAffinityView::SizeInBytes(),
                processor_apic_affinity.header().length().Read());

      EXPECT_EQ(proximity_domain_bytes[0],
                processor_apic_affinity.proximity_domain_7_0().Read());
      EXPECT_EQ(apic_ids[i], processor_apic_affinity.apic_id().Read());
      EXPECT_EQ(SRAT_PROCESSOR_APIC_AFFINITY_FLAGS_ENABLED,
                processor_apic_affinity.flags().Read());
      EXPECT_EQ(0, processor_apic_affinity.sapic_eid().Read());
      EXPECT_EQ(proximity_domain_bytes[1],
                processor_apic_affinity.proximity_domain_31_8()[0].Read());
      EXPECT_EQ(proximity_domain_bytes[2],
                processor_apic_affinity.proximity_domain_31_8()[1].Read());
      EXPECT_EQ(proximity_domain_bytes[3],
                processor_apic_affinity.proximity_domain_31_8()[2].Read());
    }
    return std::vector<SratProcessorApicAffinityView>(
        processor_apic_affinities.begin() + apic_ids.size(),
        processor_apic_affinities.end());
  }

  // Path to a valid SRAT file with static resource allocation entries.
  static constexpr absl::string_view kTestSysfsAcpiSratPath =
      "lib/acpi/test_data/sys_firmware_acpi_tables_SRAT";
};

// Validate a valid SRAT signature.
TEST_F(SratTest, ValidateSignatureValid) {
  char srat_data[SratHeaderView::SizeInBytes()];
  memset(srat_data, 0xab, sizeof(srat_data));
  auto srat_header = MakeSratHeaderView(srat_data, sizeof(srat_data));
  srat_header.header().signature().Write(Srat::kAcpiSratSignature);
  SratReader reader(srat_header);
  EXPECT_TRUE(reader.ValidateSignature());
}

// Ensure SRAT signature validation fails with an invalid signature.
TEST_F(SratTest, ValidateSignatureInvalid) {
  char srat_data[SratHeaderView::SizeInBytes()];
  memset(srat_data, 0xab, sizeof(srat_data));
  auto srat_header = MakeSratHeaderView(srat_data, sizeof(srat_data));
  SratReader reader(srat_header);
  EXPECT_FALSE(reader.ValidateSignature());
}

// Validate a valid SRAT revision.
TEST_F(SratTest, ValidateRevisionValid) {
  char srat_data[SratHeaderView::SizeInBytes()];
  memset(srat_data, 0xab, sizeof(srat_data));
  auto srat_header = MakeSratHeaderView(srat_data, sizeof(srat_data));
  srat_header.header().revision().Write(2);
  SratReader reader(srat_header);
  EXPECT_TRUE(reader.ValidateRevision());
}

// Ensure SRAT revision validation fails with an invalid revision.
TEST_F(SratTest, ValidateRevisionInvalid) {
  char srat_data[SratHeaderView::SizeInBytes()];
  memset(srat_data, 0xab, sizeof(srat_data));
  auto srat_header = MakeSratHeaderView(srat_data, sizeof(srat_data));
  SratReader reader(srat_header);
  EXPECT_FALSE(reader.ValidateRevision());
}

// Ensure that it's possible to read a valid SRAT from a file.
TEST_F(SratTest, ReadSratFromFileSuccess) {
  auto table = SystemDescriptionTable::ReadFromFile(
      GetTestDataDependencyPath(kTestSysfsAcpiSratPath));
  ASSERT_THAT(table, IsOk());

  Srat srat(std::move(table.value()));
  const SratHeaderView &srat_header = srat.GetSratHeader();

  // Validate SRAT contents
  const std::vector<MemoryRange> domain0_memory_ranges = {
      {0x0000000000000000ULL, 0x00000000000a0000ULL},
      {0x0000000000100000ULL, 0x00000000bff00000ULL},
      {0x0000000100000000ULL, 0x0000000740000000ULL},
  };
  const std::vector<MemoryRange> domain1_memory_ranges = {
      {0x0000000840000000ULL, 0x0000000800000000ULL},
  };
  const std::vector<MemoryRange> domain2_memory_ranges = {
      {0x0000001040000000ULL, 0x0000000800000000ULL},
  };
  const std::vector<MemoryRange> domain3_memory_ranges = {
      {0x0000001840000000ULL, 0x0000000800000000ULL},
  };
  const std::vector<uint8_t> domain0_apic_ids = {0x10, 0x11, 0x12,
                                                 0x13, 0x14, 0x15};
  const std::vector<uint8_t> domain1_apic_ids = {0x16, 0x17, 0x18,
                                                 0x19, 0x1a, 0x1b};
  const std::vector<uint8_t> domain2_apic_ids = {0x26, 0x27, 0x28,
                                                 0x29, 0x2a, 0x2b};
  const std::vector<uint8_t> domain3_apic_ids = {0x20, 0x21, 0x22,
                                                 0x23, 0x24, 0x25};

  const struct {
    uint32_t proximity_domain;
    const std::vector<MemoryRange> memory_ranges;
    size_t number_of_memory_ranges;
    const std::vector<uint8_t> apic_ids;
    size_t number_of_apic_ids;
  } expected_sra_structures[] = {
      {0, domain0_memory_ranges, domain0_memory_ranges.size(), domain0_apic_ids,
       domain0_apic_ids.size()},
      {1, domain1_memory_ranges, domain1_memory_ranges.size(), domain1_apic_ids,
       domain1_apic_ids.size()},
      {2, domain2_memory_ranges, domain2_memory_ranges.size(), domain2_apic_ids,
       domain2_apic_ids.size()},
      {3, domain3_memory_ranges, domain3_memory_ranges.size(), domain3_apic_ids,
       domain3_apic_ids.size()},
  };

  SratReader reader(srat_header);
  std::vector<SratMemoryAffinityView> memory_affinity_structs =
      reader.GetSraStructuresByTypeAndMinSize<
          SratMemoryAffinityView, SRAT_SRA_HEADER_MEMORY_AFFINITY>();
  std::vector<SratProcessorApicAffinityView> apic_affinity_structs =
      reader.GetSraStructuresByTypeAndMinSize<
          SratProcessorApicAffinityView,
          SRAT_SRA_HEADER_PROCESSOR_APIC_AFFINITY>();

  for (size_t i = 0; i < ABSL_ARRAYSIZE(expected_sra_structures); ++i) {
    memory_affinity_structs = ValidateMemoryAffinityStructures(
        memory_affinity_structs, i, expected_sra_structures[i].memory_ranges);

    apic_affinity_structs = ValidateProcessorApicAffinityStructures(
        apic_affinity_structs, i, expected_sra_structures[i].apic_ids);
  }
}

class SratReaderTest : public ::testing::Test {
 public:
  SratReaderTest() {}

  void SetUp() override {
    SetupSratStructurePointers();
    InitializeSratStructures();
  }

  // Setup pointers to data structures within srat_buffer in the following
  // layout...
  // Srat                      srat_header_
  // SratProcessorApicAffinity processor_apic_affinity_[0]
  // SratMemoryAffinity        memory_affinity_[0]
  // SratMemoryAffinity        memory_affinity_[1]
  // SraHeader                 other_sra_headers_[0]
  // SraHeader                 other_sra_headers_[1]
  // SratProcessorApicAffinity processor_apic_affinity_[1]
  // SraHeader                 other_sra_headers_[2]
  // SratProcessorApicAffinity processor_apic_affinity_[2]
  // SraHeader                 other_sra_headers_[3]
  // SratMemoryAffinity        memory_affinity_[2]
  void SetupSratStructurePointers() {
    uint8_t *srat_buffer_ptr = srat_buffer_;
    srat_header_ = MakeSratHeaderView(srat_buffer_, sizeof(srat_buffer_));
    srat_buffer_ptr += srat_header_.SizeInBytes();

    processor_apic_affinity_[0] = MakeSratProcessorApicAffinityView(
        srat_buffer_ptr, SratProcessorApicAffinityView::SizeInBytes());
    srat_buffer_ptr += processor_apic_affinity_[0].SizeInBytes();

    memory_affinity_[0] = MakeSratMemoryAffinityView(
        srat_buffer_ptr, SratMemoryAffinityView::SizeInBytes());
    srat_buffer_ptr += memory_affinity_[0].SizeInBytes();
    memory_affinity_[1] = MakeSratMemoryAffinityView(
        srat_buffer_ptr, SratMemoryAffinityView::SizeInBytes());
    srat_buffer_ptr += memory_affinity_[1].SizeInBytes();

    other_sra_headers_[0] =
        MakeSraHeaderView(srat_buffer_ptr, SraHeaderView::SizeInBytes());
    srat_buffer_ptr += other_sra_headers_[0].SizeInBytes();
    other_sra_headers_[1] =
        MakeSraHeaderView(srat_buffer_ptr, SraHeaderView::SizeInBytes());
    srat_buffer_ptr += other_sra_headers_[1].SizeInBytes();

    processor_apic_affinity_[1] = MakeSratProcessorApicAffinityView(
        srat_buffer_ptr, SratProcessorApicAffinityView::SizeInBytes());
    srat_buffer_ptr += processor_apic_affinity_[1].SizeInBytes();

    other_sra_headers_[2] =
        MakeSraHeaderView(srat_buffer_ptr, SraHeaderView::SizeInBytes());
    srat_buffer_ptr += other_sra_headers_[2].SizeInBytes();

    processor_apic_affinity_[2] = MakeSratProcessorApicAffinityView(
        srat_buffer_ptr, SratProcessorApicAffinityView::SizeInBytes());
    srat_buffer_ptr += processor_apic_affinity_[2].SizeInBytes();

    other_sra_headers_[3] =
        MakeSraHeaderView(srat_buffer_ptr, SraHeaderView::SizeInBytes());
    srat_buffer_ptr += other_sra_headers_[3].SizeInBytes();

    memory_affinity_[2] = MakeSratMemoryAffinityView(
        srat_buffer_ptr, SratMemoryAffinityView::SizeInBytes());
    srat_buffer_ptr += memory_affinity_[2].SizeInBytes();

    CHECK(srat_buffer_ptr == &srat_buffer_[sizeof(srat_buffer_)])
        << absl::StrFormat("Srat buffer pointer expected to be %p, actual %p",
                           &srat_buffer_[sizeof(srat_buffer_)],
                           srat_buffer_ptr);
  }

  // Initialize test SRAT structures.
  void InitializeSratStructures() {
    auto header_view = MakeSystemDescriptionTableHeaderView(
        srat_buffer_, sizeof(srat_buffer_));
    header_view.signature().Write(Srat::kAcpiSratSignature);
    header_view.length().Write(sizeof(srat_buffer_));
    header_view.revision().Write(SratReader::kMaximumSupportedAcpiSratRevision);
    header_view.checksum().Write(0xd7);
    memcpy(header_view.oem_id().BackingStorage().data(), "GOOGLE",
           header_view.oem_id().SizeInBytes());
    memcpy(header_view.oem_table_id().BackingStorage().data(), "GOOGSRAT",
           header_view.oem_table_id().SizeInBytes());
    header_view.oem_revision().Write(0xdeadbeef);
    static_assert(sizeof("GOOG") >= sizeof(header_view.creator_id().Read()));
    header_view.creator_id().Write(LittleEndian::Load32("GOOG"));
    header_view.creator_revision().Write(0xabadcafe);

    // Initialize all processor APIC affinity structures but only mark
    // structures 0 and 2 as enabled.
    uint8_t i;
    for (i = 0; i < ABSL_ARRAYSIZE(processor_apic_affinity_); ++i) {
      memset(processor_apic_affinity_[i].BackingStorage().data(), 0,
             processor_apic_affinity_[i].SizeInBytes());
      processor_apic_affinity_[i].header().length().Write(
          processor_apic_affinity_[i].SizeInBytes());
      processor_apic_affinity_[i].header().struct_type().Write(
          SRAT_SRA_HEADER_PROCESSOR_APIC_AFFINITY);
    }
    processor_apic_affinity_[0].proximity_domain_7_0().Write(1);
    processor_apic_affinity_[0].apic_id().Write(0x10);
    processor_apic_affinity_[0].flags().Write(
        SRAT_PROCESSOR_APIC_AFFINITY_FLAGS_ENABLED);

    processor_apic_affinity_[1].proximity_domain_7_0().Write(3);
    processor_apic_affinity_[1].apic_id().Write(0x12);
    processor_apic_affinity_[1].flags().Write(0);

    processor_apic_affinity_[2].proximity_domain_7_0().Write(2);
    processor_apic_affinity_[2].apic_id().Write(0x11);
    processor_apic_affinity_[2].flags().Write(
        SRAT_PROCESSOR_APIC_AFFINITY_FLAGS_ENABLED);

    // Initialize all memory affinity structures but only mark 1 and 2 as
    // enabled.
    for (i = 0; i < ABSL_ARRAYSIZE(memory_affinity_); ++i) {
      memset(memory_affinity_[i].BackingStorage().data(), 0,
             memory_affinity_[i].SizeInBytes());
      memory_affinity_[i].header().length().Write(
          memory_affinity_[i].SizeInBytes());
      memory_affinity_[i].header().struct_type().Write(
          SRAT_SRA_HEADER_MEMORY_AFFINITY);
    }

    memory_affinity_[0].proximity_domain().Write(3);
    memory_affinity_[0].base_address().Write(0x000FFFFFE000000);
    memory_affinity_[0].length().Write(0x0000000001000000);
    memory_affinity_[0].flags().Write(0);

    memory_affinity_[1].proximity_domain().Write(2);
    memory_affinity_[0].base_address().Write(0x0000000000000000);
    memory_affinity_[0].length().Write(0x000000000009d000);
    memory_affinity_[1].flags().Write(SRAT_MEMORY_AFFINITY_FLAGS_ENABLED);

    memory_affinity_[2].proximity_domain().Write(1);
    memory_affinity_[2].base_address().Write(0x000001000000000);
    memory_affinity_[2].length().Write(0x0000008000000000);
    memory_affinity_[2].flags().Write(SRAT_MEMORY_AFFINITY_FLAGS_ENABLED);

    // Initialize all other SRA headers as types not specified by ACPI.
    for (i = 0; i < ABSL_ARRAYSIZE(other_sra_headers_); ++i) {
      other_sra_headers_[i].length().Write(other_sra_headers_[i].SizeInBytes());
      other_sra_headers_[i].struct_type().Write(
          SRAT_SRA_HEADER_MEMORY_AFFINITY + i + 1);
    }
  }

  // Writable views to test SRAT and static resource allocation entries.
  SratHeaderView srat_header_;
  SratProcessorApicAffinityWriter processor_apic_affinity_[3];
  SratMemoryAffinityWriter memory_affinity_[3];
  SraHeaderWriter other_sra_headers_[4];
  // SRAT with some processor ACPI affinity and memory affinity entries
  // see SetUp().
  uint8_t srat_buffer_[SratHeaderView::SizeInBytes() +
                       (SratProcessorApicAffinityView::SizeInBytes() *
                        ABSL_ARRAYSIZE(processor_apic_affinity_)) +
                       (SratMemoryAffinityView::SizeInBytes() *
                        ABSL_ARRAYSIZE(memory_affinity_)) +
                       (SraHeaderView::SizeInBytes() *
                        ABSL_ARRAYSIZE(other_sra_headers_))];
};

// Retrieve all Processor Local APIC/SAPIC affinity structures from a valid
// SRAT.
TEST_F(SratReaderTest, GetProcessorApicAffinityAll) {
  SratReader reader(srat_header_);
  std::vector<SratProcessorApicAffinityView> processor_apic_structs =
      reader.GetProcessorApicAffinity(false);
  EXPECT_EQ(processor_apic_structs.size(),
            ABSL_ARRAYSIZE(processor_apic_affinity_));
  uint8_t i;
  for (i = 0; i < ABSL_ARRAYSIZE(processor_apic_affinity_); ++i) {
    EXPECT_TRUE(processor_apic_structs[i].Equals(processor_apic_affinity_[i]));
  }
}

// Retrieve all enabled Processor Local APIC/SAPIC affinity structures from a
// valid SRAT.
TEST_F(SratReaderTest, GetProcessorApicAffinityEnabled) {
  SratReader reader(srat_header_);
  std::vector<SratProcessorApicAffinityView> processor_apic_structs =
      reader.GetProcessorApicAffinity(true);
  EXPECT_EQ(processor_apic_structs.size(),
            ABSL_ARRAYSIZE(processor_apic_affinity_) - 1);
  EXPECT_TRUE(processor_apic_structs[0].Equals(processor_apic_affinity_[0]));
  EXPECT_TRUE(processor_apic_structs[1].Equals(processor_apic_affinity_[2]));
}

// Retrieve all memory affinity structures from a valid SRAT.
TEST_F(SratReaderTest, GetMemoryAffinityAll) {
  SratReader reader(srat_header_);
  std::vector<SratMemoryAffinityView> memory_affinity_structs =
      reader.GetMemoryAffinity(false);
  EXPECT_EQ(memory_affinity_structs.size(), ABSL_ARRAYSIZE(memory_affinity_));
  uint8_t i;
  for (i = 0; i < ABSL_ARRAYSIZE(memory_affinity_); ++i) {
    EXPECT_TRUE(memory_affinity_structs[i].Equals(memory_affinity_[i]));
  }
}

// Retrieve all enabled memory affinity structures from a valid SRAT.
TEST_F(SratReaderTest, GetMemoryAffinityEnabled) {
  SratReader reader(srat_header_);
  std::vector<SratMemoryAffinityView> memory_affinity_structs =
      reader.GetMemoryAffinity(true);
  EXPECT_EQ(memory_affinity_structs.size(),
            ABSL_ARRAYSIZE(memory_affinity_) - 1);
  EXPECT_TRUE(memory_affinity_structs[0].Equals(memory_affinity_[1]));
  EXPECT_TRUE(memory_affinity_structs[1].Equals(memory_affinity_[2]));
}

// Test the validation of a valid Processor APIC affinity structure.
TEST_F(SratReaderTest, ValidateProcessorApicAffinityValid) {
  EXPECT_TRUE(
      SraHeaderDescriptor::Validate(processor_apic_affinity_[0].header(),
                                    SRAT_SRA_HEADER_PROCESSOR_APIC_AFFINITY,
                                    processor_apic_affinity_[0].SizeInBytes(),
                                    "Processor Local APIC/SAPIC Affinity"));
}

// Test the validation of a Processor APIC affinity structure with a bad type.
TEST_F(SratReaderTest, ValidateProcessorApicAffinityInvalidType) {
  processor_apic_affinity_[0].header().struct_type().Write(
      SRAT_SRA_HEADER_MEMORY_AFFINITY);
  EXPECT_FALSE(
      SraHeaderDescriptor::Validate(processor_apic_affinity_[0].header(),
                                    SRAT_SRA_HEADER_PROCESSOR_APIC_AFFINITY,
                                    processor_apic_affinity_[0].SizeInBytes(),
                                    "Processor Local APIC/SAPIC Affinity"));
}

// Test the validation of a Processor APIC affinity structure with a bad size.
TEST_F(SratReaderTest, ValidateProcessorApicAffinityInvalidSize) {
  processor_apic_affinity_[0].header().length().Write(
      processor_apic_affinity_[0].header().length().Read() - 2);
  processor_apic_affinity_[0].header().struct_type().Write(
      SRAT_SRA_HEADER_MEMORY_AFFINITY);
  EXPECT_FALSE(
      SraHeaderDescriptor::Validate(processor_apic_affinity_[0].header(),
                                    SRAT_SRA_HEADER_PROCESSOR_APIC_AFFINITY,
                                    processor_apic_affinity_[0].SizeInBytes(),
                                    "Processor Local APIC/SAPIC Affinity"));
}
/*  return header.Validate(SRAT_SRA_HEADER_MEMORY_AFFINITY, sizeof(*this),
                         "Memory Affinity");
*/
// Test the validation of a valid memory affinity structure.
TEST_F(SratReaderTest, ValidateMemoryAffinityValid) {
  EXPECT_TRUE(SraHeaderDescriptor::Validate(
      memory_affinity_[0].header(), SRAT_SRA_HEADER_MEMORY_AFFINITY,
      memory_affinity_[0].SizeInBytes(), "Memory Affinity"));
}

// Test the validation of a valid memory affinity structure with a bad type.
TEST_F(SratReaderTest, ValidateMemoryAffinityInvalidType) {
  memory_affinity_[0].header().struct_type().Write(
      SRAT_SRA_HEADER_PROCESSOR_APIC_AFFINITY);
  EXPECT_FALSE(SraHeaderDescriptor::Validate(
      memory_affinity_[0].header(), SRAT_SRA_HEADER_MEMORY_AFFINITY,
      memory_affinity_[0].SizeInBytes(), "Memory Affinity"));
}

// Test the validation of a valid memory affinity structure with a bad size.
TEST_F(SratReaderTest, ValidateMemoryAffinityInvalidSize) {
  memory_affinity_[0].header().length().Write(
      memory_affinity_[0].header().length().Read() - 3);
  EXPECT_FALSE(SraHeaderDescriptor::Validate(
      memory_affinity_[0].header(), SRAT_SRA_HEADER_MEMORY_AFFINITY,
      memory_affinity_[0].SizeInBytes(), "Memory Affinity"));
}

class SratTestV3 : public ::testing::Test {
 public:
  SratTestV3() {
    auto table = SystemDescriptionTable::ReadFromFile(
        GetTestDataDependencyPath(kTestSysfsAcpiSratPath));
    CHECK(table.ok()) << table.status().ToString();

    srat_ = std::make_unique<Srat>(std::move(table.value()));
  }

 protected:
  // Path to a valid SRAT file with static resource allocation entries.
  static constexpr absl::string_view kTestSysfsAcpiSratPath =
      "lib/acpi/test_data/sys_firmware_acpi_tables_SRAT_v3";
  std::unique_ptr<Srat> srat_;
};

// A test for validating the reader for SRAT revision 3.
// The expectations are derived by using the acpica utilities: acpidump,
// acpixtract and isal
// These utilities can be found under: //depot/google3/third_party/acpica
// Steps to generate a human readable format for SRAT tables
// 1. acpidump > acpidump.out
// 2. acpixtract -a acpidump.out
// 3. iasl -d *.dat
// The last step creates *.dsl, a human readable form of all the ACPI tables

TEST_F(SratTestV3, ReadSratFromFileSuccess) {
  SratReader reader(srat_->GetSratHeader());

  EXPECT_TRUE(reader.Validate());
  EXPECT_EQ(reader.GetProcessorApicAffinity(false).size(), 224);
  EXPECT_EQ(reader.GetProcessorApicAffinity(true).size(), 0);

  EXPECT_EQ(reader.GetMemoryAffinity(false).size(), 32);
  EXPECT_EQ(reader.GetMemoryAffinity(true).size(), 3);

  EXPECT_EQ(reader.GetProcessorx2ApicAffinity(false).size(), 224);
  EXPECT_EQ(reader.GetProcessorx2ApicAffinity(true).size(), 112);
}

}  // namespace

}  // namespace ecclesia
