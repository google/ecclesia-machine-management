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

#include <functional>
#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/text_format.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/meta/type_traits.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "absl/types/optional.h"
#include "ecclesia/lib/file/cc_embed_interface.h"
#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/node_topology.h"
#include "ecclesia/lib/redfish/property.h"
#include "ecclesia/lib/redfish/property_definitions.h"
#include "ecclesia/lib/redfish/topology_config.pb.h"
#include "ecclesia/lib/redfish/topology_configs.h"
#include "ecclesia/lib/redfish/types.h"

namespace libredfish {
namespace {

constexpr absl::string_view kDefaultTopologyConfigName =
    "topology_configs/redfish_2021_1.textpb";

absl::optional<TopologyConfig> LoadTopologyConfigFromConfigName(
    absl::string_view config_name) {
  auto filedata =
      ecclesia::GetEmbeddedFileWithName(config_name, kTopologyConfig);
  if (filedata.has_value()) {
    TopologyConfig config;
    if (google::protobuf::TextFormat::ParseFromString(filedata->data(), &config)) {
      return config;
    }
  }
  return absl::nullopt;
}

// Function to iterate through all cables with valid Location tags and call
// callback_function on each
void FindAllCablesHelper(
    RedfishInterface* redfish_intf,
    const TopologyConfig::CableLinkages& cable_linkages,
    std::function<void(std::unique_ptr<RedfishObject>& cable_json,
                       const std::string& upstream_uri)>
        callback_function) {
  redfish_intf->GetRoot()[kRfPropertyCables][kRfPropertyMembers].Each().Do(
      [&](std::unique_ptr<RedfishObject>& cable_json) {
        const auto cable_links = (*cable_json)[kRfPropertyLinks].AsObject();
        if (!cable_links) return;

        absl::optional<std::string> upstream_uri;
        for (const auto& upstream_link : cable_linkages.upstream_links()) {
          // Assuming there is only one upstream resource
          if (auto upstream_obj = (*cable_links)[upstream_link][0].AsObject()) {
            // Collection of Resources
            upstream_uri = upstream_obj->GetUri();
          } else if (auto upstream_obj =
                         (*cable_links)[upstream_link].AsObject()) {
            // Single Resource link
            upstream_uri = upstream_obj->GetUri();
          }
        }
        if (!upstream_uri.has_value()) return;

        // In order to attach cable, we will require a PartLocation to be
        // present
        const auto cable_location =
            (*cable_json)[kRfPropertyLocation][kRfPropertyPartLocation]
                .AsObject();
        if (!cable_location) return;

        callback_function(cable_json, *upstream_uri);
      });
}

struct ResourceTypeAndVersion {
  const std::string resource_type;
  const std::string version;
};

const ResourceTypeAndVersion GetResourceTypeAndVersionFromOdataType(
    const absl::optional<std::string>& type) {
  // Resource type should be of format "#<Resource>.v<version>.<Resource>"
  if (type.has_value()) {
    std::vector<std::string> type_parts = absl::StrSplit(*type, '.');

    // Type parts should be 3 parts; the 2nd part should start with "v"
    absl::string_view version = type_parts[1];
    if (type_parts.size() == 3 && absl::ConsumePrefix(&version, "v")) {
      return {.resource_type = type_parts[2], .version = std::string(version)};
    }
  }
  return {};
}

const ResourceConfig GetResourceConfigFromResourceTypeAndVersion(
    const ResourceTypeAndVersion& resource_type_version,
    const TopologyConfig& config) {
  const auto it =
      config.resource_to_config().find(resource_type_version.resource_type);
  if (it != config.resource_to_config().end()) {
    return it->second;
  }
  return ResourceConfig();
}

// Helper function for finding downstream URIs via Links or first class
// attributes for a given RedfishObject
std::vector<std::string> FindAllDownstreamsUris(const RedfishObject& obj,
                                               const TopologyConfig& config) {
  std::vector<std::string> downstream_uris;

  const auto resource_config = GetResourceConfigFromResourceTypeAndVersion(
      GetResourceTypeAndVersionFromOdataType(
          obj.GetNodeValue<PropertyOdataType>()),
      config);

  for (const auto& array_attribute :
       resource_config.first_class_attributes().array_attributes()) {
    obj[array_attribute].Each().Do([&](std::unique_ptr<RedfishObject>& json) {
      if (json->GetUri().has_value()) {
        downstream_uris.push_back(*json->GetUri());
      }
    });
  }

  for (const auto& collection_attribute :
       resource_config.first_class_attributes().collection_attributes()) {
    obj[collection_attribute][kRfPropertyMembers].Each().Do(
        [&](std::unique_ptr<RedfishObject>& json) {
          if (json->GetUri().has_value()) {
            downstream_uris.push_back(*json->GetUri());
          }
        });
  }

  for (const auto& single_link :
       resource_config.usable_links().singular_links()) {
    if (const auto json = obj[kRfPropertyLinks][single_link].AsObject(); json) {
      if (json->GetUri().has_value()) {
        downstream_uris.push_back(*json->GetUri());
      }
    }
  }

  for (const auto& array_link : resource_config.usable_links().array_links()) {
    obj[kRfPropertyLinks][array_link].Each().Do(
        [&](std::unique_ptr<RedfishObject>& json) {
          if (json->GetUri().has_value()) {
            downstream_uris.push_back(*json->GetUri());
          }
        });
  }

  return downstream_uris;
}

// Helper function to find root chassis from service root
absl::optional<std::string> FindRootChassisUri(RedfishInterface* redfish_intf,
                                               const TopologyConfig& config) {
  const std::string chassis_link = config.find_root_node().chassis_link();

  absl::optional<std::string> chassis_uri;
  if (const auto chassis_obj =
          redfish_intf->GetRoot()[kRfPropertyChassis][kRfPropertyMembers][0]
              .AsObject();
      chassis_obj) {
    chassis_uri = chassis_obj->GetUri();
  }
  if (!chassis_uri.has_value()) return absl::nullopt;

  // Iterate through cables to find upstream connections
  absl::flat_hash_map<std::string, std::string>
      cable_downstream_to_upstream_map;
  FindAllCablesHelper(redfish_intf, config.cable_linkages(),
                      [&](std::unique_ptr<RedfishObject>& cable_json,
                          const std::string& upstream_uri) {
                        for (const auto& downstream_uri :
                             FindAllDownstreamsUris(*cable_json, config)) {
                          cable_downstream_to_upstream_map[downstream_uri] =
                              upstream_uri;
                        }
                      });

  std::string current_chassis_uri = *std::move(chassis_uri);
  absl::optional<std::string> upstream_uri;
  if (auto upstream_chassis_link =
          redfish_intf
              ->GetUri(current_chassis_uri)[kRfPropertyLinks][chassis_link]
              .AsObject();
      upstream_chassis_link) {
    upstream_uri = upstream_chassis_link->GetUri();
  } else {
    const auto it = cable_downstream_to_upstream_map.find(current_chassis_uri);
    if (it != cable_downstream_to_upstream_map.end()) {
      upstream_uri = it->second;
    }
  }

  while (upstream_uri.has_value()) {
    // Continue up the tree to the root chassis
    auto next_chassis = redfish_intf->GetUri(*upstream_uri).AsObject();
    // Upstream uri isn't valid URI
    if (!next_chassis) break;
    // If the upstream chassis exists, update current chassis and find
    // next upstream chassis resource uri
    current_chassis_uri = *std::move(upstream_uri);
    auto upstream_link =
        (*next_chassis)[kRfPropertyLinks][chassis_link].AsObject();
    if (!upstream_link) {
      // Check for cables
      const auto it =
          cable_downstream_to_upstream_map.find(current_chassis_uri);
      if (it != cable_downstream_to_upstream_map.end()) {
        upstream_uri = it->second;
      } else {
        // No further upstream Chassis or Cable
        break;
      }
    } else {
      upstream_uri = upstream_link->GetUri();
    }
  }
  // If the current chassis uri is valid, return it
  if (redfish_intf->GetUri(current_chassis_uri).AsObject()) {
    return current_chassis_uri;
  }
  return absl::nullopt;
}

absl::optional<std::string> FindRootNodeUri(RedfishInterface* redfish_intf,
                                            const TopologyConfig& config) {
  const auto finding_root = config.find_root_node();
  if (finding_root.has_chassis_link()) {
    return FindRootChassisUri(redfish_intf, config);
  }
  return absl::nullopt;
}

using UriToAttachedCableUris =
    absl::flat_hash_map<std::string, std::vector<std::string>>;

UriToAttachedCableUris GetUpstreamUriToAttachedCableMap(
    RedfishInterface* redfish_intf,
    const TopologyConfig::CableLinkages& cable_linkages) {
  UriToAttachedCableUris uri_to_cable_uri;
  FindAllCablesHelper(
      redfish_intf, cable_linkages,
      [&](std::unique_ptr<RedfishObject>& cable_json,
          const std::string upstream_uri) {
        uri_to_cable_uri[upstream_uri].push_back(*cable_json->GetUri());
      });
  return uri_to_cable_uri;
}

struct AttachingNodes {
  Node* parent;
  std::string uri;
};

constexpr absl::string_view kRootDevpath = "/phys";
constexpr absl::string_view kLocationTypeEmbedded = "Embedded";
constexpr absl::string_view kLocationTypeSlot = "Slot";

}  // namespace

NodeTopology CreateTopologyFromRedfishV2(RedfishInterface* redfish_intf) {
  NodeTopology topology;

  absl::optional<TopologyConfig> config =
      LoadTopologyConfigFromConfigName(kDefaultTopologyConfigName);
  if (!config.has_value()) {
    ecclesia::FatalLog() << "No valid config found with name: "
                         << kDefaultTopologyConfigName;
  }

  // Find root chassis to build from using config find root chassis
  const auto root_node_uri = FindRootNodeUri(redfish_intf, *config);
  if (!root_node_uri.has_value()) {
    ecclesia::ErrorLog() << "No root node found for devpath generation";
    return topology;
  }
  // Iterate through all Cables if available:
  UriToAttachedCableUris cable_map =
      GetUpstreamUriToAttachedCableMap(redfish_intf, config->cable_linkages());
  // Create queue of one item with no parent
  std::queue<AttachingNodes> queue;
  queue.push({.parent = nullptr, .uri = *root_node_uri});
  absl::flat_hash_set<std::string> visited_uris;
  while (!queue.empty()) {
    AttachingNodes node_to_attach = queue.front();
    queue.pop();
    if (visited_uris.contains(node_to_attach.uri)) {
      ecclesia::InfoLog() << "Uri found again: " << node_to_attach.uri;
      continue;
    }
    visited_uris.insert(node_to_attach.uri);
    const auto node_json = redfish_intf->GetUri(node_to_attach.uri).AsObject();
    if (!node_json) {
      ecclesia::ErrorLog() << "No node found at " << node_to_attach.uri;
      continue;
    }

    // Get node Resource name
    const ResourceTypeAndVersion resource_type_version =
        GetResourceTypeAndVersionFromOdataType(
            node_json->GetNodeValue<PropertyOdataType>());

    // Getting Location object; Handling special case due to Drive Location
    // attribute deprecation
    auto location_attribute = (*node_json)[kRfPropertyLocation].AsObject()
                                  ? (*node_json)[kRfPropertyLocation]
                                  : (*node_json)[kRfPropertyPhysicalLocation];

    // Handle Location
    Node* current_node_ptr = nullptr;
    if (!node_to_attach.parent) {
      // If no parent: make root
      auto node = std::make_unique<Node>();
      node->type = kBoard;
      node->local_devpath = std::string(kRootDevpath);
      node->name = node_json->GetNodeValue<PropertyName>().value_or("root");
      node->associated_uris.push_back(node_to_attach.uri);

      current_node_ptr = node.get();
      topology.uri_to_associated_node_map[node_to_attach.uri].push_back(
          node.get());
      topology.devpath_to_node_map[node->local_devpath] = node.get();
      topology.nodes.push_back(std::move(node));
    } else if (!location_attribute[kRfPropertyPartLocation].AsObject()) {
      // If no Location but parent: attach to parent in topology
      node_to_attach.parent->associated_uris.push_back(node_to_attach.uri);
      topology.uri_to_associated_node_map[node_to_attach.uri].push_back(
          node_to_attach.parent);
      current_node_ptr = node_to_attach.parent;
    } else {
      // If Location && parent: create child node
      const auto node_location =
          location_attribute[kRfPropertyPartLocation].AsObject();

      absl::optional<std::string> location_type =
          node_location->GetNodeValue<PropertyLocationType>();
      if (!location_type.has_value()) {
        ecclesia::ErrorLog()
            << "Location type missing at URI: " << node_to_attach.uri;
        continue;
      }

      auto node = std::make_unique<Node>();
      auto name = node_json->GetNodeValue<PropertyName>();
      if (!name.has_value()) continue;
      node->name = *std::move(name);
      node->associated_uris.push_back(node_to_attach.uri);
      std::string parent_devpath = node_to_attach.parent->local_devpath;
      if (location_type == kLocationTypeEmbedded) {
        node->local_devpath =
            absl::StrCat(parent_devpath, ":device:", node->name);
        node->type = kDevice;
      } else if (location_type == kLocationTypeSlot) {
        auto label = node_location->GetNodeValue<PropertyServiceLabel>();
        if (!label.has_value()) continue;
        node->local_devpath = absl::StrCat(parent_devpath, "/", *label);
        node->type = kBoard;
      } else {
        ecclesia::ErrorLog()
            << "Unable to handle location type at URI: " << node_to_attach.uri
            << " of type " << *location_type;
        continue;
      }

      // Handle cable type separately once node is created
      if (resource_type_version.resource_type == "Cable") {
        node->type = kCable;
      }

      current_node_ptr = node.get();
      topology.uri_to_associated_node_map[node_to_attach.uri].push_back(
          node.get());
      topology.devpath_to_node_map[node->local_devpath] = node.get();
      topology.node_to_children[node_to_attach.parent].push_back(node.get());
      topology.node_to_parents[node.get()].push_back(node_to_attach.parent);
      topology.nodes.push_back(std::move(node));
    }

    // For every downstream uri from Resource add it to the queue
    for (const auto& downstream_uri :
         FindAllDownstreamsUris(*node_json, *config)) {
      queue.push({.parent = current_node_ptr, .uri = downstream_uri});
    }

    // Adding any downstream cables
    if (const auto it = cable_map.find(node_to_attach.uri);
        it != cable_map.end()) {
      for (const auto& cable_uri : it->second) {
        queue.push({.parent = current_node_ptr, .uri = cable_uri});
      }
    }
  }

  return topology;
}

}  // namespace libredfish
