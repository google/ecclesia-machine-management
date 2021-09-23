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

#include "ecclesia/lib/acpi/mcfg.h"

#include <algorithm>
#include <cstdint>
#include <ios>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/acpi/mcfg.emb.h"
#include "ecclesia/lib/acpi/system_description_table.emb.h"
#include "ecclesia/lib/acpi/system_description_table.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/testing/status.h"
#include "runtime/cpp/emboss_cpp_util.h"
#include "runtime/cpp/emboss_prelude.h"

namespace ecclesia {

namespace {

constexpr absl::string_view kSysfsAcpiMcfgPath =
    "lib/acpi/test_data/sys_firmware_acpi_tables_MCFG";

}  // namespace

// Read from the test file and verify the read data.
TEST(McfgTest, ReadFromFile) {
  auto table = SystemDescriptionTable::ReadFromFile(
      GetTestDataDependencyPath(kSysfsAcpiMcfgPath));
  ASSERT_THAT(table, IsOk());

  Mcfg mcfg(std::move(table.value()));
  McfgReader mcfg_reader(mcfg.GetMcfgHeader());
  EXPECT_TRUE(mcfg_reader.Validate());

  auto header_view = mcfg.GetMcfgHeader().header();
  EXPECT_EQ(Mcfg::kAcpiMcfgSignature, header_view.signature().Read());
  EXPECT_EQ(60, header_view.length().Read());
  EXPECT_EQ(1, header_view.revision().Read());
  EXPECT_EQ(0x1A, header_view.checksum().Read());
  auto oem_id = header_view.oem_id().BackingStorage();
  EXPECT_TRUE(std::equal(oem_id.begin(), oem_id.end(), "INTL  "))
      << oem_id.ToString<std::string>();
  auto oem_table_id = header_view.oem_table_id().BackingStorage();
  EXPECT_TRUE(std::equal(oem_table_id.begin(), oem_table_id.end(), "PE_SC3  "))
      << oem_table_id.ToString<std::string>();
  EXPECT_EQ(1, header_view.oem_revision().Read());
  uint32_t creator_id = header_view.creator_id().Read();
  EXPECT_TRUE(std::equal(reinterpret_cast<const char*>(&creator_id),
                         reinterpret_cast<const char*>(&creator_id + 1),
                         "INTL")) << std::hex << creator_id;
  EXPECT_EQ(1, header_view.creator_revision().Read());
}

TEST(McfgTest, ReadStructuresFromFile) {
  auto table = SystemDescriptionTable::ReadFromFile(
      GetTestDataDependencyPath(kSysfsAcpiMcfgPath));
  ASSERT_THAT(table, IsOk());

  Mcfg mcfg(std::move(table.value()));
  McfgReader mcfg_reader(mcfg.GetMcfgHeader());
  EXPECT_TRUE(mcfg_reader.ValidateSignature());

  std::vector<McfgSegmentView> segments = mcfg_reader.GetSegments();
  ASSERT_EQ(1, segments.size());

  EXPECT_TRUE(segments[0].base_address().Ok());
  EXPECT_TRUE(segments[0].segment().Ok());
  EXPECT_TRUE(segments[0].start_bus_number().Ok());
  EXPECT_TRUE(segments[0].end_bus_number().Ok());

  EXPECT_EQ(0xe0000000, segments[0].base_address().Read());
  EXPECT_EQ(0, segments[0].segment().Read());
  EXPECT_EQ(0, segments[0].start_bus_number().Read());
  EXPECT_EQ(0xff, segments[0].end_bus_number().Read());
}

}  // namespace ecclesia
