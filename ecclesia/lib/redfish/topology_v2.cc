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
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/text_format.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/functional/function_ref.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
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
#include "ecclesia/lib/redfish/utils.h"

namespace ecclesia {
namespace {

constexpr absl::string_view kDefaultTopologyConfigName =
    "topology_configs/redfish_2021_1.textpb";

std::optional<TopologyConfig> LoadTopologyConfigFromConfigName(
    absl::string_view config_name) {
  auto filedata =
      ecclesia::GetEmbeddedFileWithName(config_name, kTopologyConfig);
  if (filedata.has_value()) {
    TopologyConfig config;
    if (google::protobuf::TextFormat::ParseFromString(filedata->data(), &config)) {
      return config;
    }
  }
  return std::nullopt;
}

// Function to iterate through all cables with valid Location tags and call
// callback_function on each
void FindAllCablesHelper(RedfishInterface *redfish_intf,
                         const TopologyConfig::CableLinkages &cable_linkages,
                         absl::FunctionRef<RedfishIterReturnValue(
                             std::unique_ptr<RedfishObject> cable_json,
                             const std::string &upstream_uri)>
                             callback_function) {
  redfish_intf->GetRoot()[kRfPropertyCables].Each().Do(
      [&](std::unique_ptr<RedfishObject> &cable_json) {
        DebugLog() << "Handling cable "
                   << cable_json->GetUriString().value_or("<unknown URI>");
        const auto cable_links = (*cable_json)[kRfPropertyLinks].AsObject();
        if (!cable_links) return RedfishIterReturnValue::kContinue;

        DebugLog() << "Looking for upstream connections";
        std::optional<std::string> upstream_uri;
        for (const auto &upstream_link : cable_linkages.upstream_links()) {
          // Assuming there is only one upstream resource
          if (auto upstream_obj = (*cable_links)[upstream_link][0].AsObject()) {
            // Collection of Resources
            DebugLog() << "Found a collection of resources at upstream link: "
                       << upstream_link;
            upstream_uri = upstream_obj->GetUriString();
          } else if (auto upstream_obj =
                         (*cable_links)[upstream_link].AsObject()) {
            // Single Resource link
            DebugLog() << "Found an upstream resources at upstream link: "
                       << upstream_link;
            upstream_uri = upstream_obj->GetUriString();
          }
        }
        if (!upstream_uri.has_value()) {
          DebugLog() << "No upstream connection from cable found";
          return RedfishIterReturnValue::kContinue;
        }

        // In order to attach cable, we will require a PartLocation to be
        // present
        const auto cable_location =
            (*cable_json)[kRfPropertyLocation][kRfPropertyPartLocation]
                .AsObject();
        if (!cable_location) {
          DebugLog() << "Cable has no location information";
          return RedfishIterReturnValue::kContinue;
        }

        RedfishIterReturnValue retval =
            callback_function(std::move(cable_json), *upstream_uri);
        return retval;
      });
}

struct ResourceTypeAndVersion {
  const std::string resource_type;
  const std::string version;
};

ResourceTypeAndVersion GetResourceTypeAndVersionFromOdataType(
    const std::optional<std::string> &type) {
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

ResourceConfig GetResourceConfigFromResourceTypeAndVersion(
    const ResourceTypeAndVersion &resource_type_version,
    const TopologyConfig &config) {
  const auto it =
      config.resource_to_config().find(resource_type_version.resource_type);
  if (it != config.resource_to_config().end()) {
    return it->second;
  }
  return ResourceConfig();
}

// Helper function for finding downstream URIs via Links or first class
// attributes for a given RedfishObject
std::vector<std::unique_ptr<RedfishObject>> FindAllDownstreamsUris(
    const RedfishObject &obj, const TopologyConfig &config) {
  std::vector<std::unique_ptr<RedfishObject>> downstream_objs;

  const auto resource_config = GetResourceConfigFromResourceTypeAndVersion(
      GetResourceTypeAndVersionFromOdataType(
          obj.GetNodeValue<PropertyOdataType>()),
      config);

  for (const auto &array_attribute :
       resource_config.first_class_attributes().array_attributes()) {
    obj[array_attribute].Each().Do([&](std::unique_ptr<RedfishObject> &json) {
      DebugLog() << "Found downstream obj at " << array_attribute;
      downstream_objs.push_back(std::move(json));
      return RedfishIterReturnValue::kContinue;
    });
  }

  for (const auto &collection_attribute :
       resource_config.first_class_attributes().collection_attributes()) {
    obj[collection_attribute][kRfPropertyMembers].Each().Do(
        [&](std::unique_ptr<RedfishObject> &json) {
          DebugLog() << "Found downstream obj at " << collection_attribute;
          downstream_objs.push_back(std::move(json));
          return RedfishIterReturnValue::kContinue;
        });
  }

  for (const auto &singular_attribute :
       resource_config.first_class_attributes().singular_attributes()) {
    if (std::unique_ptr<RedfishObject> singular_obj =
            obj[singular_attribute].AsObject();
        singular_obj != nullptr) {
      DebugLog() << "Found downstream obj at " << singular_attribute;
      downstream_objs.push_back(std::move(singular_obj));
    }
  }

  for (const auto &single_link :
       resource_config.usable_links().singular_links()) {
    if (std::unique_ptr<RedfishObject> json =
            obj[kRfPropertyLinks][single_link].AsObject();
        json) {
      DebugLog() << "Found downstream obj at Links." << single_link;
      downstream_objs.push_back(std::move(json));
    }
  }

  for (const auto &array_link : resource_config.usable_links().array_links()) {
    obj[kRfPropertyLinks][array_link].Each().Do(
        [&](std::unique_ptr<RedfishObject> &json) {
          DebugLog() << "Found downstream obj at Links." << array_link;
          downstream_objs.push_back(std::move(json));
          return RedfishIterReturnValue::kContinue;
        });
  }

  return downstream_objs;
}

// Helper function to find root chassis from service root
std::unique_ptr<RedfishObject> FindRootChassisUri(
    RedfishInterface *redfish_intf, const TopologyConfig &config) {
  auto chassis_iter = redfish_intf->GetRoot()[kRfPropertyChassis].AsIterable();
  if (chassis_iter == nullptr || chassis_iter->Size() == 0) {
    DebugLog() << "No Chassis in Chassis collection";
    return nullptr;
  }

  // Pop the first available Chassis (guaranteed since iter has Size > 0)
  std::unique_ptr<RedfishObject> chassis_obj = (*chassis_iter)[0].AsObject();
  std::optional<std::string> chassis_uri = chassis_obj->GetUriString();
  if (chassis_obj == nullptr || !chassis_uri.has_value()) {
    DebugLog() << "Chassis obj is not valid from Collection";
    return nullptr;
  }

  const std::string chassis_link = config.find_root_node().chassis_link();
  DebugLog() << "Using chassis link value: Links." << chassis_link;

  // Iterate through cables to find upstream connections
  absl::flat_hash_map<std::string, std::string>
      cable_downstream_to_upstream_map;
  FindAllCablesHelper(redfish_intf, config.cable_linkages(),
                      [&](std::unique_ptr<RedfishObject> cable_json,
                          const std::string &upstream_uri) {
                        for (std::unique_ptr<RedfishObject> &downstream_obj :
                             FindAllDownstreamsUris(*cable_json, config)) {
                          if (std::optional<std::string> downstream_uri =
                                  downstream_obj->GetUriString();
                              downstream_uri.has_value()) {
                            DebugLog() << "Cable between " << *downstream_uri
                                       << " and " << upstream_uri;
                            cable_downstream_to_upstream_map[*downstream_uri] =
                                upstream_uri;
                          }
                        }
                        return RedfishIterReturnValue::kContinue;
                      });

  DebugLog() << "Checking for upstream Chassis obj for " << *chassis_uri;
  std::unique_ptr<RedfishObject> upstream_chassis_obj =
      (*chassis_obj)[kRfPropertyLinks][chassis_link].AsObject();
  if (upstream_chassis_obj == nullptr) {
    DebugLog() << "None found; checking cables";
    const auto it = cable_downstream_to_upstream_map.find(*chassis_uri);
    if (it != cable_downstream_to_upstream_map.end()) {
      DebugLog() << "Found upstream cable for " << *chassis_uri;
      upstream_chassis_obj = redfish_intf->CachedGetUri(it->second).AsObject();
    }
  }

  while (upstream_chassis_obj != nullptr) {
    // If the upstream chassis exists, update current chassis and find
    // next upstream chassis resource uri
    chassis_obj = std::move(upstream_chassis_obj);
    chassis_uri = chassis_obj->GetUriString();
    DebugLog() << "Checking for upstream Chassis obj for "
               << chassis_uri.value_or("<unknown URI>");
    upstream_chassis_obj =
        (*chassis_obj)[kRfPropertyLinks][chassis_link].AsObject();
    if (upstream_chassis_obj == nullptr && chassis_uri.has_value()) {
      // Check for cables
      DebugLog() << "None found; checking cables";
      const auto it = cable_downstream_to_upstream_map.find(*chassis_uri);
      if (it != cable_downstream_to_upstream_map.end()) {
        DebugLog() << "Found upstream cable for " << *chassis_uri;
        upstream_chassis_obj =
            redfish_intf->CachedGetUri(it->second).AsObject();
      } else {
        // No further upstream Chassis or Cable
        break;
      }
    }
  }

  DebugLog() << "Found root Chassis: " << chassis_uri.value_or("<unknown URI>");
  return chassis_obj;
}

std::unique_ptr<RedfishObject> FindRootNode(RedfishInterface *redfish_intf,
                                            const TopologyConfig &config) {
  const auto &finding_root = config.find_root_node();
  if (finding_root.has_chassis_link()) {
    DebugLog() << "Finding root chassis";
    return FindRootChassisUri(redfish_intf, config);
  }
  return nullptr;
}

using UriToAttachedCableUris =
    absl::flat_hash_map<std::string, std::vector<std::string>>;

UriToAttachedCableUris GetUpstreamUriToAttachedCableMap(
    RedfishInterface *redfish_intf,
    const TopologyConfig::CableLinkages &cable_linkages) {
  UriToAttachedCableUris uri_to_cable_uri;
  FindAllCablesHelper(
      redfish_intf, cable_linkages,
      [&](std::unique_ptr<RedfishObject> cable_json,
          const std::string &upstream_uri) {
        if (auto uri = cable_json->GetUriString(); uri.has_value()) {
          DebugLog() << "Mapping " << upstream_uri << "to cable " << *uri;
          uri_to_cable_uri[upstream_uri].push_back(*std::move(uri));
        }
        return RedfishIterReturnValue::kContinue;
      });
  return uri_to_cable_uri;
}

struct AttachingNodes {
  Node *parent;
  std::unique_ptr<RedfishObject> obj;
};

constexpr absl::string_view kRootDevpath = "/phys";
constexpr absl::string_view kLocationTypeEmbedded = "Embedded";
constexpr absl::string_view kLocationTypeSlot = "Slot";
constexpr absl::string_view kLocationTypeConnector = "Connector";
constexpr absl::string_view kLocationTypeBackplane = "Backplane";
constexpr absl::string_view kLocationTypeBay = "Bay";
constexpr absl::string_view kLocationTypeSocket = "Socket";

}  // namespace

NodeTopology CreateTopologyFromRedfishV2(RedfishInterface *redfish_intf) {
  NodeTopology topology;

  DebugLog() << "Loading topology config: " << kDefaultTopologyConfigName;
  std::optional<TopologyConfig> config =
      LoadTopologyConfigFromConfigName(kDefaultTopologyConfigName);
  if (!config.has_value()) {
    ecclesia::FatalLog() << "No valid config found with name: "
                         << kDefaultTopologyConfigName;
  }

  // Find root chassis to build from using config find root chassis
  DebugLog() << "Starting root node search";
  auto root_node_uri = FindRootNode(redfish_intf, *config);
  if (root_node_uri == nullptr) {
    ecclesia::ErrorLog() << "No root node found for devpath generation";
    return topology;
  }
  // Iterate through all Cables if available
  DebugLog() << "Creating cable map for upstream and downstream links";
  UriToAttachedCableUris cable_map =
      GetUpstreamUriToAttachedCableMap(redfish_intf, config->cable_linkages());
  DebugLog() << "Cable map completed";
  // Create queue of one item with no parent
  std::queue<AttachingNodes> queue;
  queue.push({.parent = nullptr, .obj = std::move(root_node_uri)});
  absl::flat_hash_set<std::string> visited_uris;
  while (!queue.empty()) {
    AttachingNodes node_to_attach = std::move(queue.front());
    queue.pop();

    if (node_to_attach.obj == nullptr) {
      ecclesia::InfoLog() << "Object in queue is null from parent: "
                          << (node_to_attach.parent == nullptr
                                  ? "(Root)"
                                  : node_to_attach.parent->local_devpath);
      continue;
    }

    std::optional<std::string> current_uri = node_to_attach.obj->GetUriString();
    if (!current_uri.has_value()) {
      ecclesia::InfoLog() << "Unable to find URI for object in queue"
                          << node_to_attach.obj->DebugString();
      continue;
    }
    if (visited_uris.contains(*current_uri)) {
      ecclesia::InfoLog() << "Uri found again: " << *current_uri;
      continue;
    }
    visited_uris.insert(*current_uri);

    DebugLog() << "Handling node: " << *current_uri << " with parent "
               << (node_to_attach.parent != nullptr
                       ? node_to_attach.parent->local_devpath
                       : "(None)");

    // Get node Resource name
    const ResourceTypeAndVersion resource_type_version =
        GetResourceTypeAndVersionFromOdataType(
            node_to_attach.obj->GetNodeValue<PropertyOdataType>());
    DebugLog() << "Current node version and type: "
               << resource_type_version.resource_type << "/"
               << resource_type_version.version;

    // Getting Location object; Handling special case due to Drive Location
    // attribute deprecation
    auto location_attribute =
        (*node_to_attach.obj)[kRfPropertyLocation].AsObject() != nullptr
            ? (*node_to_attach.obj)[kRfPropertyLocation]
            : (*node_to_attach.obj)[kRfPropertyPhysicalLocation];

    // Handle Location
    Node *current_node_ptr = nullptr;
    if (!node_to_attach.parent) {
      // If no parent: make root
      DebugLog() << "Creating root node";
      auto node = std::make_unique<Node>();
      node->type = kBoard;
      node->local_devpath = std::string(kRootDevpath);
      auto name = GetConvertedResourceName(node_to_attach.obj.get());
      if (name.has_value()) {
        node->name = *std::move(name);
      } else {
        node->name = "root";
      }
      node->associated_uris.push_back(*current_uri);

      current_node_ptr = node.get();
      topology.uri_to_associated_node_map[*current_uri].push_back(node.get());
      topology.devpath_to_node_map[node->local_devpath] = node.get();
      // Also push google service root; order matters so that the first chassis
      // can default to being the real root.
      if (config->find_root_node().google_service_root()) {
        DebugLog() << "Checking for Google Service Root";
        std::unique_ptr<RedfishObject> new_root =
            redfish_intf->GetRoot({}, ServiceRootUri::kGoogle).AsObject();
        if (new_root != nullptr) {
          queue.push({.parent = node.get(), .obj = std::move(new_root)});
        }
      }
      topology.nodes.push_back(std::move(node));
    } else if (!location_attribute[kRfPropertyPartLocation].AsObject()) {
      // If no Location but parent: attach to parent in topology
      DebugLog() << "No Location information; attaching to parent Node";
      node_to_attach.parent->associated_uris.push_back(*current_uri);
      topology.uri_to_associated_node_map[*current_uri].push_back(
          node_to_attach.parent);
      current_node_ptr = node_to_attach.parent;
    } else {
      // Check to see if the current node is absent by checking the status; the
      // assumption is that a lack of status or state means that the current
      // node is attached to the topology
      const auto status_object =
          (*node_to_attach.obj)[kRfPropertyStatus].AsObject();
      if (status_object != nullptr &&
          status_object->GetNodeValue<PropertyState>().value_or("") ==
              kRfPropertyAbsent) {
        ecclesia::InfoLog()
            << *current_uri
            << " is absent; not including in topology generation";
        continue;
      }

      // If Location && parent: create child node
      DebugLog() << "Creating child node";
      const auto node_location =
          location_attribute[kRfPropertyPartLocation].AsObject();

      std::optional<std::string> location_type =
          node_location->GetNodeValue<PropertyLocationType>();
      if (!location_type.has_value()) {
        ecclesia::ErrorLog()
            << "Location type missing at URI: " << *current_uri;
        continue;
      }

      auto node = std::make_unique<Node>();
      auto name = GetConvertedResourceName(node_to_attach.obj.get());
      if (!name.has_value()) continue;
      node->name = *std::move(name);
      node->associated_uris.push_back(*current_uri);
      std::string parent_devpath = node_to_attach.parent->local_devpath;
      if (location_type == kLocationTypeEmbedded) {
        node->local_devpath =
            absl::StrCat(parent_devpath, ":device:", node->name);
        node->type = kDevice;
      } else if (location_type == kLocationTypeSlot ||
                 location_type == kLocationTypeConnector ||
                 location_type == kLocationTypeBay ||
                 location_type == kLocationTypeBackplane ||
                 location_type == kLocationTypeSocket) {
        // These location types probably mean the hardware piece is plugged into
        // the parent board, thus model the downstream component as a plug-in.
        auto label = node_location->GetNodeValue<PropertyServiceLabel>();
        if (!label.has_value()) continue;
        node->local_devpath = absl::StrCat(parent_devpath, "/", *label);
        node->type = kBoard;
      } else {
        ecclesia::ErrorLog()
            << "Unable to handle location type at URI: " << *current_uri
            << " of type " << *location_type;
        continue;
      }

      // Handle cable type separately once node is created
      if (resource_type_version.resource_type == "Cable") {
        node->type = kCable;
      }

      DebugLog() << "Child node created: [" << node->local_devpath << ", "
                 << node->type << ", " << node->name << "]";

      current_node_ptr = node.get();
      topology.uri_to_associated_node_map[*current_uri].push_back(node.get());
      topology.devpath_to_node_map[node->local_devpath] = node.get();
      topology.node_to_children[node_to_attach.parent].push_back(node.get());
      topology.node_to_parents[node.get()].push_back(node_to_attach.parent);
      topology.nodes.push_back(std::move(node));
    }

    DebugLog() << "Finding Downstream Nodes";
    // For every downstream uri from Resource add it to the queue
    std::vector<std::unique_ptr<RedfishObject>> downstream_objs =
        FindAllDownstreamsUris(*node_to_attach.obj, *config);
    for (int i = 0; i < downstream_objs.size(); ++i) {
      queue.push(
          {.parent = current_node_ptr, .obj = std::move(downstream_objs[i])});
    }

    // Adding any downstream cables
    if (const auto it = cable_map.find(*current_uri); it != cable_map.end()) {
      DebugLog() << "Found downstream cables";
      for (const auto &cable_uri : it->second) {
        DebugLog() << "Cable URI: " << cable_uri;
        std::unique_ptr<RedfishObject> cable_obj =
            redfish_intf->CachedGetUri(cable_uri).AsObject();
        if (!cable_obj) continue;
        queue.push({.parent = current_node_ptr, .obj = std::move(cable_obj)});
      }
    }
  }

  return topology;
}

}  // namespace ecclesia
