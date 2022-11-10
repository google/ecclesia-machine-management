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

#include "ecclesia/lib/redfish/devpath.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/node_topology.h"
#include "ecclesia/lib/redfish/testing/json_mockup.h"

namespace ecclesia {
namespace {

using ::testing::NotNull;
using ::testing::StrEq;

TEST(GetDevpathForUri, DevpathAvailable) {
  NodeTopology topology;
  absl::string_view uri = "/redfish/v1/Chassis/chassis";
  absl::string_view test_devpath = "/phys/test";
  {
    auto node = std::make_unique<Node>();
    node->local_devpath = test_devpath;
    topology.uri_to_associated_node_map[uri].push_back(node.get());
    topology.nodes.push_back(std::move(node));
  }

  auto devpath = GetDevpathForUri(topology, uri);
  ASSERT_TRUE(devpath.has_value());
  EXPECT_EQ(*devpath, test_devpath);
}

TEST(GetDevpathForUri, DevpathUnavailable) {
  NodeTopology topology;
  absl::string_view uri = "/redfish/v1/Chassis/chassis";
  absl::string_view test_devpath = "/phys/test";
  {
    auto node = std::make_unique<Node>();
    node->local_devpath = test_devpath;
    topology.uri_to_associated_node_map[uri].push_back(node.get());
    topology.nodes.push_back(std::move(node));
  }

  auto devpath = GetDevpathForUri(topology, "/redfish/v1/Chassis/not_here");
  EXPECT_FALSE(devpath.has_value());
}

TEST(GetSensorDevpathFromNodeTopology, RelatedItemDevpath) {
  auto intf = NewJsonMockupInterface(R"json(
    {
      "@odata.id": "/redfish/v1/Chassis/chassis/Sensors/sensor",
      "RelatedItem": [
        {"@odata.id": "/redfish/v1/System/system/Processors/0"}
      ]
    }
  )json");
  auto json = intf->GetRoot().AsObject();
  ASSERT_NE(json, nullptr);

  NodeTopology topology;
  absl::string_view uri = "/redfish/v1/System/system/Processors/0",
                    test_devpath = "/phys/test";
  {
    auto node = std::make_unique<Node>();
    node->local_devpath = test_devpath;
    topology.uri_to_associated_node_map[uri].push_back(node.get());
    topology.nodes.push_back(std::move(node));
  }

  auto devpath = GetSensorDevpathFromNodeTopology(json.get(), topology);
  ASSERT_TRUE(devpath.has_value());
  EXPECT_EQ(*devpath, test_devpath);
}

TEST(GetSensorDevpathFromNodeTopology, RelatedItemNoDevpath) {
  auto intf = NewJsonMockupInterface(R"json(
    {
      "@odata.id": "/redfish/v1/Chassis/chassis/Sensors/sensor",
      "RelatedItem": [
        {"@odata.id": "/redfish/v1/System/system/Processors/0"}
      ]
    }
  )json");
  auto json = intf->GetRoot().AsObject();
  ASSERT_NE(json, nullptr);

  NodeTopology topology;
  absl::string_view uri = "/redfish/v1/Chassis/chassis/Sensors/sensor",
                    test_devpath = "/phys/test";
  {
    auto node = std::make_unique<Node>();
    node->local_devpath = test_devpath;
    topology.uri_to_associated_node_map[uri].push_back(node.get());
    topology.nodes.push_back(std::move(node));
  }

  std::optional<std::string> sensor_devpath =
      GetSensorDevpathFromNodeTopology(json.get(), topology);
  ASSERT_TRUE(sensor_devpath.has_value());
  EXPECT_EQ(*sensor_devpath, test_devpath);

  std::optional<std::string> obj_devpath =
      GetDevpathForObjectAndNodeTopology(json.get(), topology);
  ASSERT_TRUE(obj_devpath.has_value());
  EXPECT_EQ(*obj_devpath, *sensor_devpath);
}

TEST(GetSensorDevpathFromNodeTopology, NoRelatedItemUsingSensor) {
  auto intf = NewJsonMockupInterface(R"json(
    {
      "@odata.id": "/redfish/v1/Chassis/chassis/Sensors/sensor"
    }
  )json");
  auto json = intf->GetRoot().AsObject();
  ASSERT_NE(json, nullptr);

  NodeTopology topology;
  absl::string_view uri = "/redfish/v1/Chassis/chassis/Sensors/sensor",
                    test_devpath = "/phys/test";
  {
    auto node = std::make_unique<Node>();
    node->local_devpath = test_devpath;
    topology.uri_to_associated_node_map[uri].push_back(node.get());
    topology.nodes.push_back(std::move(node));
  }

  auto sensor_devpath = GetSensorDevpathFromNodeTopology(json.get(), topology);
  ASSERT_TRUE(sensor_devpath.has_value());
  EXPECT_EQ(*sensor_devpath, test_devpath);

  // Confirm that the generic devpath function also matches.
  auto obj_devpath = GetDevpathForObjectAndNodeTopology(json.get(), topology);
  ASSERT_TRUE(obj_devpath.has_value());
  EXPECT_EQ(*obj_devpath, *sensor_devpath);
}

TEST(GetSensorDevpathFromNodeTopology, NoRelatedItemUsingChassisDevpath) {
  auto intf = NewJsonMockupInterface(R"json(
    {
      "@odata.id": "/redfish/v1/Chassis/chassis/Sensors/sensor",
      "@odata.type": "#Sensor.v1_0.Sensor"
    }
  )json");
  auto json = intf->GetRoot().AsObject();
  ASSERT_NE(json, nullptr);

  NodeTopology topology;
  absl::string_view uri = "/redfish/v1/Chassis/chassis",
                    test_devpath = "/phys/test";
  {
    auto node = std::make_unique<Node>();
    node->local_devpath = test_devpath;
    topology.uri_to_associated_node_map[uri].push_back(node.get());
    topology.nodes.push_back(std::move(node));
  }

  std::optional<std::string> sensor_devpath =
      GetSensorDevpathFromNodeTopology(json.get(), topology);
  ASSERT_TRUE(sensor_devpath.has_value());
  EXPECT_EQ(*sensor_devpath, test_devpath);

  // Confirm that the generic devpath function also matches.
  std::optional<std::string> obj_devpath =
      GetDevpathForObjectAndNodeTopology(json.get(), topology);
  ASSERT_TRUE(obj_devpath.has_value());
  EXPECT_EQ(*obj_devpath, *sensor_devpath);
}

TEST(GetSensorDevpathFromNodeTopology, SensorChassisPrefixInvalid) {
  auto intf = NewJsonMockupInterface(R"json(
    {
      "@odata.id": "/redfish/v1/Systems/system/Sensors/sensor",
      "@odata.type": "#Sensor.v1_0.Sensor"
    }
  )json");
  auto json = intf->GetRoot().AsObject();
  ASSERT_NE(json, nullptr);

  NodeTopology topology;
  absl::string_view uri = "/redfish/v1/Systems/system",
                    test_devpath = "/phys/test";
  {
    auto node = std::make_unique<Node>();
    node->local_devpath = test_devpath;
    topology.uri_to_associated_node_map[uri].push_back(node.get());
    topology.nodes.push_back(std::move(node));
  }

  std::optional<std::string> sensor_devpath =
      GetSensorDevpathFromNodeTopology(json.get(), topology);
  EXPECT_FALSE(sensor_devpath.has_value());

  // Confirm that the generic devpath function fails similarly.
  auto obj_devpath = GetDevpathForObjectAndNodeTopology(json.get(), topology);
  EXPECT_FALSE(obj_devpath.has_value());
}

TEST(GetSensorDevpathFromNodeTopology, NoRelatedItemSensorChassisDevpaths) {
  auto intf = NewJsonMockupInterface(R"json(
    {
      "@odata.id": "/redfish/v1/Chassis/chassis/Sensors/sensor",
      "@odata.type": "#Sensor.v1_0.Sensor"
    }
  )json");
  auto json = intf->GetRoot().AsObject();
  ASSERT_NE(json, nullptr);

  NodeTopology topology;
  absl::string_view uri = "/redfish/v1/Chassis/none_of_them",
                    test_devpath = "/phys/test";
  {
    auto node = std::make_unique<Node>();
    node->local_devpath = test_devpath;
    topology.uri_to_associated_node_map[uri].push_back(node.get());
    topology.nodes.push_back(std::move(node));
  }

  std::optional<std::string> sensor_devpath =
      GetSensorDevpathFromNodeTopology(json.get(), topology);
  EXPECT_FALSE(sensor_devpath.has_value());

  // Confirm that the generic devpath function fails similarly.
  std::optional<std::string> obj_devpath =
      GetDevpathForObjectAndNodeTopology(json.get(), topology);
  EXPECT_FALSE(obj_devpath.has_value());
}

TEST(GetManagerDevpathFromNodeTopology, DevpathByManagerInChassisLink) {
  auto intf = NewJsonMockupInterface(R"json(
    {
      "@odata.id": "/redfish/v1/Managers/bmc",
      "@odata.type": "#Manager.v1_14_0.Manager",
      "Links": {
        "ManagerInChassis": {
          "@odata.id": "/redfish/v1/Chassis/child0"
        }
      },
      "Name": "OpenBmc Manager"
    }
  )json");

  auto json = intf->GetRoot().AsObject();
  ASSERT_NE(json, nullptr);

  NodeTopology topology;
  absl::string_view uri = "/redfish/v1/Chassis/child0",
                    test_devpath = "/phys/test";
  {
    auto node = std::make_unique<Node>();
    node->local_devpath = test_devpath;
    topology.uri_to_associated_node_map[uri].push_back(node.get());
    topology.nodes.push_back(std::move(node));
  }

  std::optional<std::string> manager_chassis_devpath =
      GetManagerDevpathFromNodeTopology(json.get(), topology);
  ASSERT_TRUE(manager_chassis_devpath.has_value());

  // The devpath comes is derived from the chassis being managed, plus
  // ":device:" concatenated by "OpenBmc Manager" converted to "openbmc_manager"
  EXPECT_EQ(*manager_chassis_devpath, "/phys/test:device:openbmc_manager");

  // Confirm that the generic devpath function also matches.
  std::optional<std::string> obj_devpath =
      GetDevpathForObjectAndNodeTopology(json.get(), topology);
  ASSERT_TRUE(obj_devpath.has_value());
  EXPECT_THAT(*obj_devpath, StrEq(*manager_chassis_devpath));
}

TEST(GetManagerDevpathFromNodeTopology, DevpathByFallbackPath) {
  auto intf = NewJsonMockupInterface(R"json(
    {
      "@odata.id": "/redfish/v1/Managers/bmc",
      "@odata.type": "#Manager.v1_14_0.Manager",
      "Name": "OpenBmc Manager"
    }
  )json");

  auto json = intf->GetRoot().AsObject();
  ASSERT_NE(json, nullptr);
  NodeTopology topology;
  absl::string_view uri = "/redfish/v1/Managers/bmc",
                    test_devpath = "/phys/test_bmc";
  {
    auto node = std::make_unique<Node>();
    node->local_devpath = test_devpath;
    topology.uri_to_associated_node_map[uri].push_back(node.get());
    topology.nodes.push_back(std::move(node));
  }
  std::optional<std::string> manager_chassis_devpath =
      GetManagerDevpathFromNodeTopology(json.get(), topology);
  ASSERT_TRUE(manager_chassis_devpath.has_value());

  // Confirm that the generic devpath function also matches.
  std::optional<std::string> obj_devpath =
      GetDevpathForObjectAndNodeTopology(json.get(), topology);
  ASSERT_TRUE(obj_devpath.has_value());
  EXPECT_THAT(*manager_chassis_devpath, StrEq(*obj_devpath));

  // The fallback will attempt to find a devpath directly associated with the
  // BMC's URI. If found, the devpath will be used.
  EXPECT_THAT(*manager_chassis_devpath, StrEq("/phys/test_bmc"));
}

TEST(GetDevpathForObjectAndNodeTopology, EmptyIfTypeMissing) {
  auto intf = NewJsonMockupInterface(R"json(
    {
      "@odata.id": "/redfish/v1/Systems/system/Sensors/sensor"
    }
  )json");
  std::unique_ptr<RedfishObject> json = intf->GetRoot().AsObject();
  ASSERT_THAT(json, NotNull());

  NodeTopology topology;
  {
    auto node = std::make_unique<Node>();
    node->local_devpath = "/phys/test";
    topology.uri_to_associated_node_map["/redfish/v1/Systems/system"].push_back(
        node.get());
    topology.nodes.push_back(std::move(node));
  }

  std::optional<std::string> obj_devpath =
      GetDevpathForObjectAndNodeTopology(json.get(), topology);
  EXPECT_FALSE(obj_devpath.has_value());
}

}  // namespace
}  // namespace ecclesia
