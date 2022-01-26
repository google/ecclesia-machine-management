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

#include "ecclesia/lib/redfish/topology_v2.h"

#include <memory>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/str_cat.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/redfish/node_topology.h"
#include "ecclesia/lib/redfish/test_mockup.h"
#include "ecclesia/lib/redfish/testing/fake_redfish_server.h"
#include "ecclesia/lib/redfish/testing/node_topology_testing.h"
#include "ecclesia/lib/redfish/types.h"

namespace ecclesia {
namespace {

using ::testing::Contains;
using ::testing::Not;
using ::testing::Pointwise;

void CheckAgainstTestingMockupFullDevpaths(const NodeTopology &topology) {
  const std::vector<Node> expected_nodes = {
      Node{"root", "/phys", NodeType::kBoard},
      Node{"child1", "/phys/C1", NodeType::kBoard},
      Node{"child2", "/phys/C2", NodeType::kBoard},
      Node{"ssd", "/phys/SSD", NodeType::kBoard},
      Node{"cpu", "/phys/CPU", NodeType::kBoard},
      Node{"memory", "/phys/C1/DIMM", NodeType::kBoard},
      Node{"dangling_cable", "/phys/C1/QSFP", NodeType::kCable},
      Node{"expansion_cable", "/phys/C2/HDMI", NodeType::kCable},
      Node{"controller", "/phys/SSD:device:controller", NodeType::kDevice},
      Node{"drive", "/phys/SSD:device:drive", NodeType::kDevice},
      Node{"expansion_tray", "/phys/C2/HDMI/DOWNLINK", NodeType::kBoard},
      Node{"expansion_child", "/phys/C2/HDMI/DOWNLINK/E1", NodeType::kBoard}};

  std::vector<Node> actual_nodes;
  for (const auto &node : topology.nodes) {
    actual_nodes.push_back(*node);
  }

  EXPECT_THAT(actual_nodes, Pointwise(RedfishNodeEqId(), expected_nodes));
}

TEST(RawInterfaceTestWithMockup, TestingMockupNodesArePopulated) {
  TestingMockupServer mockup("topology_v2_testing/mockup.shar");
  auto raw_intf = mockup.RedfishClientInterface();
  CheckAgainstTestingMockupFullDevpaths(
      CreateTopologyFromRedfishV2(raw_intf.get()));
}

TEST(RawInterfaceTestWithPatchedMockup, TestingMockupFindingRootChassis) {
  ecclesia::FakeRedfishServer mockup(
      "topology_v2_testing/mockup.shar",
      absl::StrCat(ecclesia::GetTestTempUdsDirectory(), "/mockup.socket"));
  auto raw_intf = mockup.RedfishClientInterface();

  {
    // Reorder chassis so that root chassis has to be found via Link traversal
    mockup.AddHttpGetHandlerWithData("/redfish/v1/Chassis", R"json(
      {
        "@odata.id": "/redfish/v1/Chassis",
        "@odata.type": "#ChassisCollection.ChassisCollection",
        "Members": [
          {
            "@odata.id": "/redfish/v1/Chassis/child2"
          },
          {
            "@odata.id": "/redfish/v1/Chassis/root"
          },
          {
            "@odata.id": "/redfish/v1/Chassis/child1"
          },
          {
            "@odata.id": "/redfish/v1/Chassis/expansion_tray"
          },
          {
            "@odata.id": "/redfish/v1/Chassis/expansion_child"
          }
        ],
        "Members@odata.count": 5,
        "Name": "Chassis Collection"
      }
    )json");

    CheckAgainstTestingMockupFullDevpaths(
        CreateTopologyFromRedfishV2(raw_intf.get()));

    mockup.ClearHandlers();
  }
  {
    // Reorder chassis so that root chassis has to be found via Link traversal
    // and via existing Cabling
    mockup.AddHttpGetHandlerWithData("/redfish/v1/Chassis", R"json(
      {
        "@odata.id": "/redfish/v1/Chassis",
        "@odata.type": "#ChassisCollection.ChassisCollection",
        "Members": [
          {
            "@odata.id": "/redfish/v1/Chassis/expansion_tray"
          },
          {
            "@odata.id": "/redfish/v1/Chassis/child2"
          },
          {
            "@odata.id": "/redfish/v1/Chassis/root"
          },
          {
            "@odata.id": "/redfish/v1/Chassis/child1"
          },
          {
            "@odata.id": "/redfish/v1/Chassis/expansion_child"
          }
        ],
        "Members@odata.count": 5,
        "Name": "Chassis Collection"
      }
    )json");

    CheckAgainstTestingMockupFullDevpaths(
        CreateTopologyFromRedfishV2(raw_intf.get()));

    mockup.ClearHandlers();
  }
  {
    // Reorder chassis so that root chassis has to be found via Link traversal
    // and via existing Cabling (multi-level)
    mockup.AddHttpGetHandlerWithData("/redfish/v1/Chassis", R"json(
      {
        "@odata.id": "/redfish/v1/Chassis",
        "@odata.type": "#ChassisCollection.ChassisCollection",
        "Members": [
          {
            "@odata.id": "/redfish/v1/Chassis/expansion_child"
          },
          {
            "@odata.id": "/redfish/v1/Chassis/child2"
          },
          {
            "@odata.id": "/redfish/v1/Chassis/root"
          },
          {
            "@odata.id": "/redfish/v1/Chassis/child1"
          },
          {
            "@odata.id": "/redfish/v1/Chassis/expansion_tray"
          }
        ],
        "Members@odata.count": 5,
        "Name": "Chassis Collection"
      }
    )json");

    CheckAgainstTestingMockupFullDevpaths(
        CreateTopologyFromRedfishV2(raw_intf.get()));

    mockup.ClearHandlers();
  }
  {
    // No Chassis to find a root from
    mockup.AddHttpGetHandlerWithData("/redfish/v1/Chassis", R"json(
      {
        "@odata.id": "/redfish/v1/Chassis",
        "@odata.type": "#ChassisCollection.ChassisCollection",
        "Members": [],
        "Members@odata.count": 0,
        "Name": "Chassis Collection"
      }
    )json");

    auto topology = CreateTopologyFromRedfishV2(raw_intf.get());
    EXPECT_TRUE(topology.nodes.empty());

    mockup.ClearHandlers();
  }
}

TEST(RawInterfaceTestWithPatchedMockup, TestingMockupBrokenOrCircularLink) {
  ecclesia::FakeRedfishServer mockup(
      "topology_v2_testing/mockup.shar",
      absl::StrCat(ecclesia::GetTestTempUdsDirectory(), "/mockup.socket"));
  auto raw_intf = mockup.RedfishClientInterface();

  {
    // Add broken/non-existent link to Drive
    mockup.AddHttpGetHandlerWithData("/redfish/v1/Chassis/child2", R"json(
      {
        "@odata.id": "/redfish/v1/Chassis/child2",
        "@odata.type": "#Chassis.v1_14_0.Chassis",
        "ChassisType": "RackMount",
        "Id": "child2",
        "Links": {
          "ContainedBy": {
            "@odata.id": "/redfish/v1/Chassis/root"
          },
          "Drives": [{
            "@odata.id": "/redfish/v1/link/does/not/exist"
          }]
        },
        "Location": {
          "PartLocation": {
            "LocationType": "Slot",
            "ServiceLabel": "C2"
          }
        },
        "Name": "child2"
      }
    )json");

    CheckAgainstTestingMockupFullDevpaths(
        CreateTopologyFromRedfishV2(raw_intf.get()));

    mockup.ClearHandlers();
  }
  {
    // Add extraneous link to Storage that's already assigned a devpath
    mockup.AddHttpGetHandlerWithData("/redfish/v1/Chassis/child2", R"json(
      {
        "@odata.id": "/redfish/v1/Chassis/child2",
        "@odata.type": "#Chassis.v1_14_0.Chassis",
        "ChassisType": "RackMount",
        "Id": "child2",
        "Links": {
          "ContainedBy": {
            "@odata.id": "/redfish/v1/Chassis/root"
          },
          "Storage": [{
            "@odata.id": "/redfish/v1/Systems/system/Storage/1"
          }]
        },
        "Location": {
          "PartLocation": {
            "LocationType": "Slot",
            "ServiceLabel": "C2"
          }
        },
        "Name": "child2"
      }
    )json");

    CheckAgainstTestingMockupFullDevpaths(
        CreateTopologyFromRedfishV2(raw_intf.get()));

    mockup.ClearHandlers();
  }
}

// This test makes sure the node names from the created topology match
// expectations.
TEST(RawInterfaceTestWithMockup, TestingNodeName) {
  ecclesia::FakeRedfishServer mockup(
      "topology_v2_testing/mockup.shar",
      absl::StrCat(ecclesia::GetTestTempUdsDirectory(), "/mockup.socket"));
  auto raw_intf = mockup.RedfishClientInterface();

  const NodeTopology constructed_topology =
      CreateTopologyFromRedfishV2(raw_intf.get());

  std::vector<std::string> node_names;
  for (const auto &node : constructed_topology.nodes) {
    node_names.push_back(node->name);
  }

  EXPECT_THAT(node_names, Contains("expansion_tray"));
  EXPECT_THAT(node_names, Contains("expansion_child"));
  EXPECT_THAT(node_names, Not(Contains("Expansion Tray ")));
  EXPECT_THAT(node_names, Not(Contains(" Expansion Child ")));
}

TEST(RawInterfaceTestWithMockup, TestingLocationTypes) {
  ecclesia::FakeRedfishServer mockup(
      "topology_v2_testing/mockup.shar",
      absl::StrCat(ecclesia::GetTestTempUdsDirectory(), "/mockup.socket"));
  auto raw_intf = mockup.RedfishClientInterface();

  // Change the LocationType to make sure the topology can be correctly
  // constructed as well.
  mockup.AddHttpGetHandlerWithData("/redfish/v1/Chassis/child1", R"json(
      {
        "@odata.id": "/redfish/v1/Chassis/child1",
        "@odata.type": "#Chassis.v1_14_0.Chassis",
        "ChassisType": "RackMount",
        "Id": "child1",
        "Links": {
          "ContainedBy": {
            "@odata.id": "/redfish/v1/Chassis/root"
          }
        },
        "Location": {
          "PartLocation": {
            "LocationType": "Bay",
            "ServiceLabel": "C1"
          }
        },
        "Name": "child1",
        "Memory": {
          "@odata.id": "/redfish/v1/Systems/system/Memory"
        }
      }
    )json");

  CheckAgainstTestingMockupFullDevpaths(
      CreateTopologyFromRedfishV2(raw_intf.get()));

  mockup.ClearHandlers();
}

}  // namespace
}  // namespace ecclesia
