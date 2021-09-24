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

#include "ecclesia/magent/sysmodel/indus/nvme.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ecclesia/lib/io/pci/config.h"
#include "ecclesia/lib/io/pci/discovery.h"
#include "ecclesia/lib/io/pci/location.h"
#include "ecclesia/lib/io/pci/mocks.h"
#include "ecclesia/magent/sysmodel/x86/nvme.h"

namespace ecclesia {
namespace {

using ::testing::Return;
using ::testing::UnorderedElementsAre;

using PciNodeMap = PciTopologyInterface::PciNodeMap;
using PciAcpiPath = MockPciTopology::PciAcpiPath;

TEST(IndusNvmeDiscoverTest, FailureEnumerateNodes) {
  MockPciTopology pci_topology;
  EXPECT_CALL(pci_topology, EnumerateAllNodes()).WillOnce([]() {
    return absl::InternalError("EnumerateAllNodes failure");
  });

  IndusNvmeDiscover nvme_discover(&pci_topology);

  auto maybe_nvme_locations = nvme_discover.GetAllNvmeLocations();
  EXPECT_FALSE(maybe_nvme_locations.ok());
  EXPECT_EQ(maybe_nvme_locations.status().code(), absl::StatusCode::kInternal);
}

TEST(IndusNvmeDiscoverTest, FailureEnumerateAcpiPci) {
  MockPciTopology pci_topology;
  PciNodeMap pci_node_map;
  EXPECT_CALL(pci_topology, EnumerateAllNodes()).WillOnce([&]() {
    return std::move(pci_node_map);
  });

  EXPECT_CALL(pci_topology, EnumeratePciAcpiPaths()).WillOnce([&]() {
    return absl::InternalError("EnumeratePciAcpiPaths failure");
  });

  IndusNvmeDiscover nvme_discover(&pci_topology);

  auto maybe_nvme_locations = nvme_discover.GetAllNvmeLocations();
  EXPECT_FALSE(maybe_nvme_locations.ok());
  EXPECT_EQ(maybe_nvme_locations.status().code(), absl::StatusCode::kInternal);
}

TEST(IndusNvmeDiscoverTest, GetCorrectLocations) {
  MockPciTopology pci_topology;
  // Construct a PCI topology tree.
  // 0000:ae:0.0
  //   |--0000:af:0.0
  // 0000:d7:0.0
  //   |--0000:d8:0.0
  // 0000:d7:1.0
  //.  |--0000:d9:0.0
  PciDbdfLocation pci_loc_ae_0 = PciDbdfLocation::Make<0, 0xae, 0, 0>();
  PciDbdfLocation pci_loc_af_0 = PciDbdfLocation::Make<0, 0xaf, 0, 0>();
  PciDbdfLocation pci_loc_d7_0 = PciDbdfLocation::Make<0, 0xd7, 0, 0>();
  PciDbdfLocation pci_loc_d8_0 = PciDbdfLocation::Make<0, 0xd8, 0, 0>();
  PciDbdfLocation pci_loc_d7_1 = PciDbdfLocation::Make<0, 0xd7, 1, 0>();
  PciDbdfLocation pci_loc_d9_0 = PciDbdfLocation::Make<0, 0xd9, 0, 0>();
  auto node_ae_0 =
      std::make_unique<PciTopologyInterface::Node>(pci_loc_ae_0, 0, nullptr);
  auto node_af_0 = std::make_unique<PciTopologyInterface::Node>(
      pci_loc_af_0, 0, node_ae_0.get());
  auto node_d7_0 =
      std::make_unique<PciTopologyInterface::Node>(pci_loc_d7_0, 0, nullptr);
  auto node_d8_0 = std::make_unique<PciTopologyInterface::Node>(
      pci_loc_d8_0, 0, node_d7_0.get());
  auto node_d7_1 =
      std::make_unique<PciTopologyInterface::Node>(pci_loc_d7_1, 0, nullptr);
  auto node_d9_0 = std::make_unique<PciTopologyInterface::Node>(
      pci_loc_d9_0, 0, node_d7_1.get());
  node_ae_0->AddChild(node_af_0.get());
  node_d7_0->AddChild(node_d8_0.get());
  node_d7_1->AddChild(node_d9_0.get());

  // Set up mock up for PciTopology.EnumerateAllNodes.
  PciNodeMap pci_node_map;
  pci_node_map[pci_loc_ae_0] = std::move(node_ae_0);
  pci_node_map[pci_loc_af_0] = std::move(node_af_0);
  pci_node_map[pci_loc_d7_0] = std::move(node_d7_0);
  pci_node_map[pci_loc_d8_0] = std::move(node_d8_0);
  pci_node_map[pci_loc_d7_1] = std::move(node_d7_1);
  pci_node_map[pci_loc_d9_0] = std::move(node_d9_0);
  EXPECT_CALL(pci_topology, EnumerateAllNodes()).WillOnce([&](auto...) {
    return std::move(pci_node_map);
  });

  // Set up mock up for PciTopology.EnumeratePciAcpiPaths.
  // 0000:00 - ACPI "\\_SB_.PC00"
  // 0000:ae - ACPI "\\_SB_.PC08"
  // 0000:d7 - ACPI "\\_SB_.PC09"
  std::vector<PciAcpiPath> pci_acpi_paths;
  pci_acpi_paths.push_back(
      PciAcpiPath{PciDomain::Make<0>(), PciBusNum::Make<0>(), "\\_SB_.PC00"});
  pci_acpi_paths.push_back(PciAcpiPath{PciDomain::Make<0>(),
                                       PciBusNum::Make<0xae>(), "\\_SB_.PC08"});
  pci_acpi_paths.push_back(PciAcpiPath{PciDomain::Make<0>(),
                                       PciBusNum::Make<0xd7>(), "\\_SB_.PC09"});

  EXPECT_CALL(pci_topology, EnumeratePciAcpiPaths()).WillOnce([&](auto...) {
    return pci_acpi_paths;
  });

  FakePciRegion config_region =
      FakePciRegion::EndpointConfig({.class_code = 0x010802});
  PciConfigSpace pci_config_space(&config_region);

  // Set up mock up for PciTopology.CreateDevice.
  auto pci_device_af_0 = std::make_unique<MockPciDevice>(pci_loc_af_0);
  EXPECT_CALL(*pci_device_af_0, ConfigSpace())
      .WillOnce(Return(&pci_config_space));
  auto pci_device_d8_0 = std::make_unique<MockPciDevice>(pci_loc_d8_0);
  EXPECT_CALL(*pci_device_d8_0, ConfigSpace())
      .WillOnce(Return(&pci_config_space));
  auto pci_device_d9_0 = std::make_unique<MockPciDevice>(pci_loc_d9_0);
  EXPECT_CALL(*pci_device_d9_0, ConfigSpace())
      .WillOnce(Return(&pci_config_space));
  EXPECT_CALL(pci_topology, CreateDevice(pci_loc_af_0)).WillOnce([&](auto...) {
    return std::move(pci_device_af_0);
  });
  EXPECT_CALL(pci_topology, CreateDevice(pci_loc_d8_0)).WillOnce([&](auto...) {
    return std::move(pci_device_d8_0);
  });
  EXPECT_CALL(pci_topology, CreateDevice(pci_loc_d9_0)).WillOnce([&](auto...) {
    return std::move(pci_device_d9_0);
  });

  IndusNvmeDiscover nvme_discover(&pci_topology);

  auto maybe_nvme_locations = nvme_discover.GetAllNvmeLocations();
  ASSERT_TRUE(maybe_nvme_locations.ok());
  EXPECT_THAT(maybe_nvme_locations.value(),
              UnorderedElementsAre(NvmeLocation{pci_loc_af_0, "U2_0"},
                                   NvmeLocation{pci_loc_d8_0, "U2_7"},
                                   NvmeLocation{pci_loc_d9_0, "U2_6"}));
}

}  // namespace
}  // namespace ecclesia
