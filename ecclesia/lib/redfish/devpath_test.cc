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

#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/node_topology.h"
#include "ecclesia/lib/redfish/testing/json_mockup.h"

namespace libredfish {
namespace {

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

  auto devpath = GetSensorDevpathFromNodeTopology(json.get(), topology);
  ASSERT_TRUE(devpath.has_value());
  EXPECT_EQ(*devpath, test_devpath);
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

  auto devpath = GetSensorDevpathFromNodeTopology(json.get(), topology);
  ASSERT_TRUE(devpath.has_value());
  EXPECT_EQ(*devpath, test_devpath);
}

TEST(GetSensorDevpathFromNodeTopology, NoRelatedItemUsingChassisDevpath) {
  auto intf = NewJsonMockupInterface(R"json(
    {
      "@odata.id": "/redfish/v1/Chassis/chassis/Sensors/sensor"
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

  auto devpath = GetSensorDevpathFromNodeTopology(json.get(), topology);
  ASSERT_TRUE(devpath.has_value());
  EXPECT_EQ(*devpath, test_devpath);
}

TEST(GetSensorDevpathFromNodeTopology, SensorChassisPrefixInvalid) {
  auto intf = NewJsonMockupInterface(R"json(
    {
      "@odata.id": "/redfish/v1/Systems/system/Sensors/sensor"
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

  auto devpath = GetSensorDevpathFromNodeTopology(json.get(), topology);
  ASSERT_FALSE(devpath.has_value());
}

TEST(GetSensorDevpathFromNodeTopology, NoRelatedItemSensorChassisDevpaths) {
  auto intf = NewJsonMockupInterface(R"json(
    {
      "@odata.id": "/redfish/v1/Chassis/chassis/Sensors/sensor"
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

  auto devpath = GetSensorDevpathFromNodeTopology(json.get(), topology);
  ASSERT_FALSE(devpath.has_value());
}

}  // namespace
}  // namespace libredfish
