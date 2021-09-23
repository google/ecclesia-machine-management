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

#include "ecclesia/lib/acpi/dmar.h"

#include <algorithm>
#include <cstdint>
#include <ios>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/acpi/dmar.emb.h"
#include "ecclesia/lib/acpi/system_description_table.emb.h"
#include "ecclesia/lib/acpi/system_description_table.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/testing/status.h"
#include "runtime/cpp/emboss_cpp_util.h"
#include "runtime/cpp/emboss_prelude.h"

namespace ecclesia {

namespace {

// Path to the test DMAR table. The text version is at
// ${ECCLESIA_ROOT}/lib/acpi/test_data/sys_firmware_acpi_tables_DMAR.dsl.
constexpr absl::string_view kSysfsAcpiDmarPath =
    "lib/acpi/test_data/sys_firmware_acpi_tables_DMAR";

// Read from the test file and verify the header checksum.
TEST(DmarTest, ReadFromFileReturnsValidContent) {
  auto table = SystemDescriptionTable::ReadFromFile(
      GetTestDataDependencyPath(kSysfsAcpiDmarPath));
  ASSERT_THAT(table, IsOk());

  // Check the table header.
  auto header_view = table.value()->GetHeaderView();
  EXPECT_EQ(header_view.signature().Read(), Dmar::kAcpiDmarSignature);
  EXPECT_EQ(header_view.length().Read(), 0x00000160);
  EXPECT_EQ(header_view.revision().Read(), 0x01);
  EXPECT_EQ(header_view.checksum().Read(), 0xDC);

  auto oem_id = header_view.oem_id().BackingStorage();
  EXPECT_TRUE(std::equal(oem_id.begin(), oem_id.end(),
                         "ALASKA")) << oem_id.ToString<std::string>();
  auto oem_table_id = header_view.oem_table_id().BackingStorage();
  EXPECT_TRUE(std::equal(oem_table_id.begin(), oem_table_id.end(),
                         "A M I \0\0")) << oem_table_id.ToString<std::string>();
  EXPECT_EQ(header_view.oem_revision().Read(), 0x00000001);
  uint32_t creator_id = header_view.creator_id().Read();
  EXPECT_TRUE(
      std::equal(reinterpret_cast<const char*>(&creator_id),
                 reinterpret_cast<const char*>(&creator_id + 1),
                 "INTL")) << std::hex << creator_id;
  EXPECT_EQ(header_view.creator_revision().Read(), 0x20160527);
  // Check the SRA Structures.
  Dmar dmar(std::move(table.value()));
  DmarReader reader(dmar.GetDmarHeader());
  EXPECT_TRUE(reader.Validate());
  std::vector<DmarHardwareUnitDefinitionView> hardware_unit_definitions =
      reader.GetHardwareUnitDefinition();

  auto hardware_unit_validate_func = [](DmarHardwareUnitDefinitionView view) {
    return DmarSraHeaderDescriptor::Validate(
        view.header(), DMAR_SRA_HEADER_TYPE_HARDWARE_UNIT_DEFINITION,
        DmarHardwareUnitDefinitionView::SizeInBytes(),
        "Hardware Unit Definition");
  };

  EXPECT_TRUE(hardware_unit_validate_func(hardware_unit_definitions[0]));
  EXPECT_EQ(hardware_unit_definitions[0].header().struct_type().Read(), 0x0000);
  EXPECT_EQ(hardware_unit_definitions[0].header().length().Read(), 0x0060);
  EXPECT_EQ(hardware_unit_definitions[0].flags().Read(), 0x00);
  EXPECT_EQ(hardware_unit_definitions[0].reserved().Read(), 0x00);
  EXPECT_EQ(hardware_unit_definitions[0].pci_segment_number().Read(), 0x0000);
  EXPECT_EQ(hardware_unit_definitions[0].register_base_address().Read(),
            0x00000000D37FC000);

  EXPECT_TRUE(hardware_unit_validate_func(hardware_unit_definitions[1]));
  EXPECT_EQ(hardware_unit_definitions[1].header().struct_type().Read(), 0x0000);
  EXPECT_EQ(hardware_unit_definitions[1].header().length().Read(), 0x0020);
  EXPECT_EQ(hardware_unit_definitions[1].flags().Read(), 0x00);
  EXPECT_EQ(hardware_unit_definitions[1].reserved().Read(), 0x00);
  EXPECT_EQ(hardware_unit_definitions[1].pci_segment_number().Read(), 0x0000);
  EXPECT_EQ(hardware_unit_definitions[1].register_base_address().Read(),
            0x00000000E0FFC000);

  EXPECT_TRUE(hardware_unit_validate_func(hardware_unit_definitions[2]));
  EXPECT_EQ(hardware_unit_definitions[2].header().struct_type().Read(), 0x0000);
  EXPECT_EQ(hardware_unit_definitions[2].header().length().Read(), 0x0020);
  EXPECT_EQ(hardware_unit_definitions[2].flags().Read(), 0x00);
  EXPECT_EQ(hardware_unit_definitions[2].reserved().Read(), 0x00);
  EXPECT_EQ(hardware_unit_definitions[2].pci_segment_number().Read(), 0x0000);
  EXPECT_EQ(hardware_unit_definitions[2].register_base_address().Read(),
            0x00000000EE7FC000);

  std::vector<DmarReservedMemoryRegionView> reserved_memory_region =
      reader.GetReservedMemoryRegion();

  EXPECT_TRUE(DmarSraHeaderDescriptor::Validate(
      reserved_memory_region[0].header(),
      DMAR_SRA_HEADER_TYPE_RESERVED_MEMORY_REGION,
      DmarReservedMemoryRegionView::SizeInBytes(), "Reserved Memory Region"));
  EXPECT_EQ(reserved_memory_region[0].header().struct_type().Read(), 0x0001);
  EXPECT_EQ(reserved_memory_region[0].header().length().Read(), 0x0020);
  EXPECT_EQ(reserved_memory_region[0].reserved().Read(), 0x0000);
  EXPECT_EQ(reserved_memory_region[0].pci_segment_number().Read(), 0x0000);
  EXPECT_EQ(reserved_memory_region[0].register_base_address().Read(),
            0x0000000075460000);
  EXPECT_EQ(reserved_memory_region[0].register_limit_address().Read(),
            0x0000000075470FFF);

  std::vector<DmarRootPortAtsCapabilityView> root_port_ats_capability =
      reader.GetRootPortAtsCapability();

  auto root_ports_validate_func = [](DmarRootPortAtsCapabilityView view) {
    return DmarSraHeaderDescriptor::Validate(
        view.header(), DMAR_SRA_HEADER_TYPE_ROOT_PORT_ATS_CAPABILITY,
        DmarRootPortAtsCapabilityView::SizeInBytes(),
        "Root Port ATS Capability");
  };
  EXPECT_TRUE(root_ports_validate_func(root_port_ats_capability[0]));
  EXPECT_EQ(root_port_ats_capability[0].header().struct_type().Read(), 0x0002);
  EXPECT_EQ(root_port_ats_capability[0].header().length().Read(), 0x0020);
  EXPECT_EQ(root_port_ats_capability[0].flags().Read(), 0x00);
  EXPECT_EQ(root_port_ats_capability[0].reserved().Read(), 0x00);
  EXPECT_EQ(root_port_ats_capability[0].pci_segment_number().Read(), 0x0000);

  EXPECT_TRUE(root_ports_validate_func(root_port_ats_capability[1]));
  EXPECT_EQ(root_port_ats_capability[1].header().struct_type().Read(), 0x0002);
  EXPECT_EQ(root_port_ats_capability[1].header().length().Read(), 0x0028);
  EXPECT_EQ(root_port_ats_capability[1].flags().Read(), 0x00);
  EXPECT_EQ(root_port_ats_capability[1].reserved().Read(), 0x00);
  EXPECT_EQ(root_port_ats_capability[1].pci_segment_number().Read(), 0x0000);

  std::vector<DmarRemappingHardwareStaticAffinityView>
      remapping_hardware_static_affinity =
          reader.GetRemappingHardwareStaticAffinity();

  auto remapping_validate_func =
      [](DmarRemappingHardwareStaticAffinityView view) {
        return DmarSraHeaderDescriptor::Validate(
            view.header(),
            DMAR_SRA_HEADER_TYPE_REMAPPING_HARDWARE_STATIC_AFFINITY,
            DmarRemappingHardwareStaticAffinityView::SizeInBytes(),
            "Remapping Hardware Static Affinity");
      };
  EXPECT_TRUE(remapping_validate_func(remapping_hardware_static_affinity[0]));
  EXPECT_EQ(remapping_hardware_static_affinity[0].header().struct_type().Read(),
            0x0003);
  EXPECT_EQ(remapping_hardware_static_affinity[0].header().length().Read(),
            0x0014);
  EXPECT_EQ(remapping_hardware_static_affinity[0].reserved().Read(),
            0x00000000);
  EXPECT_EQ(
      remapping_hardware_static_affinity[0].register_base_address().Read(),
      0x000000009D7FC000);
  EXPECT_EQ(remapping_hardware_static_affinity[0].proximity_domain().Read(),
            0x00000000);

  EXPECT_TRUE(remapping_validate_func(remapping_hardware_static_affinity[1]));
  EXPECT_EQ(remapping_hardware_static_affinity[1].header().struct_type().Read(),
            0x0003);
  EXPECT_EQ(remapping_hardware_static_affinity[1].header().length().Read(),
            0x0014);
  EXPECT_EQ(remapping_hardware_static_affinity[1].reserved().Read(),
            0x00000000);
  EXPECT_EQ(
      remapping_hardware_static_affinity[1].register_base_address().Read(),
      0x00000000AAFFC000);
  EXPECT_EQ(remapping_hardware_static_affinity[1].proximity_domain().Read(),
            0x00000000);
}

}  // namespace

}  // namespace ecclesia
