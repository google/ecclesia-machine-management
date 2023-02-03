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

#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/node_topology.h"
#include "ecclesia/lib/redfish/property_definitions.h"
#include "ecclesia/lib/redfish/types.h"
#include "ecclesia/lib/redfish/utils.h"

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
  auto related_uri = obj[kRfPropertyRelatedItem][0].AsObject();
  if (related_uri != nullptr && related_uri->GetUriString().has_value()) {
    auto related_devpath =
        GetDevpathForUri(topology, *related_uri->GetUriString());
    if (related_devpath.has_value()) {
      return *related_devpath;
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
  if (uri_parts.size() > 5 && uri_parts[1] == kRfPropertyRedfish &&
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

}  // namespace ecclesia
