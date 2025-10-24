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

#include "ecclesia/lib/io/pci/sysfs.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/io/pci/config.h"
#include "ecclesia/lib/io/pci/discovery.h"
#include "ecclesia/lib/io/pci/location.h"
#include "ecclesia/lib/io/pci/pci.h"
#include "ecclesia/lib/testing/status.h"

namespace ecclesia {
namespace {

using ::testing::Not;
using ::testing::UnorderedElementsAre;

using PciAcpiPath = PciTopologyInterface::PciAcpiPath;
using PciPlatformPath = PciTopologyInterface::PciPlatformPath;

// Helper matchers that can check if a BarInfo is a memory or I/O BAR with the
// given base address.
MATCHER_P(IsMemoryBar, expected_address, "") {
  return arg.type == PciResources::kBarTypeMem &&
         arg.address == expected_address;
}
MATCHER_P(IsIoBar, expected_address, "") {
  return arg.type == PciResources::kBarTypeIo &&
         arg.address == expected_address;
}

constexpr absl::string_view kTestResourceFile =
    R"(0x000000009d784000 0x000000009d785fff 0x0000000000040200
0x000000009d786000 0x000000009d7860ff 0x0000000000040200
0x0000000000003030 0x0000000000003037 0x0000000000040101
0x0000000000003020 0x0000000000003023 0x0000000000040101
0x0000000000003000 0x000000000000301f 0x0000000000040101
0x000000009d700000 0x000000009d77ffff 0x0000000000040200
0x0000000000000000 0x0000000000000000 0x0000000000000000
0x0000000000000000 0x0000000000000000 0x0000000000000000
0x0000000000000000 0x0000000000000000 0x0000000000000000
0x0000000000000000 0x0000000000000000 0x0000000000000000
0x0000000000000000 0x0000000000000000 0x0000000000000000
0x0000000000000000 0x0000000000000000 0x0000000000000000
0x0000000000000000 0x0000000000000000 0x0000000000000000)";

class PciSysTest : public testing::Test {
 public:
  PciSysTest() : fs_(GetTestTempdirPath()) {
    fs_.CreateDir("/sys/bus/pci/devices/0001:02:03.4");
    fs_.CreateFile("/sys/bus/pci/devices/0001:02:03.4/config", "0123456789");
    fs_.CreateFile("/sys/bus/pci/devices/0001:02:03.4/resource",
                   kTestResourceFile);
    fs_.CreateFile("/sys/bus/pci/devices/0001:02:03.4/current_link_width",
                   "4\n");
    fs_.CreateFile("/sys/bus/pci/devices/0001:02:03.4/current_link_speed",
                   "2.5 GT/s\n");
    fs_.CreateFile("/sys/bus/pci/devices/0001:02:03.4/max_link_width", "8\n");
    fs_.CreateFile("/sys/bus/pci/devices/0001:02:03.4/max_link_speed",
                   "8 GT/s\n");
  }

 protected:
  TestFilesystem fs_;
};

class PciTopologyTest : public testing::Test {
 public:
  PciTopologyTest() : fs_(GetTestTempdirPath()) {
    fs_.CreateDir("/sys/devices");
    fs_.CreateDir("/sys/devices/pci0000:ae");
    fs_.CreateDir("/sys/devices/pci0000:ae/firmware_node");
    fs_.CreateFile("/sys/devices/pci0000:ae/firmware_node/path",
                   "\\_SB_.PC00\n");
    fs_.CreateDir("/sys/devices/pci0000:ae/0000:ae:00.0");
    fs_.CreateDir("/sys/devices/pci0000:ae/0000:ae:00.0/0000:af:00.0");
    fs_.CreateDir("/sys/devices/pci0000:ae/0000:ae:00.0/0000:af:01.0");
    fs_.CreateDir("/sys/devices/pci0000:ae/0000:ae:01.0");
    fs_.CreateDir("/sys/devices/pci0000:d7");
    fs_.CreateDir("/sys/devices/pci0000:d7/firmware_node");
    fs_.CreateFile("/sys/devices/pci0000:d7/firmware_node/path",
                   "\\_SB_.PC01\n");
    fs_.CreateDir("/sys/devices/pci0000:d7/0000.d7:00.0");
    fs_.CreateDir("/sys/devices/pci0000:ae/0000:d7:00.0/0000:d8:00.0");
  }

 protected:
  TestFilesystem fs_;
};

TEST(SysfsPciDeviceTest, CreatePciDevice) {
  TestFilesystem fs(GetTestTempdirPath());
  auto existing_pci_loc = PciDbdfLocation::Make<0, 0xfe, 12, 3>();
  fs.CreateDir(
      absl::StrCat("/sys/bus/pci/devices/", existing_pci_loc.ToString()));

  auto pci_device = SysfsPciDevice::TryCreateDevice(
      fs.GetTruePath("/sys/bus/pci/devices/"), existing_pci_loc);
  EXPECT_NE(pci_device, nullptr);

  auto non_existing_pci_loc = PciDbdfLocation::Make<0, 0xfe, 12, 4>();
  pci_device = SysfsPciDevice::TryCreateDevice(
      fs.GetTruePath("/sys/bus/pci/devices/"), non_existing_pci_loc);
  EXPECT_EQ(pci_device, nullptr);
}

TEST_F(PciSysTest, TestRegionDefaultConstructed) {
  auto loc = PciDbdfLocation::Make<1, 2, 3, 4>();
  auto region = SysPciRegion(loc);

  auto maybe_uint8 = region.Read8(4);
  EXPECT_TRUE(absl::IsNotFound(maybe_uint8.status()));
}

TEST_F(PciSysTest, TestRead) {
  auto loc = PciDbdfLocation::Make<1, 2, 3, 4>();
  auto region = SysPciRegion(fs_.GetTruePath("/sys/bus/pci/devices/"), loc);

  auto maybe_uint8 = region.Read8(4);
  EXPECT_TRUE(maybe_uint8.ok());
  EXPECT_EQ(maybe_uint8.value(), 0x34);

  auto maybe_uint16 = region.Read16(4);
  EXPECT_TRUE(maybe_uint16.ok());
  EXPECT_EQ(maybe_uint16.value(), 0x3534);

  auto maybe_uint32 = region.Read32(4);
  EXPECT_TRUE(maybe_uint32.ok());
  EXPECT_EQ(maybe_uint32.value(), 0x37363534);
}

TEST_F(PciSysTest, TestReadFailOutofRange) {
  auto loc = PciDbdfLocation::Make<1, 2, 3, 4>();
  auto region = SysPciRegion(fs_.GetTruePath("/sys/bus/pci/devices/"), loc);

  auto maybe_uint8 = region.Read8(10);
  EXPECT_FALSE(maybe_uint8.ok());
  EXPECT_TRUE(absl::IsInternal(maybe_uint8.status()));

  auto maybe_uint16 = region.Read16(10);
  EXPECT_FALSE(maybe_uint16.ok());
  EXPECT_TRUE(absl::IsInternal(maybe_uint16.status()));

  auto maybe_uint32 = region.Read32(10);
  EXPECT_FALSE(maybe_uint32.ok());
  EXPECT_TRUE(absl::IsInternal(maybe_uint32.status()));
}

TEST_F(PciSysTest, TestReadFailNotFound) {
  auto loc = PciDbdfLocation::Make<1, 2, 3, 5>();
  auto region = SysPciRegion(fs_.GetTruePath("/sys/bus/pci/devices/"), loc);

  auto maybe_uint8 = region.Read8(10);
  EXPECT_FALSE(maybe_uint8.ok());
  EXPECT_TRUE(absl::IsNotFound(maybe_uint8.status()));

  auto maybe_uint16 = region.Read16(10);
  EXPECT_FALSE(maybe_uint16.ok());
  EXPECT_TRUE(absl::IsNotFound(maybe_uint16.status()));

  auto maybe_uint32 = region.Read32(10);
  EXPECT_FALSE(maybe_uint32.ok());
  EXPECT_TRUE(absl::IsNotFound(maybe_uint32.status()));
}

TEST_F(PciSysTest, TestWrite) {
  auto loc = PciDbdfLocation::Make<1, 2, 3, 4>();
  auto region = SysPciRegion(fs_.GetTruePath("/sys/bus/pci/devices/"), loc);

  uint8_t u8 = 0x0B;
  EXPECT_TRUE(region.Write8(4, u8).ok());
  auto maybe_uint8 = region.Read8(4);
  EXPECT_TRUE(maybe_uint8.ok());
  EXPECT_EQ(maybe_uint8.value(), u8);

  uint16_t u16 = 0xBE;
  EXPECT_TRUE(region.Write16(4, u16).ok());
  auto maybe_uint16 = region.Read16(4);
  EXPECT_TRUE(maybe_uint16.ok());
  EXPECT_EQ(maybe_uint16.value(), u16);

  uint32_t u32 = 0xBEEF;
  EXPECT_TRUE(region.Write32(4, u32).ok());
  auto maybe_uint32 = region.Read32(4);
  EXPECT_TRUE(maybe_uint32.ok());
  EXPECT_EQ(maybe_uint32.value(), u32);
}

TEST_F(PciSysTest, TestWriteFailNotFound) {
  auto loc = PciDbdfLocation::Make<1, 2, 3, 5>();
  auto region = SysPciRegion(fs_.GetTruePath("/sys/bus/pci/devices/"), loc);
  absl::Status status;

  uint8_t u8 = 0x0B;
  status = region.Write8(4, u8);
  EXPECT_FALSE(status.ok());
  EXPECT_TRUE(absl::IsNotFound(status));

  uint16_t u16 = 0x0BE;
  status = region.Write16(4, u16);
  EXPECT_FALSE(status.ok());
  EXPECT_TRUE(absl::IsNotFound(status));

  uint32_t u32 = 0x0BEEF;
  status = region.Write32(4, u32);
  EXPECT_FALSE(status.ok());
  EXPECT_TRUE(absl::IsNotFound(status));
}

TEST_F(PciSysTest, TestResources) {
  auto loc = PciDbdfLocation::Make<1, 2, 3, 4>();
  auto resources =
      SysfsPciResources(fs_.GetTruePath("/sys/bus/pci/devices/"), loc);

  EXPECT_TRUE(resources.Exists());
  EXPECT_THAT(resources.GetBaseAddress<0>(),
              IsOkAndHolds(IsMemoryBar(0x9d784000)));
  EXPECT_THAT(resources.GetBaseAddress<1>(),
              IsOkAndHolds(IsMemoryBar(0x9d786000)));
  EXPECT_THAT(resources.GetBaseAddress<2>(), IsOkAndHolds(IsIoBar(0x3030)));
  EXPECT_THAT(resources.GetBaseAddress<3>(), IsOkAndHolds(IsIoBar(0x3020)));
  EXPECT_THAT(resources.GetBaseAddress<4>(), IsOkAndHolds(IsIoBar(0x3000)));
  EXPECT_THAT(resources.GetBaseAddress<5>(),
              IsOkAndHolds(IsMemoryBar(0x9d700000)));
}

TEST_F(PciSysTest, TestResourcesNotFound) {
  auto loc = PciDbdfLocation::Make<1, 2, 3, 5>();
  auto resources =
      SysfsPciResources(fs_.GetTruePath("/sys/bus/pci/devices/"), loc);

  EXPECT_FALSE(resources.Exists());
  EXPECT_THAT(resources.GetBaseAddress<0>(), Not(IsOk()));
  EXPECT_THAT(resources.GetBaseAddress<1>(), Not(IsOk()));
  EXPECT_THAT(resources.GetBaseAddress<2>(), Not(IsOk()));
  EXPECT_THAT(resources.GetBaseAddress<3>(), Not(IsOk()));
  EXPECT_THAT(resources.GetBaseAddress<4>(), Not(IsOk()));
  EXPECT_THAT(resources.GetBaseAddress<5>(), Not(IsOk()));
}

TEST_F(PciSysTest, TestGettingLinkCapabilities) {
  auto loc = PciDbdfLocation::Make<1, 2, 3, 4>();

  auto pci_dev = SysfsPciDevice::TryCreateDevice(
      fs_.GetTruePath("/sys/bus/pci/devices/"), loc);
  ASSERT_NE(pci_dev, nullptr);

  auto current_capabilities = pci_dev->PcieLinkCurrentCapabilities();
  ASSERT_TRUE(current_capabilities.ok());
  EXPECT_EQ(current_capabilities->width, PcieLinkWidth::kWidth4);
  EXPECT_EQ(current_capabilities->speed, PcieLinkSpeed::kGen1Speed2500MT);

  auto max_capabilities = pci_dev->PcieLinkMaxCapabilities();
  ASSERT_TRUE(max_capabilities.ok());
  EXPECT_EQ(max_capabilities->width, PcieLinkWidth::kWidth8);
  EXPECT_EQ(max_capabilities->speed, PcieLinkSpeed::kGen3Speed8GT);
}

TEST_F(PciTopologyTest, EnumerateAllNodes) {
  SysfsPciTopology pci_topology(fs_.GetTruePath("/sys/devices/"));
  auto maybe_pci_map = pci_topology.EnumerateAllNodes();
  ASSERT_TRUE(maybe_pci_map.ok());
  auto &location_nodes_map = maybe_pci_map.value();
  ASSERT_EQ(location_nodes_map.size(), 6);
  std::vector<PciDbdfLocation> expect_all_locations = {
      PciDbdfLocation::Make<0, 0xae, 0, 0>(),
      PciDbdfLocation::Make<0, 0xae, 1, 0>(),
      PciDbdfLocation::Make<0, 0xaf, 0, 0>(),
      PciDbdfLocation::Make<0, 0xaf, 1, 0>(),
      PciDbdfLocation::Make<0, 0xd7, 0, 0>(),
      PciDbdfLocation::Make<0, 0xd8, 0, 0>()};
  for (const auto &location : expect_all_locations) {
    EXPECT_TRUE(location_nodes_map.contains(location));
    EXPECT_NE(location_nodes_map.at(location), nullptr);
  }
}

TEST_F(PciTopologyTest, CorrectTopology) {
  SysfsPciTopology pci_topology(fs_.GetTruePath("/sys/devices/"));
  auto maybe_pci_map = pci_topology.EnumerateAllNodes();
  ASSERT_TRUE(maybe_pci_map.ok());
  auto &location_nodes_map = maybe_pci_map.value();

  // Check the PCI tree branch:
  // 0000:ae:0.0
  //   |--0000:af:0.0
  //   |--0000:af:1.0
  auto root_node0_location = PciDbdfLocation::Make<0, 0xae, 0, 0>();
  ASSERT_TRUE(location_nodes_map.contains(root_node0_location));
  auto *root_node0 = location_nodes_map.at(root_node0_location).get();
  EXPECT_EQ(root_node0->Location(), root_node0_location);
  EXPECT_EQ(root_node0->Depth(), 0);
  EXPECT_EQ(root_node0->Parent(), nullptr);
  auto node0_children = root_node0->Children();
  ASSERT_EQ(node0_children.size(), 2);
  std::vector<PciDbdfLocation> children_locations;
  auto *child0 = node0_children.at(0);
  children_locations.push_back(child0->Location());
  EXPECT_EQ(child0->Depth(), 1);
  EXPECT_TRUE(child0->Children().empty());
  EXPECT_EQ(child0->Parent()->Location(), root_node0_location);
  auto *child1 = node0_children.at(1);
  children_locations.push_back(child1->Location());
  EXPECT_EQ(child1->Depth(), 1);
  EXPECT_TRUE(child1->Children().empty());
  EXPECT_EQ(child1->Parent()->Location(), root_node0_location);
  // The two children have no particular order.
  EXPECT_THAT(children_locations,
              UnorderedElementsAre(PciDbdfLocation::Make<0, 0xaf, 0, 0>(),
                                   PciDbdfLocation::Make<0, 0xaf, 1, 0>()));

  // Check the PCI tree branch:
  // 0000:d7:0.0
  //   |--0000:d8:0.0
  auto root_node1_location = PciDbdfLocation::Make<0, 0xd7, 0, 0>();
  ASSERT_TRUE(location_nodes_map.contains(root_node1_location));
  auto *root_node1 = location_nodes_map.at(root_node1_location).get();
  EXPECT_EQ(root_node1->Location(), root_node1_location);
  EXPECT_EQ(root_node1->Depth(), 0);
  EXPECT_EQ(root_node1->Parent(), nullptr);
  auto node1_children = root_node1->Children();
  ASSERT_EQ(node1_children.size(), 1);
  auto node1_child0_location = PciDbdfLocation::Make<0, 0xd8, 0, 0>();
  child0 = node1_children.at(0);
  EXPECT_EQ(child0->Location(), node1_child0_location);
  EXPECT_EQ(child0->Depth(), 1);
  EXPECT_TRUE(child0->Children().empty());
  EXPECT_EQ(child0->Parent()->Location(), root_node1_location);
}

TEST_F(PciTopologyTest, EnumeratePciAcpiPaths) {
  SysfsPciTopology pci_topology(fs_.GetTruePath("/sys/devices/"));
  auto maybe_pci_acpi_paths = pci_topology.EnumeratePciAcpiPaths();
  ASSERT_TRUE(maybe_pci_acpi_paths.ok());
  auto &pci_acpi_paths = maybe_pci_acpi_paths.value();
  ASSERT_EQ(pci_acpi_paths.size(), 2);

  EXPECT_THAT(pci_acpi_paths,
              UnorderedElementsAre(
                  PciAcpiPath{PciDomain::Make<0>(), PciBusNum::Make<0xae>(),
                              "\\_SB_.PC00"},
                  PciAcpiPath{PciDomain::Make<0>(), PciBusNum::Make<0xd7>(),
                              "\\_SB_.PC01"}));
}

TEST(SysfsPciTopologyTest, EnumeratePciPlatformPaths) {
  TestFilesystem fs(GetTestTempdirPath());
  // This contents reflects the /sys/devices directory on a dioriteimc node.
  fs.CreateDir("/sys/devices");
  fs.CreateDir("/sys/devices/platform/2041800000.pcie/pci0000:00");
  fs.CreateDir("/sys/devices/platform/204e400000.pcie/pci0001:00");
  fs.CreateDir("/sys/devices/platform/204ec00000.pcie");
  fs.CreateDir("/sys/devices/platform/2053a00000.core-mbox");
  fs.CreateDir("/sys/devices/platform/20c0010000.cpe_gcsr");

  SysfsPciTopology pci_topology(fs.GetTruePath("/sys/devices/"));
  auto maybe_pci_platform_paths = pci_topology.EnumeratePciPlatformPaths();
  ASSERT_TRUE(maybe_pci_platform_paths.ok());
  auto &pci_platform_paths = maybe_pci_platform_paths.value();
  ASSERT_EQ(pci_platform_paths.size(), 2);

  EXPECT_THAT(
      pci_platform_paths,
      UnorderedElementsAre(
          PciPlatformPath{PciDomain::Make<0>(), PciBusNum::Make<00>(),
                          "2041800000.pcie"},
          PciPlatformPath{PciDomain::Make<1>(), PciBusNum::Make<00>(),
                          "204e400000.pcie"}));
}

TEST(SysfsPciTopologyTest, EnumeratePciPlatformPathsHexParsing) {
  TestFilesystem fs(GetTestTempdirPath());
  fs.CreateDir("/sys/devices");
  fs.CreateDir("/sys/devices/platform/123456789.pcie/pci000a:bc");
  SysfsPciTopology pci_topology(fs.GetTruePath("/sys/devices/"));
  auto maybe_pci_platform_paths = pci_topology.EnumeratePciPlatformPaths();
  ASSERT_TRUE(maybe_pci_platform_paths.ok());
  auto &pci_platform_paths = maybe_pci_platform_paths.value();
  ASSERT_EQ(pci_platform_paths.size(), 1);

  EXPECT_THAT(
      pci_platform_paths,
      UnorderedElementsAre(
          PciPlatformPath{PciDomain::Make<0xa>(), PciBusNum::Make<0xbc>(),
                          "123456789.pcie"}));
}

TEST(SysfsPciTopologyTest, EnumeratePciPlatformPathsNoPlatformDir) {
  TestFilesystem fs(GetTestTempdirPath());
  fs.CreateDir("/sys/devices");
  SysfsPciTopology pci_topology(fs.GetTruePath("/sys/devices/"));
  auto maybe_pci_platform_paths = pci_topology.EnumeratePciPlatformPaths();
  ASSERT_TRUE(maybe_pci_platform_paths.ok());
  auto &pci_platform_paths = maybe_pci_platform_paths.value();
  ASSERT_EQ(pci_platform_paths.size(), 0);
}

TEST(SysfsPciTopologyTest, EnumeratePciPlatformPathsEmptyPlatformDir) {
  TestFilesystem fs(GetTestTempdirPath());
  fs.CreateDir("/sys/devices");
  fs.CreateDir("/sys/devices/platform");
  SysfsPciTopology pci_topology(fs.GetTruePath("/sys/devices/"));
  auto maybe_pci_platform_paths = pci_topology.EnumeratePciPlatformPaths();
  ASSERT_TRUE(maybe_pci_platform_paths.ok());
  auto &pci_platform_paths = maybe_pci_platform_paths.value();
  ASSERT_EQ(pci_platform_paths.size(), 0);
}

}  // namespace
}  // namespace ecclesia
