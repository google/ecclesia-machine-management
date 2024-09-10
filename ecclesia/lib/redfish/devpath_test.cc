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
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/location.h"
#include "ecclesia/lib/redfish/node_topology.h"
#include "ecclesia/lib/redfish/testing/json_mockup.h"
#include "ecclesia/lib/testing/status.h"

namespace ecclesia {
namespace {

using ::testing::Eq;
using ::testing::NotNull;
using ::testing::Optional;
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

  std::optional<std::string> devpath = GetDevpathForUri(topology, uri);
  EXPECT_THAT(devpath, Optional(Eq(test_devpath)));
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

  std::optional<std::string> devpath =
      GetDevpathForUri(topology, "/redfish/v1/Chassis/not_here");
  EXPECT_FALSE(devpath.has_value());
}

TEST(GetDevpathForUri, GetUriFromDevpathSuccess) {
  NodeTopology topology;
  absl::string_view uri = "/redfish/v1/Chassis/chassis";
  absl::string_view uri_extra = "/redfish/v1/Test";
  absl::string_view test_devpath = "/phys/test";

  auto node = std::make_unique<Node>();
  node->local_devpath = std::string(test_devpath);
  node->associated_uris.push_back(std::string(uri));
  node->associated_uris.push_back(std::string(uri_extra));
  topology.devpath_to_node_map[test_devpath] = node.get();

  EXPECT_THAT(GetFirstUriForDevpath(topology, test_devpath), IsOkAndHolds(uri));
}

TEST(GetDevpathForUri, GetUriFromDevpathMissingDevpath) {
  NodeTopology topology;
  absl::string_view uri = "/redfish/v1/Chassis/chassis";
  absl::string_view test_devpath = "/phys/test";

  auto node = std::make_unique<Node>();
  node->local_devpath = std::string(test_devpath);
  node->associated_uris.push_back(std::string(uri));
  topology.devpath_to_node_map[test_devpath] = node.get();

  EXPECT_THAT(GetFirstUriForDevpath(topology, "/phys/no_exist"),
              IsStatusNotFound());
}

TEST(GetDevpathForUri, GetUriFromDevpathNoNode) {
  NodeTopology topology;
  absl::string_view test_devpath = "/phys/test";

  topology.devpath_to_node_map[test_devpath] = nullptr;
  EXPECT_THAT(GetFirstUriForDevpath(topology, test_devpath),
              IsStatusNotFound());
}

TEST(GetDevpathForUri, GetUriFromDevpathNoAssociatedUri) {
  NodeTopology topology;
  absl::string_view test_devpath = "/phys/test";

  auto node = std::make_unique<Node>();
  node->local_devpath = std::string(test_devpath);
  topology.devpath_to_node_map[test_devpath] = node.get();
  topology.nodes.push_back(std::move(node));

  EXPECT_THAT(GetFirstUriForDevpath(topology, test_devpath),
              IsStatusNotFound());
}

TEST(GetDevpathForUri, GetDevpathUsingAlternateResolverSuccess) {
  NodeTopology topology;
  absl::string_view uri = "/redfish/v1/Chassis/chassis";
  absl::string_view test_devpath = "/phys/test";
  auto node = std::make_unique<Node>();
  node->local_devpath = test_devpath;
  node->supplemental_location_info = SupplementalLocationInfo{
      .service_label = "ABC", .part_location_context = "DEF"};
  topology.uri_to_associated_node_map[uri].push_back(node.get());
  topology.nodes.push_back(std::move(node));

  absl::string_view child_uri = "/redfish/v1/Chassis/child";
  absl::string_view child_test_devpath = "/phys/test/child";
  auto child_node = std::make_unique<Node>();
  child_node->local_devpath = child_test_devpath;
  child_node->supplemental_location_info = SupplementalLocationInfo{
      .service_label = "GHI", .part_location_context = "JKL"};
  topology.uri_to_associated_node_map[child_uri].push_back(child_node.get());
  topology.nodes.push_back(std::move(child_node));

  auto alternate_resolver =
      [&](const NodeTopology &topo,
          absl::string_view id) -> std::optional<std::string> {
    auto it = topo.uri_to_associated_node_map.find(id);
    if (it != topo.uri_to_associated_node_map.end() && !it->second.empty()) {
      // Assume that first Node will represent the local devpath for the URI
      SupplementalLocationInfo supplemental_location_info =
          it->second.front()->supplemental_location_info.value_or(
              SupplementalLocationInfo{.service_label = "unknown_sl",
                                       .part_location_context = "unknown_plc"});
      if (supplemental_location_info ==
          SupplementalLocationInfo{.service_label = "GHI",
                                   .part_location_context = "JKL"}) {
        return "alternate/child/path";
      }
    }
    return std::nullopt;
  };

  // Base case will fall back to standard resolver
  auto devpath = GetDevpathForUri(topology, uri, alternate_resolver);
  EXPECT_THAT(devpath, Optional(Eq(test_devpath)));

  devpath = GetDevpathForUri(topology, child_uri, alternate_resolver);
  EXPECT_THAT(devpath, Optional(Eq("alternate/child/path")));
}

TEST(GetSensorDevpathFromNodeTopology, RelatedItemDevpath) {
  std::unique_ptr<RedfishInterface> intf = NewJsonMockupInterface(R"json(
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
  absl::string_view uri = "/redfish/v1/System/system/Processors/0";
  absl::string_view test_devpath = "/phys/test";
  {
    auto node = std::make_unique<Node>();
    node->local_devpath = test_devpath;
    node->supplemental_location_info = SupplementalLocationInfo{
        .service_label = "ABC", .part_location_context = "DEF"};
    topology.uri_to_associated_node_map[uri].push_back(node.get());
    topology.nodes.push_back(std::move(node));
  }

  auto sensor_devpath = GetSensorDevpathFromNodeTopology(*json, topology);
  EXPECT_THAT(sensor_devpath, Optional(Eq(test_devpath)));
}

TEST(GetSensorDevpathFromNodeTopology, RelatedItemDevpathAlternateResolver) {
  std::unique_ptr<RedfishInterface> intf = NewJsonMockupInterface(R"json(
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
  absl::string_view uri = "/redfish/v1/System/system/Processors/0";
  absl::string_view test_devpath = "/phys/test";
  {
    auto node = std::make_unique<Node>();
    node->local_devpath = test_devpath;
    node->supplemental_location_info = SupplementalLocationInfo{
        .service_label = "ABC", .part_location_context = "DEF"};
    topology.uri_to_associated_node_map[uri].push_back(node.get());
    topology.nodes.push_back(std::move(node));
  }

  auto alternate_resolver =
      [&](const NodeTopology &topo,
          absl::string_view id) -> std::optional<std::string> {
    auto it = topo.uri_to_associated_node_map.find(id);
    if (it != topo.uri_to_associated_node_map.end() && !it->second.empty()) {
      // Assume that first Node will represent the local devpath for the URI
      SupplementalLocationInfo supplemental_location_info =
          it->second.front()->supplemental_location_info.value_or(
              SupplementalLocationInfo{.service_label = "unknown_sl",
                                       .part_location_context = "unknown_plc"});
      if (supplemental_location_info ==
          SupplementalLocationInfo{.service_label = "ABC",
                                   .part_location_context = "DEF"}) {
        return "alternate/sensor/path";
      }
    }
    return std::nullopt;
  };

  std::optional<std::string> sensor_devpath =
      GetSensorDevpathFromNodeTopology(*json, topology, alternate_resolver);
  EXPECT_THAT(sensor_devpath, Optional(Eq("alternate/sensor/path")));
}

TEST(GetSensorDevpathFromNodeTopology, RelatedItemNoDevpath) {
  std::unique_ptr<RedfishInterface> intf = NewJsonMockupInterface(R"json(
    {
      "@odata.id": "/redfish/v1/Chassis/chassis/Sensors/sensor",
      "RelatedItem": [
        {
          "@odata.id": "/redfish/v1/System/system/Processors/0"
        }
      ]
    }
  )json");
  std::unique_ptr<RedfishObject> json = intf->GetRoot().AsObject();
  ASSERT_NE(json, nullptr);

  NodeTopology topology;
  absl::string_view uri = "/redfish/v1/Chassis/chassis/Sensors/sensor";
  absl::string_view test_devpath = "/phys/test";
  {
    auto node = std::make_unique<Node>();
    node->local_devpath = test_devpath;
    topology.uri_to_associated_node_map[uri].push_back(node.get());
    topology.nodes.push_back(std::move(node));
  }

  std::optional<std::string> sensor_devpath =
      GetSensorDevpathFromNodeTopology(*json, topology);
  EXPECT_THAT(sensor_devpath, Optional(Eq(test_devpath)));

  std::optional<std::string> obj_devpath =
      GetDevpathForObjectAndNodeTopology(*json, topology);
  EXPECT_THAT(obj_devpath, Optional(Eq(test_devpath)));
}

TEST(GetSensorDevpathFromNodeTopology, NoRelatedItemUsingSensor) {
  std::unique_ptr<RedfishInterface> intf = NewJsonMockupInterface(R"json(
    {
      "@odata.id": "/redfish/v1/Chassis/chassis/Sensors/sensor"
    }
  )json");
  std::unique_ptr<RedfishObject> json = intf->GetRoot().AsObject();
  ASSERT_NE(json, nullptr);

  NodeTopology topology;
  absl::string_view uri = "/redfish/v1/Chassis/chassis/Sensors/sensor";
  absl::string_view test_devpath = "/phys/test";
  {
    auto node = std::make_unique<Node>();
    node->local_devpath = test_devpath;
    node->supplemental_location_info = SupplementalLocationInfo{
        .service_label = "ABC", .part_location_context = "DEF"};
    topology.uri_to_associated_node_map[uri].push_back(node.get());
    topology.nodes.push_back(std::move(node));
  }

  std::optional<std::string> sensor_devpath =
      GetSensorDevpathFromNodeTopology(*json, topology);
  ASSERT_TRUE(sensor_devpath.has_value());
  EXPECT_EQ(*sensor_devpath, test_devpath);

  // Confirm that the generic devpath function also matches.
  std::optional<std::string> obj_devpath =
      GetDevpathForObjectAndNodeTopology(*json, topology);
  ASSERT_TRUE(obj_devpath.has_value());
  EXPECT_EQ(*obj_devpath, *sensor_devpath);

  auto alternate_resolver =
      [&](const NodeTopology &topo,
          absl::string_view id) -> std::optional<std::string> {
    auto it = topo.uri_to_associated_node_map.find(id);
    if (it != topo.uri_to_associated_node_map.end() && !it->second.empty()) {
      // Assume that first Node will represent the local devpath for the URI
      SupplementalLocationInfo supplemental_location_info =
          it->second.front()->supplemental_location_info.value_or(
              SupplementalLocationInfo{.service_label = "unknown_sl",
                                       .part_location_context = "unknown_plc"});
      if (supplemental_location_info ==
          SupplementalLocationInfo{.service_label = "ABC",
                                   .part_location_context = "DEF"}) {
        return "alternate/chassis/path";
      }
    }
    return std::nullopt;
  };

  obj_devpath =
      GetDevpathForObjectAndNodeTopology(*json, topology, alternate_resolver);
  ASSERT_TRUE(obj_devpath.has_value());
  EXPECT_THAT(obj_devpath, Optional(Eq("alternate/chassis/path")));
}

TEST(GetSensorDevpathFromNodeTopology,
     NoRelatedItemUsingSensorAlternateResolver) {
  std::unique_ptr<RedfishInterface> intf = NewJsonMockupInterface(R"json(
    {
      "@odata.id": "/redfish/v1/Chassis/chassis/Sensors/sensor"
    }
  )json");
  std::unique_ptr<RedfishObject> json = intf->GetRoot().AsObject();
  ASSERT_NE(json, nullptr);

  NodeTopology topology;
  absl::string_view uri = "/redfish/v1/Chassis/chassis/Sensors/sensor";
  absl::string_view test_devpath = "/phys/test";
  {
    auto node = std::make_unique<Node>();
    node->local_devpath = test_devpath;
    node->supplemental_location_info = SupplementalLocationInfo{
        .service_label = "ABC", .part_location_context = "DEF"};
    topology.uri_to_associated_node_map[uri].push_back(node.get());
    topology.nodes.push_back(std::move(node));
  }

  auto alternate_resolver =
      [&](const NodeTopology &topo,
          absl::string_view id) -> std::optional<std::string> {
    auto it = topo.uri_to_associated_node_map.find(id);
    if (it != topo.uri_to_associated_node_map.end() && !it->second.empty()) {
      // Assume that first Node will represent the local devpath for the URI
      SupplementalLocationInfo supplemental_location_info =
          it->second.front()->supplemental_location_info.value_or(
              SupplementalLocationInfo{.service_label = "unknown_sl",
                                       .part_location_context = "unknown_plc"});
      if (supplemental_location_info ==
          SupplementalLocationInfo{.service_label = "ABC",
                                   .part_location_context = "DEF"}) {
        return "alternate/chassis/path";
      }
    }
    return std::nullopt;
  };

  std::optional<std::string> obj_devpath =
      GetDevpathForObjectAndNodeTopology(*json, topology, alternate_resolver);
  EXPECT_THAT(obj_devpath, Optional(Eq("alternate/chassis/path")));
}

TEST(GetSensorDevpathFromNodeTopology, NoRelatedItemUsingChassisDevpath) {
  std::unique_ptr<RedfishInterface> intf = NewJsonMockupInterface(R"json(
    {
      "@odata.id": "/redfish/v1/Chassis/chassis/Sensors/sensor",
      "@odata.type": "#Sensor.v1_0.Sensor"
    }
  )json");
  std::unique_ptr<RedfishObject> json = intf->GetRoot().AsObject();
  ASSERT_NE(json, nullptr);

  NodeTopology topology;
  absl::string_view uri = "/redfish/v1/Chassis/chassis";
  absl::string_view test_devpath = "/phys/test";
  {
    auto node = std::make_unique<Node>();
    node->local_devpath = test_devpath;
    topology.uri_to_associated_node_map[uri].push_back(node.get());
    topology.nodes.push_back(std::move(node));
  }

  std::optional<std::string> sensor_devpath =
      GetSensorDevpathFromNodeTopology(*json, topology);
  EXPECT_THAT(sensor_devpath, Optional(Eq(test_devpath)));

  // Confirm that the generic devpath function also matches.
  std::optional<std::string> obj_devpath =
      GetDevpathForObjectAndNodeTopology(*json, topology);
  EXPECT_THAT(obj_devpath, Optional(Eq(sensor_devpath)));
}

TEST(GetSensorDevpathFromNodeTopology, SensorChassisPrefixInvalid) {
  std::unique_ptr<RedfishInterface> intf = NewJsonMockupInterface(R"json(
    {
      "@odata.id": "/redfish/v1/Systems/system/Sensors/sensor",
      "@odata.type": "#Sensor.v1_0.Sensor"
    }
  )json");
  std::unique_ptr<RedfishObject> json = intf->GetRoot().AsObject();
  ASSERT_NE(json, nullptr);

  NodeTopology topology;
  absl::string_view uri = "/redfish/v1/Systems/system";
  absl::string_view test_devpath = "/phys/test";
  {
    auto node = std::make_unique<Node>();
    node->local_devpath = test_devpath;
    topology.uri_to_associated_node_map[uri].push_back(node.get());
    topology.nodes.push_back(std::move(node));
  }

  std::optional<std::string> sensor_devpath =
      GetSensorDevpathFromNodeTopology(*json, topology);
  EXPECT_FALSE(sensor_devpath.has_value());

  // Confirm that the generic devpath function fails similarly.
  auto obj_devpath = GetDevpathForObjectAndNodeTopology(*json, topology);
  EXPECT_FALSE(obj_devpath.has_value());
}

TEST(GetSensorDevpathFromNodeTopology, NoRelatedItemSensorChassisDevpaths) {
  std::unique_ptr<RedfishInterface> intf = NewJsonMockupInterface(R"json(
    {
      "@odata.id": "/redfish/v1/Chassis/chassis/Sensors/sensor",
      "@odata.type": "#Sensor.v1_0.Sensor"
    }
  )json");
  auto json = intf->GetRoot().AsObject();
  ASSERT_NE(json, nullptr);

  NodeTopology topology;
  absl::string_view uri = "/redfish/v1/Chassis/none_of_them";
  absl::string_view test_devpath = "/phys/test";
  {
    auto node = std::make_unique<Node>();
    node->local_devpath = test_devpath;
    topology.uri_to_associated_node_map[uri].push_back(node.get());
    topology.nodes.push_back(std::move(node));
  }

  std::optional<std::string> sensor_devpath =
      GetSensorDevpathFromNodeTopology(*json, topology);
  EXPECT_FALSE(sensor_devpath.has_value());

  // Confirm that the generic devpath function fails similarly.
  std::optional<std::string> obj_devpath =
      GetDevpathForObjectAndNodeTopology(*json, topology);
  EXPECT_FALSE(obj_devpath.has_value());
}

TEST(GetManagerDevpathFromNodeTopology, DevpathByManagerInChassisLink) {
  std::unique_ptr<RedfishInterface> intf = NewJsonMockupInterface(R"json(
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
  absl::string_view uri = "/redfish/v1/Chassis/child0";
  absl::string_view test_devpath = "/phys/test";
  {
    auto node = std::make_unique<Node>();
    node->local_devpath = test_devpath;
    node->supplemental_location_info = SupplementalLocationInfo{
        .service_label = "ABC", .part_location_context = "DEF"};
    topology.uri_to_associated_node_map[uri].push_back(node.get());
    topology.nodes.push_back(std::move(node));
  }

  std::optional<std::string> manager_chassis_devpath =
      GetManagerDevpathFromNodeTopology(*json, topology);
  ASSERT_TRUE(manager_chassis_devpath.has_value());

  // Confirm that the generic devpath function also matches.
  std::optional<std::string> obj_devpath =
      GetDevpathForObjectAndNodeTopology(*json, topology);
  ASSERT_TRUE(obj_devpath.has_value());
  EXPECT_EQ(*obj_devpath, *manager_chassis_devpath);

  // The devpath comes is derived from the chassis being managed, plus
  // ":device:" concatenated by "OpenBmc Manager" converted to "openbmc_manager"
  EXPECT_EQ(*manager_chassis_devpath, "/phys/test:device:openbmc_manager");

  auto alternate_resolver =
      [&](const NodeTopology &topo,
          absl::string_view id) -> std::optional<std::string> {
    auto it = topo.uri_to_associated_node_map.find(id);
    if (it != topo.uri_to_associated_node_map.end() && !it->second.empty()) {
      // Assume that first Node will represent the local devpath for the URI
      SupplementalLocationInfo supplemental_location_info =
          it->second.front()->supplemental_location_info.value_or(
              SupplementalLocationInfo{.service_label = "unknown_sl",
                                       .part_location_context = "unknown_plc"});
      if (supplemental_location_info ==
          SupplementalLocationInfo{.service_label = "ABC",
                                   .part_location_context = "DEF"}) {
        return "alternate/chassis/path";
      }
    }
    return std::nullopt;
  };

  manager_chassis_devpath =
      GetManagerDevpathFromNodeTopology(*json, topology, alternate_resolver);
  ASSERT_THAT(manager_chassis_devpath,
              Optional(Eq("alternate/chassis/path:device:openbmc_manager")));
}

TEST(GetManagerDevpathFromNodeTopology, DevpathByFallbackPath) {
  std::unique_ptr<RedfishInterface> intf = NewJsonMockupInterface(R"json(
    {
      "@odata.id": "/redfish/v1/Managers/bmc",
      "@odata.type": "#Manager.v1_14_0.Manager",
      "Name": "OpenBmc Manager"
    }
  )json");

  auto json = intf->GetRoot().AsObject();
  ASSERT_NE(json, nullptr);
  NodeTopology topology;
  absl::string_view uri = "/redfish/v1/Managers/bmc";
  absl::string_view test_devpath = "/phys/test_bmc";
  {
    auto node = std::make_unique<Node>();
    node->local_devpath = test_devpath;
    topology.uri_to_associated_node_map[uri].push_back(node.get());
    topology.nodes.push_back(std::move(node));
  }
  std::optional<std::string> manager_chassis_devpath =
      GetManagerDevpathFromNodeTopology(*json, topology);
  ASSERT_TRUE(manager_chassis_devpath.has_value());

  // Confirm that the generic devpath function also matches.
  std::optional<std::string> obj_devpath =
      GetDevpathForObjectAndNodeTopology(*json, topology);
  ASSERT_TRUE(obj_devpath.has_value());
  EXPECT_THAT(*manager_chassis_devpath, StrEq(*obj_devpath));

  // The fallback will attempt to find a devpath directly associated with the
  // BMC's URI. If found, the devpath will be used.
  EXPECT_THAT(*manager_chassis_devpath, StrEq("/phys/test_bmc"));
}

TEST(GetDevpathForObjectAndNodeTopology, EmptyIfTypeMissing) {
  std::unique_ptr<RedfishInterface> intf = NewJsonMockupInterface(R"json(
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
      GetDevpathForObjectAndNodeTopology(*json, topology);
  EXPECT_FALSE(obj_devpath.has_value());
}

TEST(GetSlotDevpathFromNodeTopology, DevpathFromNodeLocation) {
  std::unique_ptr<RedfishInterface> intf = NewJsonMockupInterface(R"json(
    {
      "@odata.context": "/redfish/v1/$metadata#Drive.Drive",
      "@odata.id": "/redfish/v1/Chassis/test/Drives/SataDrive_0",
      "Location": {
        "PartLocation": {
            "LocationType": "Slot",
            "ServiceLabel": "SATA"
        }
      }
    }
  )json");
  auto json = intf->GetRoot().AsObject();
  ASSERT_NE(json, nullptr);

  NodeTopology topology;
  absl::string_view uri = "test/redfish/v1/Chassis/test";
  absl::string_view test_devpath = "/phys";
  {
    auto node = std::make_unique<Node>();
    node->local_devpath = test_devpath;
    topology.uri_to_associated_node_map[uri].push_back(node.get());
    topology.nodes.push_back(std::move(node));
  }

  absl::string_view parent_uri =
      "test/redfish/v1/Chassis/test/Drives/SataDrive_0";
  absl::string_view expect_devpath = "/phys:connector:SATA";
  std::optional<std::string> slot_devpath =
      GetSlotDevpathFromNodeTopology(*json, parent_uri, topology);
  ASSERT_TRUE(slot_devpath.has_value());
  EXPECT_EQ(*slot_devpath, expect_devpath);
}

TEST(GetSlotDevpathFromNodeTopology, DevpathFromNodePhysicalLocation) {
  std::unique_ptr<RedfishInterface> intf = NewJsonMockupInterface(R"json(
    {
      "@odata.context": "/redfish/v1/$metadata#Drive.Drive",
      "@odata.id": "/redfish/v1/Chassis/test/Drives/SataDrive_0",
      "PhysicalLocation": {
        "PartLocation": {
            "LocationType": "Slot",
            "ServiceLabel": "SATA"
        }
      }
    }
  )json");
  auto json = intf->GetRoot().AsObject();
  ASSERT_NE(json, nullptr);

  NodeTopology topology;
  absl::string_view uri = "test/redfish/v1/Chassis/test";
  absl::string_view test_devpath = "/phys";
  {
    auto node = std::make_unique<Node>();
    node->local_devpath = test_devpath;
    topology.uri_to_associated_node_map[uri].push_back(node.get());
    topology.nodes.push_back(std::move(node));
  }

  absl::string_view parent_uri =
      "test/redfish/v1/Chassis/test/Drives/SataDrive_0";
  absl::string_view expect_devpath = "/phys:connector:SATA";
  std::optional<std::string> slot_devpath =
      GetSlotDevpathFromNodeTopology(*json, parent_uri, topology);
  ASSERT_TRUE(slot_devpath.has_value());
  EXPECT_EQ(*slot_devpath, expect_devpath);
}

TEST(GetSlotDevpathFromNodeTopology, NumberOfParentUriSizeCanEqualToFive) {
  std::unique_ptr<RedfishInterface> intf = NewJsonMockupInterface(R"json(
    {
      "@odata.context": "/redfish/v1/$metadata#Drive.Drive",
      "@odata.id": "/redfish/v1/Chassis/Tray0_SATA0/Drives/SataDrive_0",
      "Location": {
        "PartLocation": {
            "LocationType": "Slot",
            "ServiceLabel": "SATA"
        }
      }
    }
  )json");
  auto json = intf->GetRoot().AsObject();
  ASSERT_THAT(json, NotNull());

  NodeTopology topology;
  absl::string_view uri = "test/redfish/v1/Chassis/Tray0_SATA0";
  absl::string_view test_devpath = "/phys/PCIE0";
  {
    auto node = std::make_unique<Node>();
    node->local_devpath = test_devpath;
    topology.uri_to_associated_node_map[uri].push_back(node.get());
    topology.nodes.push_back(std::move(node));
  }

  absl::string_view parent_uri = "test/redfish/v1/Chassis/Tray0_SATA0";
  absl::string_view expect_devpath = "/phys/PCIE0:connector:SATA";
  std::optional<std::string> slot_devpath =
      GetSlotDevpathFromNodeTopology(*json, parent_uri, topology);
  ASSERT_TRUE(slot_devpath.has_value());
  EXPECT_EQ(*slot_devpath, expect_devpath);
}

TEST(GetDevpathFromSlotDevpath, WrongSlotDevpath) {
  absl::string_view wrong_slot_devpath = "/phys/PCIE0/sas@0";
  auto result = GetDevpathFromSlotDevpath(wrong_slot_devpath);
  ASSERT_FALSE(result.has_value());
}

TEST(GetDevpathFromSlotDevpath, DevpathSuccess) {
  absl::string_view slot_devpath = "/phys/PCIE0:connector:sas@0";
  absl::string_view expect_devpath = "/phys/PCIE0/sas@0";
  auto result = GetDevpathFromSlotDevpath(slot_devpath);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, expect_devpath);
}

TEST(GetDevpathForUri, ReplaceableDevpathSuccess) {
  NodeTopology topology;
  absl::string_view uri = "/redfish/v1/Chassis/chassis";
  absl::string_view child_uri = "/redfish/v1/Chassis/child";
  absl::string_view test_devpath = "/phys/test";
  absl::string_view child_devpath = "/phys/test/child";

  auto node = std::make_unique<Node>();
  node->local_devpath = test_devpath;
  node->replaceable = true;
  topology.uri_to_associated_node_map[uri].push_back(node.get());
  topology.devpath_to_node_map[test_devpath] = node.get();

  auto child_node = std::make_unique<Node>();
  child_node->local_devpath = child_devpath;
  child_node->replaceable = false;

  topology.uri_to_associated_node_map[child_uri].push_back(child_node.get());
  topology.devpath_to_node_map[child_devpath] = child_node.get();

  topology.nodes.push_back(std::move(node));
  topology.nodes.push_back(std::move(child_node));
  // Ensure child node points to test node as parent.
  std::vector<Node *> parents;
  topology.node_to_parents[topology.nodes.back().get()] =
      std::vector<Node *>{topology.nodes.front().get()};

  absl::StatusOr<std::string> replaceable_devpath =
      GetReplaceableDevpathForDevpathAndNodeTopology(child_devpath, topology);
  EXPECT_THAT(replaceable_devpath, IsOkAndHolds(test_devpath));
}

TEST(GetDevpathForUri, NoReplaceableDevpath) {
  NodeTopology topology;
  std::string uri = "/redfish/v1/Chassis/chassis";
  std::string child_uri = "/redfish/v1/Chassis/child";
  std::string test_devpath = "/phys/test";
  std::string child_devpath = "/phys/test/child";
  {
    auto node = std::make_unique<Node>();
    node->local_devpath = test_devpath;
    node->replaceable = false;
    topology.uri_to_associated_node_map[uri].push_back(node.get());
    topology.devpath_to_node_map[test_devpath] = node.get();
    topology.nodes.push_back(std::move(node));
  }
  {
    auto node = std::make_unique<Node>();
    node->local_devpath = child_devpath;
    node->replaceable = false;
    topology.uri_to_associated_node_map[child_uri].push_back(node.get());
    topology.devpath_to_node_map[child_devpath] = node.get();
    topology.nodes.push_back(std::move(node));
  }

  absl::StatusOr<std::string> replaceable_devpath =
      GetReplaceableDevpathForDevpathAndNodeTopology(child_devpath, topology);
  EXPECT_THAT(replaceable_devpath, IsStatusNotFound());
}

TEST(GetDevpathForUri, ReplaceableDevpathAlternateResolver) {
  NodeTopology topology;
  std::string uri = "/redfish/v1/Chassis/chassis";
  std::string child_uri = "/redfish/v1/Chassis/child";
  std::string test_devpath = "/phys/test";
  std::string child_devpath = "/phys/test/child";

  auto node = std::make_unique<Node>();
  node->local_devpath = test_devpath;
  node->replaceable = true;
  node->associated_uris.push_back(uri);
  topology.uri_to_associated_node_map[uri].push_back(node.get());
  topology.devpath_to_node_map[test_devpath] = node.get();

  auto child_node = std::make_unique<Node>();
  child_node->local_devpath = child_devpath;
  child_node->associated_uris.push_back(child_uri);
  child_node->supplemental_location_info = SupplementalLocationInfo{
      .service_label = "ABC", .part_location_context = "DEF"};
  child_node->replaceable = false;

  topology.uri_to_associated_node_map[child_uri].push_back(child_node.get());
  topology.devpath_to_node_map[child_devpath] = child_node.get();

  topology.nodes.push_back(std::move(node));
  topology.nodes.push_back(std::move(child_node));
  // Ensure child node points to test node as parent.
  std::vector<Node *> parents;
  topology.node_to_parents[topology.nodes.back().get()] =
      std::vector<Node *>{topology.nodes.front().get()};
  auto alternate_resolver =
      [&](const NodeTopology &topo,
          absl::string_view path) -> std::optional<std::string> {
    auto it = topo.uri_to_associated_node_map.find(path);
    if (it != topo.uri_to_associated_node_map.end() && !it->second.empty()) {
      // Assume that first Node will represent the local devpath for the URI
      SupplementalLocationInfo supplemental_location_info =
          it->second.front()->supplemental_location_info.value_or(
              SupplementalLocationInfo{.service_label = "unknown_sl",
                                       .part_location_context = "unknown_plc"});
      if (supplemental_location_info ==
          SupplementalLocationInfo{.service_label = "ABC",
                                   .part_location_context = "DEF"}) {
        return "alternate/replaceable/path";
      }
    }
    return std::nullopt;
  };

  absl::StatusOr<std::string> devpath =
      GetReplaceableDevpathForDevpathAndNodeTopology(child_devpath, topology,
                                                     alternate_resolver);
  EXPECT_THAT(devpath, IsOkAndHolds("alternate/replaceable/path"));
}

}  // namespace
}  // namespace ecclesia
