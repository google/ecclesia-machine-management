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

#include <optional>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/node_topology.h"
#include "ecclesia/lib/redfish/property_definitions.h"
#include "ecclesia/lib/redfish/types.h"
#include "ecclesia/lib/redfish/utils.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {

std::optional<std::string> GetDevpathForUri(const NodeTopology &topology,
                                            absl::string_view uri) {
  auto it = topology.uri_to_associated_node_map.find(uri);
  if (it != topology.uri_to_associated_node_map.end() && !it->second.empty()) {
    // Assume that first Node will represent the local devpath for the URI
    return it->second.front()->local_devpath;
  }
  return std::nullopt;
}

absl::StatusOr<std::string> GetFirstUriForDevpath(const NodeTopology &topology,
                                                  absl::string_view devpath) {
  auto it = topology.devpath_to_node_map.find(devpath);
  if (it == topology.devpath_to_node_map.end() || (it->second == nullptr) ||
      it->second->associated_uris.empty()) {
    return absl::NotFoundError(
        absl::StrCat("Unable to find a uri for devpath ", devpath));
  }
  return it->second->associated_uris[0];
}

std::optional<std::string> GetDevpathForObjectAndNodeTopology(
    const RedfishObject &obj, const NodeTopology &topology) {
  if (std::optional<ResourceTypeAndVersion> type_and_version =
          GetResourceTypeAndVersionForObject(obj);
      type_and_version.has_value()) {
    if (type_and_version->resource_type == ResourceManager::Name) {
      return GetManagerDevpathFromNodeTopology(obj, topology);
    }
    if (type_and_version->resource_type == ResourceSensor::Name) {
      return GetSensorDevpathFromNodeTopology(obj, topology);
    }
  }
  std::optional<std::string> maybe_uri = obj.GetUriString();
  if (!maybe_uri.has_value()) {
    return std::nullopt;
  }
  return GetDevpathForUri(topology, *maybe_uri);
}

std::optional<std::string> GetSensorDevpathFromNodeTopology(
    const RedfishObject &obj, const NodeTopology &topology) {
  nlohmann::json json_obj = obj.GetContentAsJson();
  if (json_obj.contains(kRfPropertyRelatedItem)) {
    nlohmann::json related_item_json = json_obj[kRfPropertyRelatedItem];
    if (related_item_json.is_array() && !related_item_json.empty() &&
        related_item_json[0].is_object() &&
        related_item_json[0].contains(PropertyOdataId::Name)) {
      std::string related_item_uri =
          related_item_json[0][PropertyOdataId::Name];
      if (auto related_devpath = GetDevpathForUri(topology, related_item_uri);
          related_devpath.has_value()) {
        return *related_devpath;
      }
    }
  }
  // Fallback to providing devpath for obj
  auto sensor_uri = obj.GetUriString();
  if (sensor_uri.has_value()) {
    auto sensor_devpath = GetDevpathForUri(topology, *sensor_uri);
    if (sensor_devpath.has_value()) {
      return *sensor_devpath;
    }

    // If sensor itself and Related Item don't have devpaths, check the prefix
    // Chassis URI
    std::vector<absl::string_view> uri_parts = absl::StrSplit(*sensor_uri, '/');
    // The URI should be of the format
    // /redfish/v1/Chassis/<chassis-id>/{Thermals#|Sensors|Power#}/...
    // So we can resize the parts down to /redfish/v1/Chassis/<chassis-id> which
    // is equivalent to five parts: "", "redfish", "v1", "Chassis",
    // "<chassis-id>"
    if (uri_parts.size() > 5 && uri_parts[1] == kRfPropertyRedfish &&
        uri_parts[2] == kRfPropertyV1 && uri_parts[3] == kRfPropertyChassis) {
      uri_parts.resize(5);
      auto devpath = GetDevpathForUri(topology, absl::StrJoin(uri_parts, "/"));
      if (devpath.has_value()) {
        return *devpath;
      }
    }
  }
  return std::nullopt;
}

std::optional<std::string> GetSensorDevpathFromNodeTopology(
    RedfishObject *obj, const NodeTopology &topology) {
  return GetSensorDevpathFromNodeTopology(*obj, topology);
}

std::optional<std::string> GetSlotDevpathFromNodeTopology(
    const RedfishObject &obj, absl::string_view parent_uri,
    const NodeTopology &topology) {
  // Append the parent URL to each SLOT Service label.
  std::vector<absl::string_view> uri_parts = absl::StrSplit(parent_uri, '/');
  // The URI should be of the format
  // /redfish/v1/Chassis/<chassis-id>/PCIeSlots
  // So we can resize the parts down to /redfish/v1/Chassis/<chassis-id> which
  // is equivalent to five parts: "", "redfish", "v1", "Chassis",
  // "<chassis-id>"
  if (uri_parts.size() >= 5 && uri_parts[1] == kRfPropertyRedfish &&
      uri_parts[2] == kRfPropertyV1 && uri_parts[3] == kRfPropertyChassis) {
    uri_parts.resize(5);
    auto parent_devpath =
        GetDevpathForUri(topology, absl::StrJoin(uri_parts, "/"));
    if (parent_devpath.has_value()) {
      // The slot_location could be PartLocation property in Location or
      // PhysicalLocation, because Redfish specification uses "PhysicalLocation"
      // instead of "Location" in Drives resource post v1.4.
      auto slot_location =
          obj[kRfPropertyLocation][kRfPropertyPartLocation].AsObject();
      if (!slot_location) {
        slot_location =
            obj[kRfPropertyPhysicalLocation][kRfPropertyPartLocation]
                .AsObject();
      }

      if (slot_location) {
        auto label = slot_location->GetNodeValue<PropertyServiceLabel>();

        if (label.has_value()) {
          return absl::StrCat(*parent_devpath, ":connector:", *label);
        }
      }
    }
  }
  return std::nullopt;
}

std::optional<std::string> GetDevpathFromSlotDevpath(
    absl::string_view slot_devpath) {
  if (!absl::StrContains(slot_devpath, ":connector:")) return std::nullopt;
  return absl::StrReplaceAll(slot_devpath, {{":connector:", "/"}});
}

std::optional<std::string> GetManagerDevpathFromNodeTopology(
    const RedfishObject &obj, const NodeTopology &topology) {
  auto manager_in_chassis =
      obj[kRfPropertyLinks][kRfPropertyManagerInChassis].AsObject();
  if (manager_in_chassis != nullptr) {
    auto manager_odata_id = manager_in_chassis->GetNodeValue<PropertyOdataId>();
    if (manager_odata_id.has_value()) {
      auto devpath = GetDevpathForUri(topology, *manager_odata_id);
      if (devpath.has_value()) {
        auto name = GetConvertedResourceName(obj);
        if (name.has_value()) {
          return absl::StrCat(*devpath, ":device:", *name);
        }
      }
    }
  }

  // Fallback to providing devpath using the non-Manager devpath method
  auto manager_uri = obj.GetUriString();
  if (manager_uri.has_value()) {
    auto manager_devpath = GetDevpathForUri(topology, *manager_uri);
    if (manager_devpath.has_value()) {
      return *manager_devpath;
    }
  }

  return std::nullopt;
}

absl::StatusOr<absl::string_view>
GetReplaceableDevpathForDevpathAndNodeTopology(absl::string_view devpath,
                                               const NodeTopology &topology) {
  auto node_iter = topology.devpath_to_node_map.find(devpath);
  if (node_iter == topology.devpath_to_node_map.end()) {
    return absl::NotFoundError(
        absl::StrCat("NodeTopology not found through devpath: ", devpath));
  }
  Node *node = node_iter->second;
  std::queue<Node *> node_queue;
  node_queue.push(node);
  while (!node_queue.empty() && !node_queue.front()->replaceable) {
    auto parent_iter = topology.node_to_parents.find(node_queue.front());
    if (parent_iter != topology.node_to_parents.end()) {
      for (Node *parent : parent_iter->second) {
        node_queue.push(parent);
      }
    }
    node_queue.pop();
  }
  if (node_queue.empty()) {
    return absl::NotFoundError(
        absl::StrCat("No parent is replaceable: ", devpath));
  }
  return node_queue.front()->local_devpath;
}

}  // namespace ecclesia
