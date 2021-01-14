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

#include "ecclesia/lib/redfish/topology.h"

#include <memory>
#include <ostream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/test_mockup.h"
#include "ecclesia/lib/redfish/testing/json_mockup.h"
#include "ecclesia/lib/redfish/types.h"

namespace libredfish {

// Define equality of two nodes being only their name, devpath, type. Ignore
// the properties.
bool operator==(const Node &n1, const Node &n2) {
  return std::tie(n1.name, n1.local_devpath, n1.type) ==
         std::tie(n2.name, n2.local_devpath, n2.type);
}

// Some print functions defined to simplify debugging with GMock output.
std::string ToString(NodeType type) {
  switch (type) {
    case NodeType::kBoard:
      return "Board";
    case NodeType::kConnector:
      return "Connector";
    case NodeType::kDevice:
      return "Device";
    case NodeType::kCable:
      return "Cable";
  }
}

void PrintTo(const Node &node, std::ostream *os) {
  *os << absl::StrFormat("\n{name: \"%s\" devpath: \"%s\" type: %s)", node.name,
                         node.local_devpath, ToString(node.type));
}

void PrintTo(const std::unique_ptr<Node> &node, std::ostream *os) {
  if (node) {
    PrintTo(*node, os);
  } else {
    *os << "\n(null)";
  }
}

namespace {

using ::testing::ContainerEq;
using ::testing::Eq;
using ::testing::IsNull;
using ::testing::Not;
using ::testing::Pointee;
using ::testing::UnorderedElementsAre;

TEST(Topology, Empty) {
  auto redfish_intf = NewJsonMockupInterface(R"json(
    {}
  )json");

  NodeTopology topology = CreateTopologyFromRedfish(redfish_intf.get());
  EXPECT_TRUE(topology.devpath_to_node_map.empty());
  EXPECT_TRUE(topology.nodes.empty());
  EXPECT_TRUE(topology.uri_to_associated_node_map.empty());
}

TEST(NodeTopologiesHaveTheSameNodes, Empty) {
  NodeTopology n1;
  NodeTopology n2;
  EXPECT_TRUE(NodeTopologiesHaveTheSameNodes(n1, n2));
}

TEST(NodeTopologiesHaveTheSameNodes, IdenticalNodes) {
  Node phys{.name = "mobo", .local_devpath = "/phys", .type = kBoard};
  Node phys_connector_pe0{.name = "PE0",
                          .local_devpath = "/phys:connector:PE0",
                          .type = kConnector};
  Node phys_connector_pe1{.name = "PE1",
                          .local_devpath = "/phys:connector:PE1",
                          .type = kConnector};
  Node pe0{.name = "fuzzyback", .local_devpath = "/phys/PE0", .type = kBoard};
  Node pe1{.name = "pcie_cable", .local_devpath = "/phys/PE1", .type = kCable};
  Node pe1_connector_downlink{.name = "DOWNLINK",
                              .local_devpath = "/phys/PE1:connector:DOWNLINK",
                              .type = kConnector};
  Node pe1_downlink{.name = "wuzzyback",
                    .local_devpath = "/phys/PE1/DOWNLINK",
                    .type = kBoard};

  NodeTopology n1;
  n1.nodes.push_back(absl::make_unique<Node>(phys));
  n1.nodes.push_back(absl::make_unique<Node>(phys_connector_pe0));
  n1.nodes.push_back(absl::make_unique<Node>(phys_connector_pe1));
  n1.nodes.push_back(absl::make_unique<Node>(pe0));
  n1.nodes.push_back(absl::make_unique<Node>(pe1));
  n1.nodes.push_back(absl::make_unique<Node>(pe1_connector_downlink));
  n1.nodes.push_back(absl::make_unique<Node>(pe1_downlink));

  NodeTopology n2;
  n2.nodes.push_back(absl::make_unique<Node>(phys));
  n2.nodes.push_back(absl::make_unique<Node>(phys_connector_pe0));
  n2.nodes.push_back(absl::make_unique<Node>(phys_connector_pe1));
  n2.nodes.push_back(absl::make_unique<Node>(pe0));
  n2.nodes.push_back(absl::make_unique<Node>(pe1));
  n2.nodes.push_back(absl::make_unique<Node>(pe1_connector_downlink));
  n2.nodes.push_back(absl::make_unique<Node>(pe1_downlink));

  EXPECT_TRUE(NodeTopologiesHaveTheSameNodes(n1, n2));
}

TEST(NodeTopologiesHaveTheSameNodes, OrderDoesntMatter) {
  Node phys{.name = "mobo", .local_devpath = "/phys", .type = kBoard};
  Node phys_connector_pe0{.name = "PE0",
                          .local_devpath = "/phys:connector:PE0",
                          .type = kConnector};
  Node phys_connector_pe1{.name = "PE1",
                          .local_devpath = "/phys:connector:PE1",
                          .type = kConnector};
  Node pe0{.name = "fuzzyback", .local_devpath = "/phys/PE0", .type = kBoard};
  Node pe1{.name = "pcie_cable", .local_devpath = "/phys/PE1", .type = kCable};
  Node pe1_connector_downlink{.name = "DOWNLINK",
                              .local_devpath = "/phys/PE1:connector:DOWNLINK",
                              .type = kConnector};
  Node pe1_downlink{.name = "wuzzyback",
                    .local_devpath = "/phys/PE1/DOWNLINK",
                    .type = kBoard};

  NodeTopology n1;
  n1.nodes.push_back(absl::make_unique<Node>(phys));
  n1.nodes.push_back(absl::make_unique<Node>(phys_connector_pe0));
  n1.nodes.push_back(absl::make_unique<Node>(phys_connector_pe1));
  n1.nodes.push_back(absl::make_unique<Node>(pe0));
  n1.nodes.push_back(absl::make_unique<Node>(pe1));
  n1.nodes.push_back(absl::make_unique<Node>(pe1_connector_downlink));
  n1.nodes.push_back(absl::make_unique<Node>(pe1_downlink));

  NodeTopology n2;
  n2.nodes.push_back(absl::make_unique<Node>(phys_connector_pe0));
  n2.nodes.push_back(absl::make_unique<Node>(phys));
  n2.nodes.push_back(absl::make_unique<Node>(pe0));
  n2.nodes.push_back(absl::make_unique<Node>(phys_connector_pe1));
  n2.nodes.push_back(absl::make_unique<Node>(pe1_connector_downlink));
  n2.nodes.push_back(absl::make_unique<Node>(pe1));
  n2.nodes.push_back(absl::make_unique<Node>(pe1_downlink));

  EXPECT_TRUE(NodeTopologiesHaveTheSameNodes(n1, n2));
}

TEST(NodeTopologiesHaveTheSameNodes, MissingNodesDetected) {
  Node phys{.name = "mobo", .local_devpath = "/phys", .type = kBoard};
  Node phys_connector_pe0{.name = "PE0",
                          .local_devpath = "/phys:connector:PE0",
                          .type = kConnector};
  Node pe0{.name = "fuzzyback", .local_devpath = "/phys/PE0", .type = kBoard};

  NodeTopology n1;
  n1.nodes.push_back(absl::make_unique<Node>(phys));
  n1.nodes.push_back(absl::make_unique<Node>(phys_connector_pe0));
  n1.nodes.push_back(absl::make_unique<Node>(pe0));

  NodeTopology n2;
  n2.nodes.push_back(absl::make_unique<Node>(phys));
  n2.nodes.push_back(absl::make_unique<Node>(phys_connector_pe0));

  EXPECT_FALSE(NodeTopologiesHaveTheSameNodes(n1, n2));
}

TEST(NodeTopologiesHaveTheSameNodes, NewNameDetected) {
  Node phys{.name = "mobo", .local_devpath = "/phys", .type = kBoard};
  Node phys_connector_pe1{.name = "PE1",
                          .local_devpath = "/phys:connector:PE1",
                          .type = kConnector};
  Node pe1{.name = "pcie_cable", .local_devpath = "/phys/PE1", .type = kCable};
  Node pe1_connector_downlink{.name = "DOWNLINK",
                              .local_devpath = "/phys/PE1:connector:DOWNLINK",
                              .type = kConnector};

  Node pe1_downlink_old{.name = "wuzzyback",
                        .local_devpath = "/phys/PE1/DOWNLINK",
                        .type = kBoard};

  Node pe1_downlink_new{.name = "fuzzywuzzyback",
                        .local_devpath = "/phys/PE1/DOWNLINK",
                        .type = kBoard};

  NodeTopology n1;
  n1.nodes.push_back(absl::make_unique<Node>(phys));
  n1.nodes.push_back(absl::make_unique<Node>(phys_connector_pe1));
  n1.nodes.push_back(absl::make_unique<Node>(pe1));
  n1.nodes.push_back(absl::make_unique<Node>(pe1_connector_downlink));
  n1.nodes.push_back(absl::make_unique<Node>(pe1_downlink_old));

  NodeTopology n2;
  n2.nodes.push_back(absl::make_unique<Node>(phys));
  n2.nodes.push_back(absl::make_unique<Node>(phys_connector_pe1));
  n2.nodes.push_back(absl::make_unique<Node>(pe1));
  n2.nodes.push_back(absl::make_unique<Node>(pe1_connector_downlink));
  n2.nodes.push_back(absl::make_unique<Node>(pe1_downlink_new));

  EXPECT_FALSE(NodeTopologiesHaveTheSameNodes(n1, n2));
}

TEST(NodeTopologiesHaveTheSameNodes, NewDevpathDetected) {
  Node phys{.name = "mobo", .local_devpath = "/phys", .type = kBoard};
  Node phys_connector_pe1{.name = "PE1",
                          .local_devpath = "/phys:connector:PE1",
                          .type = kConnector};
  Node phys_connector_pe2{.name = "PE2",
                          .local_devpath = "/phys:connector:PE2",
                          .type = kConnector};

  NodeTopology n1;
  n1.nodes.push_back(absl::make_unique<Node>(phys));
  n1.nodes.push_back(absl::make_unique<Node>(phys_connector_pe1));

  NodeTopology n2;
  n2.nodes.push_back(absl::make_unique<Node>(phys));
  n2.nodes.push_back(absl::make_unique<Node>(phys_connector_pe2));

  EXPECT_FALSE(NodeTopologiesHaveTheSameNodes(n1, n2));
}

TEST(NodeTopologiesHaveTheSameNodes, NewTypeDetected) {
  Node phys{.name = "mobo", .local_devpath = "/phys", .type = kBoard};
  Node phys_connector_pe1{.name = "PE1",
                          .local_devpath = "/phys:connector:PE1",
                          .type = kConnector};
  Node pe1_old{.name = "plugin", .local_devpath = "/phys/PE1", .type = kCable};
  Node pe1_new{.name = "plugin", .local_devpath = "/phys/PE1", .type = kBoard};

  NodeTopology n1;
  n1.nodes.push_back(absl::make_unique<Node>(phys));
  n1.nodes.push_back(absl::make_unique<Node>(phys_connector_pe1));

  NodeTopology n2;
  n2.nodes.push_back(absl::make_unique<Node>(phys));
  n2.nodes.push_back(absl::make_unique<Node>(pe1_new));

  EXPECT_FALSE(NodeTopologiesHaveTheSameNodes(n1, n2));
}

TEST(RawInterfaceTestWithMockup, IndusHmbCnMockupNodesArePopulated) {
  TestingMockupServer mockup("indus_hmb_cn/mockup.shar");
  auto raw_intf = mockup.RedfishClientInterface();
  NodeTopology topology = CreateTopologyFromRedfish(raw_intf.get());

  const std::vector<Node> expected_nodes = {
      Node{"indus", "/phys", NodeType::kBoard},
      Node{"CPU0", "/phys:connector:CPU0", NodeType::kConnector},
      Node{"CPU1", "/phys:connector:CPU1", NodeType::kConnector},
      Node{"DIMM0", "/phys:connector:DIMM0", NodeType::kConnector},
      Node{"DIMM1", "/phys:connector:DIMM1", NodeType::kConnector},
      Node{"DIMM2", "/phys:connector:DIMM2", NodeType::kConnector},
      Node{"DIMM3", "/phys:connector:DIMM3", NodeType::kConnector},
      Node{"DIMM4", "/phys:connector:DIMM4", NodeType::kConnector},
      Node{"DIMM5", "/phys:connector:DIMM5", NodeType::kConnector},
      Node{"DIMM6", "/phys:connector:DIMM6", NodeType::kConnector},
      Node{"DIMM7", "/phys:connector:DIMM7", NodeType::kConnector},
      Node{"DIMM8", "/phys:connector:DIMM8", NodeType::kConnector},
      Node{"DIMM9", "/phys:connector:DIMM9", NodeType::kConnector},
      Node{"DIMM10", "/phys:connector:DIMM10", NodeType::kConnector},
      Node{"DIMM11", "/phys:connector:DIMM11", NodeType::kConnector},
      Node{"DIMM12", "/phys:connector:DIMM12", NodeType::kConnector},
      Node{"DIMM13", "/phys:connector:DIMM13", NodeType::kConnector},
      Node{"DIMM14", "/phys:connector:DIMM14", NodeType::kConnector},
      Node{"DIMM15", "/phys:connector:DIMM15", NodeType::kConnector},
      Node{"DIMM16", "/phys:connector:DIMM16", NodeType::kConnector},
      Node{"DIMM17", "/phys:connector:DIMM17", NodeType::kConnector},
      Node{"DIMM18", "/phys:connector:DIMM18", NodeType::kConnector},
      Node{"DIMM19", "/phys:connector:DIMM19", NodeType::kConnector},
      Node{"DIMM20", "/phys:connector:DIMM20", NodeType::kConnector},
      Node{"DIMM21", "/phys:connector:DIMM21", NodeType::kConnector},
      Node{"DIMM22", "/phys:connector:DIMM22", NodeType::kConnector},
      Node{"DIMM23", "/phys:connector:DIMM23", NodeType::kConnector},
      Node{"KA0", "/phys:connector:KA0", NodeType::kConnector},
      Node{"KA2", "/phys:connector:KA2", NodeType::kConnector},
      Node{"PE0", "/phys:connector:PE0", NodeType::kConnector},
      Node{"PE1", "/phys:connector:PE1", NodeType::kConnector},
      Node{"PE2", "/phys:connector:PE2", NodeType::kConnector},
      Node{"PE3", "/phys:connector:PE3", NodeType::kConnector},
      Node{"PE4", "/phys:connector:PE4", NodeType::kConnector},
      Node{"PE5", "/phys:connector:PE5", NodeType::kConnector},
      Node{"PE6", "/phys:connector:PE6", NodeType::kConnector},
      Node{"IPASS0", "/phys:connector:IPASS0", NodeType::kConnector},
      Node{"IPASS1", "/phys:connector:IPASS1", NodeType::kConnector},
      Node{"USB_lower", "/phys:connector:USB_lower", NodeType::kConnector},
      Node{"USB_upper", "/phys:connector:USB_upper", NodeType::kConnector},
      Node{"SYS_FAN0", "/phys:connector:SYS_FAN0", NodeType::kConnector},
      Node{"SYS_FAN1", "/phys:connector:SYS_FAN1", NodeType::kConnector},
      Node{"SYS_FAN2", "/phys:connector:SYS_FAN2", NodeType::kConnector},
      Node{"SYS_FAN3", "/phys:connector:SYS_FAN3", NodeType::kConnector},
      Node{"SYS_FAN4", "/phys:connector:SYS_FAN4", NodeType::kConnector},
      Node{"SYS_FAN5", "/phys:connector:SYS_FAN5", NodeType::kConnector},
      Node{"SYS_FAN6", "/phys:connector:SYS_FAN6", NodeType::kConnector},
      Node{"SYS_FAN7", "/phys:connector:SYS_FAN7", NodeType::kConnector},
      Node{"P48_PSU_L", "/phys:connector:P48_PSU_L", NodeType::kConnector},
      Node{"TRAY", "/phys:connector:TRAY", NodeType::kConnector},
      Node{"BIOS_SPI", "/phys:connector:BIOS_SPI", NodeType::kConnector},
      Node{"CPU0_ANCHORS", "/phys:connector:CPU0_ANCHORS",
           NodeType::kConnector},
      Node{"CPU1_ANCHORS", "/phys:connector:CPU1_ANCHORS",
           NodeType::kConnector},
      Node{"NCSI", "/phys:connector:NCSI", NodeType::kConnector},
      Node{"fan_40mm", "/phys/SYS_FAN0", NodeType::kBoard},
      Node{"fan_40mm", "/phys/SYS_FAN1", NodeType::kBoard},
      Node{"fan_assembly", "/phys/SYS_FAN2", NodeType::kBoard},
      Node{"FAN2", "/phys/SYS_FAN2:connector:FAN2", NodeType::kConnector},
      Node{"FAN3", "/phys/SYS_FAN2:connector:FAN3", NodeType::kConnector},
      Node{"FAN4", "/phys/SYS_FAN2:connector:FAN4", NodeType::kConnector},
      Node{"FAN5", "/phys/SYS_FAN2:connector:FAN5", NodeType::kConnector},
      Node{"FAN6", "/phys/SYS_FAN2:connector:FAN6", NodeType::kConnector},
      Node{"FAN7", "/phys/SYS_FAN2:connector:FAN7", NodeType::kConnector},
      Node{"fan_60mm", "/phys/SYS_FAN2/FAN2", NodeType::kBoard},
      Node{"fan_60mm", "/phys/SYS_FAN2/FAN3", NodeType::kBoard},
      Node{"fan_60mm", "/phys/SYS_FAN2/FAN4", NodeType::kBoard},
      Node{"fan_60mm", "/phys/SYS_FAN2/FAN5", NodeType::kBoard},
      Node{"fan_40mm", "/phys/SYS_FAN2/FAN6", NodeType::kBoard},
      Node{"fan_40mm", "/phys/SYS_FAN2/FAN7", NodeType::kBoard},
      Node{"dc_rack_power_cable", "/phys/P48_PSU_L", NodeType::kCable},
      Node{"DOWNLINK", "/phys/P48_PSU_L:connector:DOWNLINK",
           NodeType::kConnector},
      Node{"tray", "/phys/TRAY", NodeType::kBoard},
      Node{"flash_chip", "/phys/BIOS_SPI", NodeType::kBoard},
      Node{"cooler", "/phys/CPU0_ANCHORS", NodeType::kBoard},
      Node{"cooler", "/phys/CPU1_ANCHORS", NodeType::kBoard},
      Node{"hdmi_cable", "/phys/NCSI", NodeType::kCable},
      Node{"DOWNLINK", "/phys/NCSI:connector:DOWNLINK", NodeType::kConnector},
      Node{"koolaid", "/phys/KA0", NodeType::kBoard},
      Node{"koolaid", "/phys/KA2", NodeType::kBoard},
      Node{"ddr4", "/phys/DIMM0", NodeType::kBoard},
      Node{"ddr4", "/phys/DIMM1", NodeType::kBoard},
      Node{"ddr4", "/phys/DIMM2", NodeType::kBoard},
      Node{"ddr4", "/phys/DIMM3", NodeType::kBoard},
      Node{"ddr4", "/phys/DIMM4", NodeType::kBoard},
      Node{"ddr4", "/phys/DIMM5", NodeType::kBoard},
      Node{"ddr4", "/phys/DIMM6", NodeType::kBoard},
      Node{"ddr4", "/phys/DIMM7", NodeType::kBoard},
      Node{"ddr4", "/phys/DIMM8", NodeType::kBoard},
      Node{"ddr4", "/phys/DIMM9", NodeType::kBoard},
      Node{"ddr4", "/phys/DIMM10", NodeType::kBoard},
      Node{"ddr4", "/phys/DIMM11", NodeType::kBoard},
      Node{"ddr4", "/phys/DIMM12", NodeType::kBoard},
      Node{"ddr4", "/phys/DIMM13", NodeType::kBoard},
      Node{"ddr4", "/phys/DIMM14", NodeType::kBoard},
      Node{"ddr4", "/phys/DIMM15", NodeType::kBoard},
      Node{"ddr4", "/phys/DIMM16", NodeType::kBoard},
      Node{"ddr4", "/phys/DIMM17", NodeType::kBoard},
      Node{"ddr4", "/phys/DIMM18", NodeType::kBoard},
      Node{"ddr4", "/phys/DIMM19", NodeType::kBoard},
      Node{"ddr4", "/phys/DIMM20", NodeType::kBoard},
      Node{"ddr4", "/phys/DIMM21", NodeType::kBoard},
      Node{"ddr4", "/phys/DIMM22", NodeType::kBoard},
      Node{"ddr4", "/phys/DIMM23", NodeType::kBoard},
      Node{"cascadelake", "/phys/CPU0", NodeType::kBoard},
      Node{"cascadelake", "/phys/CPU1", NodeType::kBoard},
  };

  std::vector<Node> actual_nodes;
  for (const auto &node : topology.nodes) {
    actual_nodes.push_back(*node);
  }

  EXPECT_THAT(actual_nodes, ContainerEq(expected_nodes));
}

TEST(RawInterfaceTestWithMockup, IndusHmbCnMockupDevpathToNodeMapMatches) {
  TestingMockupServer mockup("indus_hmb_cn/mockup.shar");
  auto raw_intf = mockup.RedfishClientInterface();
  NodeTopology topology = CreateTopologyFromRedfish(raw_intf.get());

  for (const auto &pair : topology.devpath_to_node_map) {
    const absl::string_view &devpath = pair.first;
    ASSERT_THAT(pair.second, Not(IsNull()));
    EXPECT_THAT(devpath, Eq(pair.second->local_devpath));
  }
}

TEST(RawInterfaceTestWithMockup, IndusHmbCnMockupUriMapIsCorrect) {
  TestingMockupServer mockup("indus_hmb_cn/mockup.shar");
  auto raw_intf = mockup.RedfishClientInterface();
  NodeTopology topology = CreateTopologyFromRedfish(raw_intf.get());

  ASSERT_THAT(topology.uri_to_associated_node_map.size(), Eq(51));

  ASSERT_TRUE(topology.uri_to_associated_node_map.contains(
      "/redfish/v1/Chassis/chassis"));
  EXPECT_THAT(
      topology.uri_to_associated_node_map.at("/redfish/v1/Chassis/chassis"),
      UnorderedElementsAre(Pointee(Node{"indus", "/phys", NodeType::kBoard})));

  ASSERT_TRUE(topology.uri_to_associated_node_map.contains(
      "/redfish/v1/Systems/system/Processors/0"));
  EXPECT_THAT(topology.uri_to_associated_node_map.at(
                  "/redfish/v1/Systems/system/Processors/0"),
              UnorderedElementsAre(Pointee(
                  Node{"cascadelake", "/phys/CPU0", NodeType::kBoard})));

  ASSERT_TRUE(topology.uri_to_associated_node_map.contains(
      "/redfish/v1/Systems/system/Processors/1"));
  EXPECT_THAT(topology.uri_to_associated_node_map.at(
                  "/redfish/v1/Systems/system/Processors/1"),
              UnorderedElementsAre(Pointee(
                  Node{"cascadelake", "/phys/CPU1", NodeType::kBoard})));

  ASSERT_TRUE(topology.uri_to_associated_node_map.contains(
      "/redfish/v1/Systems/system/Memory/0"));
  EXPECT_THAT(topology.uri_to_associated_node_map.at(
                  "/redfish/v1/Systems/system/Memory/0"),
              UnorderedElementsAre(
                  Pointee(Node{"ddr4", "/phys/DIMM0", NodeType::kBoard})));

  ASSERT_TRUE(topology.uri_to_associated_node_map.contains(
      "/redfish/v1/Systems/system/Memory/1"));
  EXPECT_THAT(topology.uri_to_associated_node_map.at(
                  "/redfish/v1/Systems/system/Memory/1"),
              UnorderedElementsAre(
                  Pointee(Node{"ddr4", "/phys/DIMM1", NodeType::kBoard})));

  ASSERT_TRUE(topology.uri_to_associated_node_map.contains(
      "/redfish/v1/Systems/system/Memory/2"));
  EXPECT_THAT(topology.uri_to_associated_node_map.at(
                  "/redfish/v1/Systems/system/Memory/2"),
              UnorderedElementsAre(
                  Pointee(Node{"ddr4", "/phys/DIMM2", NodeType::kBoard})));

  ASSERT_TRUE(topology.uri_to_associated_node_map.contains(
      "/redfish/v1/Systems/system/Memory/3"));
  EXPECT_THAT(topology.uri_to_associated_node_map.at(
                  "/redfish/v1/Systems/system/Memory/3"),
              UnorderedElementsAre(
                  Pointee(Node{"ddr4", "/phys/DIMM3", NodeType::kBoard})));

  ASSERT_TRUE(topology.uri_to_associated_node_map.contains(
      "/redfish/v1/Systems/system/Memory/4"));
  EXPECT_THAT(topology.uri_to_associated_node_map.at(
                  "/redfish/v1/Systems/system/Memory/4"),
              UnorderedElementsAre(
                  Pointee(Node{"ddr4", "/phys/DIMM4", NodeType::kBoard})));

  ASSERT_TRUE(topology.uri_to_associated_node_map.contains(
      "/redfish/v1/Systems/system/Memory/5"));
  EXPECT_THAT(topology.uri_to_associated_node_map.at(
                  "/redfish/v1/Systems/system/Memory/5"),
              UnorderedElementsAre(
                  Pointee(Node{"ddr4", "/phys/DIMM5", NodeType::kBoard})));

  ASSERT_TRUE(topology.uri_to_associated_node_map.contains(
      "/redfish/v1/Systems/system/Memory/6"));
  EXPECT_THAT(topology.uri_to_associated_node_map.at(
                  "/redfish/v1/Systems/system/Memory/6"),
              UnorderedElementsAre(
                  Pointee(Node{"ddr4", "/phys/DIMM6", NodeType::kBoard})));

  ASSERT_TRUE(topology.uri_to_associated_node_map.contains(
      "/redfish/v1/Systems/system/Memory/7"));
  EXPECT_THAT(topology.uri_to_associated_node_map.at(
                  "/redfish/v1/Systems/system/Memory/7"),
              UnorderedElementsAre(
                  Pointee(Node{"ddr4", "/phys/DIMM7", NodeType::kBoard})));

  ASSERT_TRUE(topology.uri_to_associated_node_map.contains(
      "/redfish/v1/Systems/system/Memory/8"));
  EXPECT_THAT(topology.uri_to_associated_node_map.at(
                  "/redfish/v1/Systems/system/Memory/8"),
              UnorderedElementsAre(
                  Pointee(Node{"ddr4", "/phys/DIMM8", NodeType::kBoard})));

  ASSERT_TRUE(topology.uri_to_associated_node_map.contains(
      "/redfish/v1/Systems/system/Memory/9"));
  EXPECT_THAT(topology.uri_to_associated_node_map.at(
                  "/redfish/v1/Systems/system/Memory/9"),
              UnorderedElementsAre(
                  Pointee(Node{"ddr4", "/phys/DIMM9", NodeType::kBoard})));

  ASSERT_TRUE(topology.uri_to_associated_node_map.contains(
      "/redfish/v1/Systems/system/Memory/10"));
  EXPECT_THAT(topology.uri_to_associated_node_map.at(
                  "/redfish/v1/Systems/system/Memory/10"),
              UnorderedElementsAre(
                  Pointee(Node{"ddr4", "/phys/DIMM10", NodeType::kBoard})));

  ASSERT_TRUE(topology.uri_to_associated_node_map.contains(
      "/redfish/v1/Systems/system/Memory/11"));
  EXPECT_THAT(topology.uri_to_associated_node_map.at(
                  "/redfish/v1/Systems/system/Memory/11"),
              UnorderedElementsAre(
                  Pointee(Node{"ddr4", "/phys/DIMM11", NodeType::kBoard})));

  ASSERT_TRUE(topology.uri_to_associated_node_map.contains(
      "/redfish/v1/Systems/system/Memory/12"));
  EXPECT_THAT(topology.uri_to_associated_node_map.at(
                  "/redfish/v1/Systems/system/Memory/12"),
              UnorderedElementsAre(
                  Pointee(Node{"ddr4", "/phys/DIMM12", NodeType::kBoard})));

  ASSERT_TRUE(topology.uri_to_associated_node_map.contains(
      "/redfish/v1/Systems/system/Memory/13"));
  EXPECT_THAT(topology.uri_to_associated_node_map.at(
                  "/redfish/v1/Systems/system/Memory/13"),
              UnorderedElementsAre(
                  Pointee(Node{"ddr4", "/phys/DIMM13", NodeType::kBoard})));

  ASSERT_TRUE(topology.uri_to_associated_node_map.contains(
      "/redfish/v1/Systems/system/Memory/14"));
  EXPECT_THAT(topology.uri_to_associated_node_map.at(
                  "/redfish/v1/Systems/system/Memory/14"),
              UnorderedElementsAre(
                  Pointee(Node{"ddr4", "/phys/DIMM14", NodeType::kBoard})));

  ASSERT_TRUE(topology.uri_to_associated_node_map.contains(
      "/redfish/v1/Systems/system/Memory/15"));
  EXPECT_THAT(topology.uri_to_associated_node_map.at(
                  "/redfish/v1/Systems/system/Memory/15"),
              UnorderedElementsAre(
                  Pointee(Node{"ddr4", "/phys/DIMM15", NodeType::kBoard})));

  ASSERT_TRUE(topology.uri_to_associated_node_map.contains(
      "/redfish/v1/Systems/system/Memory/16"));
  EXPECT_THAT(topology.uri_to_associated_node_map.at(
                  "/redfish/v1/Systems/system/Memory/16"),
              UnorderedElementsAre(
                  Pointee(Node{"ddr4", "/phys/DIMM16", NodeType::kBoard})));

  ASSERT_TRUE(topology.uri_to_associated_node_map.contains(
      "/redfish/v1/Systems/system/Memory/17"));
  EXPECT_THAT(topology.uri_to_associated_node_map.at(
                  "/redfish/v1/Systems/system/Memory/17"),
              UnorderedElementsAre(
                  Pointee(Node{"ddr4", "/phys/DIMM17", NodeType::kBoard})));

  ASSERT_TRUE(topology.uri_to_associated_node_map.contains(
      "/redfish/v1/Systems/system/Memory/18"));
  EXPECT_THAT(topology.uri_to_associated_node_map.at(
                  "/redfish/v1/Systems/system/Memory/18"),
              UnorderedElementsAre(
                  Pointee(Node{"ddr4", "/phys/DIMM18", NodeType::kBoard})));

  ASSERT_TRUE(topology.uri_to_associated_node_map.contains(
      "/redfish/v1/Systems/system/Memory/19"));
  EXPECT_THAT(topology.uri_to_associated_node_map.at(
                  "/redfish/v1/Systems/system/Memory/19"),
              UnorderedElementsAre(
                  Pointee(Node{"ddr4", "/phys/DIMM19", NodeType::kBoard})));

  ASSERT_TRUE(topology.uri_to_associated_node_map.contains(
      "/redfish/v1/Systems/system/Memory/20"));
  EXPECT_THAT(topology.uri_to_associated_node_map.at(
                  "/redfish/v1/Systems/system/Memory/20"),
              UnorderedElementsAre(
                  Pointee(Node{"ddr4", "/phys/DIMM20", NodeType::kBoard})));

  ASSERT_TRUE(topology.uri_to_associated_node_map.contains(
      "/redfish/v1/Systems/system/Memory/21"));
  EXPECT_THAT(topology.uri_to_associated_node_map.at(
                  "/redfish/v1/Systems/system/Memory/21"),
              UnorderedElementsAre(
                  Pointee(Node{"ddr4", "/phys/DIMM21", NodeType::kBoard})));

  ASSERT_TRUE(topology.uri_to_associated_node_map.contains(
      "/redfish/v1/Systems/system/Memory/22"));
  EXPECT_THAT(topology.uri_to_associated_node_map.at(
                  "/redfish/v1/Systems/system/Memory/22"),
              UnorderedElementsAre(
                  Pointee(Node{"ddr4", "/phys/DIMM22", NodeType::kBoard})));

  ASSERT_TRUE(topology.uri_to_associated_node_map.contains(
      "/redfish/v1/Systems/system/Memory/23"));
  EXPECT_THAT(topology.uri_to_associated_node_map.at(
                  "/redfish/v1/Systems/system/Memory/23"),
              UnorderedElementsAre(
                  Pointee(Node{"ddr4", "/phys/DIMM23", NodeType::kBoard})));
}

TEST(RawInterfaceTestWithMockup, NodesMatch) {
  TestingMockupServer mockup("indus_hmb_cn/mockup.shar");
  auto raw_intf = mockup.RedfishClientInterface();

  NodeTopology topology1 = CreateTopologyFromRedfish(raw_intf.get());
  NodeTopology topology2 = CreateTopologyFromRedfish(raw_intf.get());

  NodeTopologiesHaveTheSameNodes(topology1, topology2);
}

}  // namespace
}  // namespace libredfish
