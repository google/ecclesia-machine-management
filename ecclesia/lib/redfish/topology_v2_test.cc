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
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/functional/function_ref.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/location.h"
#include "ecclesia/lib/redfish/node_topology.h"
#include "ecclesia/lib/redfish/test_mockup.h"
#include "ecclesia/lib/redfish/testing/fake_redfish_server.h"
#include "ecclesia/lib/redfish/testing/node_topology_testing.h"
#include "ecclesia/lib/redfish/transport/cache.h"
#include "ecclesia/lib/redfish/transport/http_redfish_intf.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/redfish/types.h"
#include "tensorflow_serving/util/net_http/public/response_code_enum.h"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"

namespace ecclesia {
namespace {

using ::tensorflow::serving::net_http::HTTPStatusCode;
using ::tensorflow::serving::net_http::ServerRequestInterface;
using ::testing::_;
using ::testing::Contains;
using ::testing::Not;
using ::testing::Pointee;
using ::testing::Pointwise;

using NodeTopologyBuilderType =
    absl::FunctionRef<NodeTopology(RedfishInterface*)>;

void CheckAgainstTestingMockupFullDevpaths(const NodeTopology& topology) {
  const std::vector<Node> expected_nodes = {
      Node{.name = "root",
           .model = "root",
           .local_devpath = "/phys",
           .type = NodeType::kBoard,
           .supplemental_location_info = std::nullopt,
           .replaceable = true},
      Node{.name = "child1",
           .model = "child1",
           .local_devpath = "/phys/C1",
           .type = NodeType::kBoard,
           .supplemental_location_info =
               SupplementalLocationInfo{.service_label = "C1",
                                        .part_location_context = ""},
           .replaceable = true},
      Node{.name = "child2",
           .model = "child2",
           .local_devpath = "/phys/C2",
           .type = NodeType::kBoard,
           .supplemental_location_info =
               SupplementalLocationInfo{.service_label = "C2",
                                        .part_location_context = ""},
           .replaceable = true},
      Node{.name = "ssd",
           .model = "ssd",
           .local_devpath = "/phys/SSD",
           .type = NodeType::kBoard,
           .supplemental_location_info =
               SupplementalLocationInfo{.service_label = "SSD",
                                        .part_location_context = ""},
           .replaceable = true},
      Node{.name = "cpu",
           .model = "cpu",
           .local_devpath = "/phys/CPU",
           .type = NodeType::kBoard,
           .supplemental_location_info =
               SupplementalLocationInfo{.service_label = "CPU",
                                        .part_location_context = ""},
           .replaceable = true},
      Node{.name = "memory",
           .model = "memory",
           .local_devpath = "/phys/C1/DIMM",
           .type = NodeType::kBoard,
           .supplemental_location_info =
               SupplementalLocationInfo{.service_label = "DIMM",
                                        .part_location_context = ""},
           .replaceable = true},
      Node{.name = "dangling_cable",
           .model = "dangling_cable",
           .local_devpath = "/phys/C1/QSFP",
           .type = NodeType::kCable,
           .supplemental_location_info =
               SupplementalLocationInfo{.service_label = "QSFP",
                                        .part_location_context = "C1"},
           .replaceable = true},
      Node{.name = "trusted_component",
           .model = "trusted_component",
           .local_devpath = "/phys/C2:device:trusted_component",
           .type = NodeType::kDevice,
           .supplemental_location_info =
               SupplementalLocationInfo{.service_label = "",
                                        .part_location_context = "C2_ROT"},
           .replaceable = false},
      Node{.name = "expansion_cable",
           .model = "expansion_cable",
           .local_devpath = "/phys/C2/HDMI",
           .type = NodeType::kCable,
           .supplemental_location_info =
               SupplementalLocationInfo{.service_label = "HDMI",
                                        .part_location_context = "C2"},
           .replaceable = true},
      Node{.name = "controller",
           .model = "controller",
           .local_devpath = "/phys/SSD:device:controller",
           .type = NodeType::kDevice,
           .supplemental_location_info =
               SupplementalLocationInfo{.service_label = "",
                                        .part_location_context = "SSD"},
           .replaceable = false},
      Node{.name = "drive",
           .model = "drive",
           .local_devpath = "/phys/SSD:device:drive",
           .type = NodeType::kDevice,
           .supplemental_location_info =
               SupplementalLocationInfo{.service_label = "",
                                        .part_location_context = "SSD"},
           .replaceable = false},
      Node{.name = "expansion_tray",
           .model = "expansion_tray",
           .local_devpath = "/phys/C2/HDMI/DOWNLINK",
           .type = NodeType::kBoard,
           .supplemental_location_info =
               SupplementalLocationInfo{.service_label = "DOWNLINK",
                                        .part_location_context = "C2/HDMI"},
           .replaceable = true},
      Node{.name = "expansion_child",
           .model = "expansion_child",
           .local_devpath = "/phys/C2/HDMI/DOWNLINK/E1",
           .type = NodeType::kBoard,
           .supplemental_location_info =
               SupplementalLocationInfo{
                   .service_label = "E1",
                   .part_location_context = "C2/HDMI/DOWNLINK"},
           .replaceable = false}};

  std::vector<Node> actual_nodes;
  actual_nodes.reserve(topology.nodes.size());
  for (const auto& node : topology.nodes) {
    actual_nodes.push_back(*node);
  }

  EXPECT_THAT(actual_nodes, Pointwise(RedfishNodeEqId(), expected_nodes));
}

void CheckAgainstTestingMultiHostFullDevpaths(const NodeTopology& topology) {
  const std::vector<Node> expected_nodes = {
      Node{.name = "multi1",
           .model = "multi1",
           .local_devpath = "/phys",
           .type = NodeType::kBoard,
           .replaceable = true},
      Node{.name = "memory_m1",
           .model = "memory_m1",
           .local_devpath = "/phys/DIMM0",
           .type = NodeType::kBoard,
           .supplemental_location_info =
               SupplementalLocationInfo{.service_label = "DIMM0"},
           .replaceable = true},
      Node{.name = "cpu_m1",
           .model = "cpu_m1",
           .local_devpath = "/phys/CPU0",
           .type = NodeType::kBoard,
           .supplemental_location_info =
               SupplementalLocationInfo{.service_label = "CPU0"},
           .replaceable = true},
      Node{.name = "memory_m2",
           .model = "memory_m2",
           .local_devpath = "/phys/DIMM1",
           .type = NodeType::kBoard,
           .supplemental_location_info =
               SupplementalLocationInfo{.service_label = "DIMM1"},
           .replaceable = true},
      Node{.name = "cpu_m2",
           .model = "cpu_m2",
           .local_devpath = "/phys/CPU1",
           .type = NodeType::kBoard,
           .supplemental_location_info =
               SupplementalLocationInfo{.service_label = "CPU1"},
           .replaceable = true},
      Node{.name = "cable1",
           .model = "cable1",
           .local_devpath = "/phys/PE1",
           .type = NodeType::kCable,
           .supplemental_location_info =
               SupplementalLocationInfo{.service_label = "PE1"},
           .replaceable = true},
      Node{.name = "multi2",
           .model = "multi2",
           .local_devpath = "/phys/PE1/DOWNLINK",
           .type = NodeType::kBoard,
           .supplemental_location_info =
               SupplementalLocationInfo{.service_label = "DOWNLINK"},
           .replaceable = true}};

  std::vector<Node> actual_nodes;
  actual_nodes.reserve(topology.nodes.size());
  for (const auto& node : topology.nodes) {
    actual_nodes.push_back(*node);
  }

  EXPECT_THAT(actual_nodes, Pointwise(RedfishNodeEqId(), expected_nodes));
}

TEST(TopologyTestRunner, TestingMockupNodesArePopulated) {
  TestingMockupServer mockup("topology_v2_testing/mockup.shar");
  auto raw_intf = mockup.RedfishClientInterface();
  CheckAgainstTestingMockupFullDevpaths(
      CreateTopologyFromRedfishV2(raw_intf.get()));
}

TEST(TopologyTestRunner, TestingMockupFindingRootChassis) {
  FakeRedfishServer mockup("topology_v2_testing/mockup.shar");
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
}

TEST(TopologyTestRunner, TestingMockupFindingRootChassisWithExpand) {
  // Verifies that the crawler can use expand to find the root chassis.
  FakeRedfishServer mockup("topology_v2_testing/mockup.shar");
  mockup.EnableExpandGetHandler();
  auto raw_intf = mockup.RedfishClientInterface();

  {
    // Reorder chassis so that root chassis has to be found via Link traversal
    // and via existing Cabling (multi-level)
    mockup.AddHttpGetHandlerWithData("/redfish/v1/Chassis?$expand=.($levels=1)",
                                     R"json(
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
    mockup.AddHttpGetHandlerWithData("/redfish/v1/Chassis?$expand=.($levels=1)",
                                     R"json(
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

TEST(TopologyTestRunner, TestingMockupFindingRootChassisNoExpand) {
  // Verifies that the crawler safely defaults to "no expand" and successfully
  // builds the topology via manual traversal.
  FakeRedfishServer mockup("topology_v2_testing/mockup.shar");

  // Construct a RedfishInterface with expand disabled forcefully.
  std::unique_ptr<RedfishTransport> transport = mockup.RedfishClientTransport();
  std::unique_ptr<RedfishInterface> intf = NewHttpInterfaceWithoutExpand(
      std::move(transport), NullCache::Create, RedfishInterface::kTrusted);

  mockup.AddHttpGetHandlerWithData("/redfish/v1/Chassis", R"json(
    {
      "@odata.id": "/redfish/v1/Chassis",
      "@odata.type": "#ChassisCollection.ChassisCollection",
      "Members": [
        {
          "@odata.id": "/redfish/v1/Chassis/root"
        }
      ],
      "Members@odata.count": 1,
      "Name": "Chassis Collection"
    }
  )json");

  // Register a handler for the expanded URI that returns an error. If the
  // topology crawler tries to use expand, it will hit this handler and fail.
  mockup.AddHttpGetHandler("/redfish/v1/Chassis?$expand=.($levels=1)",
                           [&](ServerRequestInterface* req) -> void {
                             req->ReplyWithStatus(HTTPStatusCode::BAD_REQUEST);
                           });

  NodeTopology topology = CreateTopologyFromRedfishV2(intf.get());

  // Verify that we still managed to build a topology (meaning we successfully
  // queried "/redfish/v1/Chassis" and did not hit the error handler for
  // expanded URI).
  EXPECT_FALSE(topology.nodes.empty());
  EXPECT_THAT(topology.nodes, Contains(Pointee(RedfishNodeIdIs("root", _, _))));
}

TEST(TopologyTestRunner, TestingMockupFindingRootChassisFeaturesMissing) {
  // Verifies that the crawler safely defaults to "no expand" and successfully
  // builds the topology via manual traversal.
  FakeRedfishServer mockup("topology_v2_testing/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = mockup.RedfishClientInterface();

  // Override /redfish/v1 to return a response WITHOUT
  // ProtocolFeaturesSupported.
  mockup.AddHttpGetHandlerWithData("/redfish/v1", R"json({
      "@odata.id": "/redfish/v1",
      "@odata.type": "#ServiceRoot.v1_5_0.ServiceRoot",
      "Chassis": {
        "@odata.id": "/redfish/v1/Chassis"
      },
      "Id": "RootService",
      "Name": "Root Service",
      "RedfishVersion": "1.6.1"
  })json");

  mockup.AddHttpGetHandlerWithData("/redfish/v1/Chassis", R"json({
      "@odata.id": "/redfish/v1/Chassis",
      "@odata.type": "#ChassisCollection.ChassisCollection",
      "Members": [
        {
          "@odata.id": "/redfish/v1/Chassis/root"
        }
      ],
      "Members@odata.count": 1,
      "Name": "Chassis Collection"
  })json");

  // Register a handler for the expanded URI that returns an error.
  // If the topology crawler tries to use expand (which it shouldn't because
  // features are missing), it will hit this handler and fail.
  mockup.AddHttpGetHandler("/redfish/v1/Chassis?$expand=.($levels=1)",
                           [&](ServerRequestInterface* req) -> void {
                             req->ReplyWithStatus(HTTPStatusCode::BAD_REQUEST);
                           });

  NodeTopology topology = CreateTopologyFromRedfishV2(intf.get());

  // Verify that we still managed to build a topology.
  EXPECT_FALSE(topology.nodes.empty());
  EXPECT_THAT(topology.nodes, Contains(Pointee(RedfishNodeIdIs("root", _, _))));
}

TEST(TopologyTestRunner, TestingMockupFindingRootChassisGetRootFailure) {
  // Verifies that the crawler doesn't abort immediately on `GetRoot()` failure
  // and still attempts to build the topology.
  FakeRedfishServer mockup("topology_v2_testing/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = mockup.RedfishClientInterface();

  // Override /redfish/v1 to return an error.
  mockup.AddHttpGetHandler("/redfish/v1",
                           [&](ServerRequestInterface* req) -> void {
                             req->ReplyWithStatus(HTTPStatusCode::ERROR);
                           });

  // Since GetRoot failed, features are not populated, so we keep expand.
  // We should request WITH expand.
  mockup.AddHttpGetHandlerWithData("/redfish/v1/Chassis?$expand=.($levels=1)",
                                   R"json({
                                    "@odata.id": "/redfish/v1/Chassis",
                                    "@odata.type": "#ChassisCollection.ChassisCollection",
                                    "Members": [
                                      {
                                        "@odata.id": "/redfish/v1/Chassis/root"
                                      }
                                    ],
                                    "Members@odata.count": 1,
                                    "Name": "Chassis Collection"
                                  })json");

  NodeTopology topology = CreateTopologyFromRedfishV2(intf.get());

  // Verify that we still managed to build a topology because we used expand
  // which succeeded.
  EXPECT_FALSE(topology.nodes.empty());
  EXPECT_THAT(topology.nodes, Contains(Pointee(RedfishNodeIdIs("root", _, _))));
}

TEST(TopologyTestRunner, TestingMockupBrokenOrCircularLink) {
  FakeRedfishServer mockup("topology_v2_testing/mockup.shar");
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
        "Name": "child2",
        "TrustedComponents": {
          "@odata.id": "/redfish/v1/Chassis/child2/TrustedComponents"
        }
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
        "Name": "child2",
        "TrustedComponents": {
          "@odata.id": "/redfish/v1/Chassis/child2/TrustedComponents"
        }
      }
    )json");

    CheckAgainstTestingMockupFullDevpaths(
        CreateTopologyFromRedfishV2(raw_intf.get()));

    mockup.ClearHandlers();
  }
}

// This test makes sure the node names from the created topology match
// expectations.
TEST(TopologyTestRunner, TestingNodeName) {
  FakeRedfishServer mockup("topology_v2_testing/mockup.shar");
  auto raw_intf = mockup.RedfishClientInterface();

  const NodeTopology constructed_topology =
      CreateTopologyFromRedfishV2(raw_intf.get());

  std::vector<std::string> node_names;
  node_names.reserve(constructed_topology.nodes.size());
  for (const auto& node : constructed_topology.nodes) {
    node_names.push_back(node->name);
  }

  EXPECT_THAT(node_names, Contains("expansion_tray"));
  EXPECT_THAT(node_names, Contains("expansion_child"));
  EXPECT_THAT(node_names, Not(Contains("Expansion Tray ")));
  EXPECT_THAT(node_names, Not(Contains(" Expansion Child ")));
}

TEST(TopologyTestRunner, TestingLocationTypes) {
  FakeRedfishServer mockup("topology_v2_testing/mockup.shar");
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

TEST(TopologyTestRunner, GoogleRootCoexistsWithRedfishRoot) {
  FakeRedfishServer mockup("features/component_integrity/mockup.shar");
  auto raw_intf = mockup.RedfishClientInterface();
  NodeTopology topology = CreateTopologyFromRedfishV2(raw_intf.get());

  const std::vector<Node> expected_nodes = {
      Node{.name = "root",
           .model = "root",
           .local_devpath = "/phys",
           .type = NodeType::kBoard,
           .replaceable = true},
      Node{.name = "erot-gpu1",
           .model = "erot-gpu1",
           .local_devpath = "/phys:device:erot-gpu1",
           .type = NodeType::kDevice},
      Node{.name = "erot-gpu2",
           .model = "erot-gpu2",
           .local_devpath = "/phys:device:erot-gpu2",
           .type = NodeType::kDevice},
      Node{.name = "hoth",
           .model = "hoth",
           .local_devpath = "/phys:device:hoth",
           .type = NodeType::kDevice,
           .supplemental_location_info =
               SupplementalLocationInfo{.service_label = "Hoth",
                                        .part_location_context = ""}},
      Node{.name = "tpu0",
           .model = "tpu0",
           .local_devpath = "/phys:device:tpu0",
           .type = NodeType::kDevice,
           .supplemental_location_info =
               SupplementalLocationInfo{.service_label = "tpu0",
                                        .part_location_context = ""}},
  };

  std::vector<Node> actual_nodes;
  actual_nodes.reserve(topology.nodes.size());
  for (const auto& node : topology.nodes) {
    actual_nodes.push_back(*node);
  }

  EXPECT_THAT(actual_nodes, Pointwise(RedfishNodeEqId(), expected_nodes));
}

TEST(TopologyTestRunner, UriUnqueryableFirstChassisBad) {
  FakeRedfishServer mockup("topology_v2_testing/mockup.shar");
  auto raw_intf = mockup.RedfishClientInterface();
  // If the first Chassis is unqueryable.
  mockup.AddHttpGetHandler("/redfish/v1/Chassis/child1",
                           [&](ServerRequestInterface* req) {
                             req->ReplyWithStatus(HTTPStatusCode::REQUEST_TO);
                           });
  NodeTopology topology = CreateTopologyFromRedfishV2(raw_intf.get());
  const std::vector<Node> expected_nodes = {
      Node{.name = "root",
           .model = "root",
           .local_devpath = "/phys",
           .type = NodeType::kBoard,
           .supplemental_location_info = std::nullopt,
           .replaceable = true},
      Node{.name = "child2",
           .model = "child2",
           .local_devpath = "/phys/C2",
           .type = NodeType::kBoard,
           .supplemental_location_info =
               SupplementalLocationInfo{.service_label = "C2",
                                        .part_location_context = ""},
           .replaceable = true},
      Node{.name = "ssd",
           .model = "ssd",
           .local_devpath = "/phys/SSD",
           .type = NodeType::kBoard,
           .supplemental_location_info =
               SupplementalLocationInfo{.service_label = "SSD",
                                        .part_location_context = ""},
           .replaceable = true},
      Node{.name = "cpu",
           .model = "cpu",
           .local_devpath = "/phys/CPU",
           .type = NodeType::kBoard,
           .supplemental_location_info =
               SupplementalLocationInfo{.service_label = "CPU",
                                        .part_location_context = ""},
           .replaceable = true},
      Node{.name = "trusted_component",
           .model = "trusted_component",
           .local_devpath = "/phys/C2:device:trusted_component",
           .type = NodeType::kDevice,
           .supplemental_location_info =
               SupplementalLocationInfo{.service_label = "",
                                        .part_location_context = "C2_ROT"},
           .replaceable = false},
      Node{.name = "expansion_cable",
           .model = "expansion_cable",
           .local_devpath = "/phys/C2/HDMI",
           .type = NodeType::kCable,
           .supplemental_location_info =
               SupplementalLocationInfo{.service_label = "HDMI",
                                        .part_location_context = "C2"},
           .replaceable = true},
      Node{.name = "controller",
           .model = "controller",
           .local_devpath = "/phys/SSD:device:controller",
           .type = NodeType::kDevice,
           .supplemental_location_info =
               SupplementalLocationInfo{.service_label = "",
                                        .part_location_context = "SSD"},
           .replaceable = false},
      Node{.name = "drive",
           .model = "drive",
           .local_devpath = "/phys/SSD:device:drive",
           .type = NodeType::kDevice,
           .supplemental_location_info =
               SupplementalLocationInfo{.service_label = "",
                                        .part_location_context = "SSD"},
           .replaceable = false},
      Node{.name = "expansion_tray",
           .model = "expansion_tray",
           .local_devpath = "/phys/C2/HDMI/DOWNLINK",
           .type = NodeType::kBoard,
           .supplemental_location_info =
               SupplementalLocationInfo{.service_label = "DOWNLINK",
                                        .part_location_context = "C2/HDMI"},
           .replaceable = true},
      Node{.name = "expansion_child",
           .model = "expansion_child",
           .local_devpath = "/phys/C2/HDMI/DOWNLINK/E1",
           .type = NodeType::kBoard,
           .supplemental_location_info =
               SupplementalLocationInfo{
                   .service_label = "E1",
                   .part_location_context = "C2/HDMI/DOWNLINK"},
           .replaceable = false}};

  std::vector<Node> actual_nodes;
  actual_nodes.reserve(topology.nodes.size());
  for (const auto& node : topology.nodes) {
    actual_nodes.push_back(*node);
  }
  EXPECT_THAT(actual_nodes, Pointwise(RedfishNodeEqId(), expected_nodes));
}

TEST(TopologyTestRunner, UriUnqueryableRootChassisBad) {
  FakeRedfishServer mockup("topology_v2_testing/mockup.shar");
  auto raw_intf = mockup.RedfishClientInterface();
  // If the root Chassis is unqueryable.
  mockup.AddHttpGetHandler("/redfish/v1/Chassis/root",
                           [&](ServerRequestInterface* req) {
                             req->ReplyWithStatus(HTTPStatusCode::REQUEST_TO);
                           });
  mockup.AddHttpGetHandlerWithData("/redfish/v1/Chassis?$expand=.($levels=1)",
                                   R"json(
    {
      "@odata.id": "/redfish/v1/Chassis",
      "@odata.type": "#ChassisCollection.ChassisCollection",
      "Members": [
        {
          "@odata.id": "/redfish/v1/Chassis/child1"
        },
        {
          "@odata.id": "/redfish/v1/Chassis/child2"
        },
        {
          "@odata.id": "/redfish/v1/Chassis/expansion_tray"
        },
        {
          "@odata.id": "/redfish/v1/Chassis/expansion_child"
        }
      ],
      "Members@odata.count": 4,
      "Name": "Chassis Collection"
    }
  )json");
  NodeTopology topology = CreateTopologyFromRedfishV2(raw_intf.get());
  const std::vector<Node> expected_nodes = {
      Node{.name = "child1",
           .model = "child1",
           .local_devpath = "/phys",
           .type = NodeType::kBoard,
           .supplemental_location_info =
               SupplementalLocationInfo{.service_label = "C1",
                                        .part_location_context = ""},
           .replaceable = true},
      Node{.name = "memory",
           .model = "memory",
           .local_devpath = "/phys/DIMM",
           .type = NodeType::kBoard,
           .supplemental_location_info =
               SupplementalLocationInfo{.service_label = "DIMM",
                                        .part_location_context = ""},
           .replaceable = true},
      Node{.name = "dangling_cable",
           .model = "dangling_cable",
           .local_devpath = "/phys/QSFP",
           .type = NodeType::kCable,
           .supplemental_location_info =
               SupplementalLocationInfo{.service_label = "QSFP",
                                        .part_location_context = "C1"},
           .replaceable = true}};
  std::vector<Node> actual_nodes;
  actual_nodes.reserve(topology.nodes.size());
  for (const auto& node : topology.nodes) {
    actual_nodes.push_back(*node);
  }
  EXPECT_THAT(actual_nodes, Pointwise(RedfishNodeEqId(), expected_nodes));
}

TEST(TopologyTestRunner, UriUnqueryableAllChassisBad) {
  FakeRedfishServer mockup("topology_v2_testing/mockup.shar");
  auto raw_intf = mockup.RedfishClientInterface();
  // If all Chassis are unqueryable.
  mockup.AddHttpGetHandlerWithData("/redfish/v1/Chassis?$expand=.($levels=1)",
                                   R"json(
    {
      "@odata.id": "/redfish/v1/Chassis",
      "@odata.type": "#ChassisCollection.ChassisCollection",
      "Members": [
      ],
      "Members@odata.count": 0,
      "Name": "Chassis Collection"
    }
  )json");
  mockup.AddHttpGetHandler("/redfish/v1/Chassis/root",
                           [&](ServerRequestInterface* req) {
                             req->ReplyWithStatus(HTTPStatusCode::REQUEST_TO);
                           });
  mockup.AddHttpGetHandler("/redfish/v1/Chassis/child1",
                           [&](ServerRequestInterface* req) {
                             req->ReplyWithStatus(HTTPStatusCode::REQUEST_TO);
                           });
  mockup.AddHttpGetHandler("/redfish/v1/Chassis/child2",
                           [&](ServerRequestInterface* req) {
                             req->ReplyWithStatus(HTTPStatusCode::REQUEST_TO);
                           });
  mockup.AddHttpGetHandler("/redfish/v1/Chassis/expansion_tray",
                           [&](ServerRequestInterface* req) {
                             req->ReplyWithStatus(HTTPStatusCode::REQUEST_TO);
                           });
  mockup.AddHttpGetHandler("/redfish/v1/Chassis/expansion_child",
                           [&](ServerRequestInterface* req) {
                             req->ReplyWithStatus(HTTPStatusCode::REQUEST_TO);
                           });
  NodeTopology topology = CreateTopologyFromRedfishV2(raw_intf.get());
  EXPECT_TRUE(topology.nodes.empty());
}

TEST(TopologyTestRunner, UriUnqueryableChassisCollectionBad) {
  FakeRedfishServer mockup("topology_v2_testing/mockup.shar");
  mockup.EnableExpandGetHandler();
  auto raw_intf = mockup.RedfishClientInterface();
  // If Chassis Collection is unqueryable.
  mockup.AddHttpGetHandler("/redfish/v1/Chassis?$expand=.($levels=1)",
                           [&](ServerRequestInterface* req) {
                             req->ReplyWithStatus(HTTPStatusCode::UNAUTHORIZED);
                           });
  NodeTopology topology = CreateTopologyFromRedfishV2(raw_intf.get());
  EXPECT_TRUE(topology.nodes.empty());
}

TEST(TopologyTestRunner, TestingReplaceable) {
  TestingMockupServer mockup("topology_v2_testing/mockup.shar");
  auto raw_intf = mockup.RedfishClientInterface();

  NodeTopology topology = CreateTopologyFromRedfishV2(raw_intf.get());

  // Make sure all nodes are created properly
  CheckAgainstTestingMockupFullDevpaths(topology);

  // Get the expansion child node and check its replaceable field
  auto it = topology.devpath_to_node_map.find("/phys/C2/HDMI/DOWNLINK/E1");
  ASSERT_TRUE(it != topology.devpath_to_node_map.end());
  EXPECT_FALSE(it->second->replaceable);
}

TEST(TopologyTestRunner, TestingOemReplaceable) {
  TestingMockupServer mockup("topology_v2_testing/mockup.shar");
  auto raw_intf = mockup.RedfishClientInterface();

  NodeTopology topology = CreateTopologyFromRedfishV2(raw_intf.get());

  // Make sure all nodes are created properly
  CheckAgainstTestingMockupFullDevpaths(topology);

  // Get the memory node and check its replaceable field
  auto it = topology.devpath_to_node_map.find("/phys/C1/DIMM");
  ASSERT_TRUE(it != topology.devpath_to_node_map.end());
  EXPECT_TRUE(it->second->replaceable);
}

TEST(TopologyTestRunner, TestingConfigsOption) {
  TestingMockupServer mockup("topology_v2_testing/mockup.shar");
  auto raw_intf = mockup.RedfishClientInterface();

  NodeTopology topology =
      CreateTopologyFromRedfishV2(raw_intf.get(), "redfish_test.textpb");
  const std::vector<Node> expected_nodes = {
      Node{.name = "root",
           .model = "root",
           .local_devpath = "/phys",
           .type = NodeType::kBoard,
           .replaceable = true},
      Node{.name = "cpu",
           .model = "cpu",
           .local_devpath = "/phys/CPU",
           .type = NodeType::kBoard,
           .supplemental_location_info =
               SupplementalLocationInfo{.service_label = "CPU"},
           .replaceable = true}};

  std::vector<Node> actual_nodes;
  actual_nodes.reserve(topology.nodes.size());
  for (const auto& node : topology.nodes) {
    actual_nodes.push_back(*node);
  }
  EXPECT_THAT(actual_nodes, Pointwise(RedfishNodeEqId(), expected_nodes));
}

TEST(TopologyTestRunner, TestingMultiHostMockupNodesArePopulated) {
  TestingMockupServer mockup("topology_v2_multi_host_testing/mockup.shar");
  auto raw_intf = mockup.RedfishClientInterface();
  CheckAgainstTestingMultiHostFullDevpaths(
      CreateTopologyFromRedfishV2(raw_intf.get(), "redfish_multihost.textpb"));
}

}  // namespace
}  // namespace ecclesia
