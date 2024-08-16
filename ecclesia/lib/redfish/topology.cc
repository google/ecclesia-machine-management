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

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/hash/hash.h"
#include "absl/meta/type_traits.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/node_topology.h"
#include "ecclesia/lib/redfish/property_definitions.h"
#include "ecclesia/lib/redfish/property_values.h"
#include "ecclesia/lib/redfish/topology_config.pb.h"
#include "ecclesia/lib/redfish/topology_v2.h"
#include "ecclesia/lib/redfish/types.h"
#include "ecclesia/lib/redfish/utils.h"

namespace ecclesia {
namespace {

constexpr char kDevpathConnectorSuffix[] = "connector:";
constexpr char kDevpathDeviceSuffix[] = "device:";
constexpr char kDevpathRoot[] = "/phys";

constexpr char kPhysicalContextConnector[] = "Connector";

// Component is a data structure containing processed data derived from Redfish
// properties. At a high level, Component represents a physical node in the
// system (e.g. board, connector, device). It contains the information required
// to generate a local devpath.
struct Component {
  // Redfish name property value
  std::string name;
  // Redfish model property value
  std::string model;
  // Redfish @odata.id property value is a Redfish URI identifier
  std::string odata_id;
  // Type is used in the devpath generation algorithm.
  NodeType type;

  // This devpath is the physical identifier specifically in the domain of the
  // associated Redfish backend. This devpath should not be confused with the
  // global devpath as seen and translated at the Machine Manager level.
  std::string local_devpath;
  // This string is the upstream connector sequence (all connector names
  // concatenated with "/") of the plugin this component resides on,
  // specifically in the domain of the associated Redfish backend. This
  // connector sequence will not reference any connectors in other domains
  // visible at the Machine Manager level.
  std::string upstream_connector_sequence;

  // List of URIs which are associated with this component.
  std::vector<std::string> associated_uris;
};

// Assembly is a data structure containing processed data derived from Redfish
// properties. At a high level, Assembly represents a field replaceable unit
// comprised of a collection of Components.
struct Assembly {
  // Redfish name property value
  std::string name;
  // Redfish model property value
  std::string model;
  // The constituent components in this Assembly
  std::vector<std::unique_ptr<Component>> components;
  // Redfish @odata.id of all the upstream components.
  std::vector<std::string> upstream_odata_ids;
};

// If this Redfish object has a "Assembly" property, retrieve it and append it
// to assembly_out.
void ExtractAssemblyProperties(const RedfishObject &obj,
                               std::vector<RedfishVariant> *assembly_out) {
  auto assembly_node = obj[kRfPropertyAssembly].AsObject();
  if (!assembly_node) return;

  auto assembly_collection =
      (*assembly_node)[kRfPropertyAssemblies].AsIterable();
  if (!assembly_collection) return;

  for (auto assembly : *assembly_collection) {
    auto assembly_obj = assembly.AsObject();
    if (!assembly_obj || !AssemblyIsEnabled(*assembly_obj)) {
      continue;
    }
    assembly_out->push_back(std::move(assembly));
  }
}

// Finds Assembly resources under the System URI by searching hardcoded URIs:
// * /redfish/v1/Systems/{id}/Assembly
// * /redfish/v1/Systems/{id}/Memory/{id}/Assembly
// * /redfish/v1/Systems/{id}/Processors/{id}/Assembly
// * /redfish/v1/Systems/{id}/EthernetInterfaces/{id}/Assembly
// * /redfish/v1/Systems/{id}/Storage/{id}Drives/{id}/Assembly
void ExtractAssemblyFromSystemUri(const RedfishObject &root_obj,
                                  std::vector<RedfishVariant> *assembly_out) {
  auto system_collection = root_obj[kRfPropertySystems].AsIterable();
  if (!system_collection) return;

  for (auto system : *system_collection) {
    auto system_obj = system.AsObject();
    if (!system_obj) continue;
    ExtractAssemblyProperties(*system_obj, assembly_out);

    auto memory_collection = (*system_obj)[kRfPropertyMemory].AsIterable();
    if (memory_collection) {
      for (auto memory : *memory_collection) {
        auto memory_obj = memory.AsObject();
        if (!memory_obj) continue;
        ExtractAssemblyProperties(*memory_obj, assembly_out);
      }
    }

    auto processors_collection =
        (*system_obj)[kRfPropertyProcessors].AsIterable();
    if (processors_collection) {
      for (auto processor : *processors_collection) {
        auto processor_obj = processor.AsObject();
        if (!processor_obj) continue;
        ExtractAssemblyProperties(*processor_obj, assembly_out);
      }
    }

    auto ethernet_interfaces_collection =
        (*system_obj)[kRfPropertyEthernetInterfaces].AsIterable();
    if (ethernet_interfaces_collection) {
      for (auto ethernet_interface : *ethernet_interfaces_collection) {
        auto ethernet_interface_obj = ethernet_interface.AsObject();
        if (!ethernet_interface_obj) continue;
        ExtractAssemblyProperties(*ethernet_interface_obj, assembly_out);
      }
    }

    auto storage_collection = (*system_obj)[kRfPropertyStorage].AsIterable();
    if (storage_collection) {
      for (auto storage : *storage_collection) {
        auto storage_obj = storage.AsObject();
        if (!storage_obj) continue;
        auto drives_collection = (*storage_obj)[kRfPropertyDrives].AsIterable();
        if (drives_collection) {
          for (auto drive : *drives_collection) {
            auto drive_obj = drive.AsObject();
            if (!drive_obj) continue;
            ExtractAssemblyProperties(*drive_obj, assembly_out);
          }
        }
      }
    }
  }
}

// Finds Assembly resources under the Chassis URI by searching hardcoded URIs:
// * /redfish/v1/chassis/{chassis_id}/Assembly
void ExtractAssemblyFromChassisUri(const RedfishObject &root_obj,
                                   std::vector<RedfishVariant> *assembly_out) {
  auto chassis_collection = root_obj[kRfPropertyChassis].AsIterable();
  if (!chassis_collection) return;

  for (auto chassis : *chassis_collection) {
    auto chassis_obj = chassis.AsObject();
    if (!chassis_obj) continue;
    ExtractAssemblyProperties(*chassis_obj, assembly_out);
  }
}

std::vector<RedfishVariant> FindAssemblyPayloads(
    RedfishInterface *redfish_intf) {
  std::vector<RedfishVariant> assemblies;
  auto root = redfish_intf->GetRoot().AsObject();
  if (!root) return assemblies;

  ExtractAssemblyFromChassisUri(*root, &assemblies);
  ExtractAssemblyFromSystemUri(*root, &assemblies);

  return assemblies;
}

// Modifies the provided vector of Components by adding devpath information.
// Devpaths are generated from the provided upstream_connector_sequence and
// appending a suffix depending on the Component type.
void GenerateDevpathsForComponents(
    std::vector<std::unique_ptr<Component>> *components,
    absl::string_view upstream_connector_sequence) {
  for (auto &component : *components) {
    component->upstream_connector_sequence =
        std::string(upstream_connector_sequence);
    switch (component->type) {
      case kBoard:
      case kCable:
        component->local_devpath = std::string(upstream_connector_sequence);
        break;
      case kConnector:
        component->local_devpath =
            absl::StrCat(upstream_connector_sequence, ":",
                         kDevpathConnectorSuffix, component->name);
        break;
      case kDevice:
        component->local_devpath =
            absl::StrCat(upstream_connector_sequence, ":", kDevpathDeviceSuffix,
                         component->name);
        break;
    }
  }
}

// Assigns devpaths to all Components in the provided list of Assemblies.
// The input parameter assemblies will have its members modified with devpath
// information.
void GenerateDevpaths(std::vector<Assembly> *assemblies) {
  // Map @odata.id identifier to a Component. The Component pointers in this
  // map must have a local_devpath and upstream_connector_sequence assigned.
  // This map is used for downstream components to get their upstream connector
  // paths.
  absl::flat_hash_map<absl::string_view, const Component *>
      odata_id_to_component;
  // This set stores the Assembly structures which have already been processed.
  absl::flat_hash_set<Assembly *> processed;
  // last_processed_count will let us know if we are unable to process some
  // assemblies and we are stuck in the loop without making progress.
  size_t last_processed_count;
  do {
    last_processed_count = processed.size();
    // We need to make multiple passes through the list of Assemblies and assign
    // devpaths to components in a topological order, with the root Assemblies
    // first. Downstream assemblies cannot determine their devpaths without
    // their upstream connectors having a devpath.
    for (auto &assembly : *assemblies) {
      if (processed.count(&assembly) != 0u) continue;
      // If there are no upstream_odata_ids values, this Assembly is the root.
      std::string upstream_connector_sequence = kDevpathRoot;
      if (!assembly.upstream_odata_ids.empty()) {
        // Check that our upstream reference has had a devpath assigned to it.
        // If not, we have to skip assignment this iteration and try again
        // in a subsequent iteration.
        auto upstream_component =
            odata_id_to_component.find(assembly.upstream_odata_ids[0]);
        if (upstream_component == odata_id_to_component.end()) continue;
        upstream_connector_sequence = absl::StrCat(
            upstream_component->second->upstream_connector_sequence, "/",
            upstream_component->second->name);
      }
      // After new devpaths have been assigned, we need to update our upstream
      // mappings for odata.id to Component.
      GenerateDevpathsForComponents(&assembly.components,
                                    upstream_connector_sequence);
      for (const auto &component : assembly.components) {
        odata_id_to_component[component->odata_id] = component.get();
      }
      processed.insert(&assembly);
    }
  } while (processed.size() > last_processed_count);
}

// Parses a raw Redfish Assembly payload into an Assembly data structure.
std::optional<Assembly> ProcessAssembly(RedfishObject *assembly_payload) {
  Assembly assembly;

  auto fru_name = assembly_payload->GetNodeValue<PropertyName>();
  if (!fru_name.has_value()) return std::nullopt;
  assembly.name = fru_name.value();

  assembly.model = assembly.name;
  auto fru_model = assembly_payload->GetNodeValue<PropertyModel>();
  if (fru_model.has_value()) {
    assembly.model = fru_model.value();
  }

  auto oem = (*assembly_payload)[kRfPropertyOem].AsObject();
  if (!oem) return assembly;

  auto google = (*oem)[kRfOemPropertyGoogle].AsObject();
  if (!google) return assembly;

  // Fetch the Assembly's upstream odata.id reference if it exists
  auto attached_to = (*google)[kRfOemPropertyAttachedTo].AsIterable();
  if (attached_to && !attached_to->Empty()) {
    for (auto upstream_variant : *attached_to) {
      if (auto upstream = upstream_variant.AsObject(); upstream) {
        if (auto uri = upstream->GetUriString(); uri.has_value()) {
          assembly.upstream_odata_ids.push_back(std::move(*uri));
        }
      }
    }
  }

  auto components = (*google)[kRfOemPropertyComponents].AsIterable();
  if (!components) return assembly;

  for (auto component_resource : *components) {
    auto component_resource_obj = component_resource.AsObject();
    if (!component_resource_obj) continue;
    auto component = std::make_unique<Component>();

    auto odata_id = component_resource_obj->GetNodeValue<PropertyOdataId>();
    if (!odata_id.has_value()) continue;
    component->odata_id = odata_id.value();

    auto component_name = component_resource_obj->GetNodeValue<PropertyName>();
    if (!component_name.has_value()) continue;
    component->name = component_name.value();

    // If no model is listed in Redfish object, use name as the fallback.
    component->model = component->name;
    auto component_model =
        component_resource_obj->GetNodeValue<PropertyModel>();
    if (component_model.has_value()) {
      component->model = component_model.value();
    }

    // Determine the component type. Assume by default it is a device.
    component->type = NodeType::kDevice;
    if (component->name == fru_name.value()) {
      // If the Component's name matches the Assembly's name, it is the board.
      component->type = NodeType::kBoard;
    } else {
      // If the Component's PhysicalContext indicates it is a connector, it is
      // a connector.
      auto physical_context =
          component_resource_obj->GetNodeValue<PropertyPhysicalContext>();
      if (physical_context.has_value()) {
        if (physical_context.value() == kPhysicalContextConnector) {
          component->type = NodeType::kConnector;
        }
      }
    }

    // For every resource associated with this Component, extract the associated
    // URIs.
    auto associated_with =
        (*component_resource_obj)[kRfOemPropertyAssociatedWith];
    if (auto associated_with_iterable = associated_with.AsIterable()) {
      for (auto association : *associated_with_iterable) {
        if (auto association_obj = association.AsObject()) {
          if (auto uri = association_obj->GetUriString(); uri.has_value()) {
            component->associated_uris.push_back(std::move(*uri));
          }
        }
      }
    }

    assembly.components.push_back(std::move(component));
  }

  return assembly;
}

// Run during initialization. Fetches all Assemblies from a Redfish server
// and parses them into data structs.
std::vector<Assembly> CreateAssembliesFromRedfish(
    RedfishInterface *redfish_intf) {
  std::vector<Assembly> assemblies;
  for (auto &assembly : FindAssemblyPayloads(redfish_intf)) {
    auto view = assembly.AsObject();
    if (!view) continue;
    auto processed_info = ProcessAssembly(view.get());
    if (processed_info.has_value()) {
      assemblies.push_back(std::move(processed_info.value()));
    }
  }

  GenerateDevpaths(&assemblies);
  return assemblies;
}

NodeTopology CreateNodeTopologyFromAssemblies(
    const std::vector<Assembly> &assemblies) {
  NodeTopology node_topology;

  // A map of odata IDs to the Node* created for them.
  absl::flat_hash_map<absl::string_view, Node *> odata_id_to_node;
  // A map of nodes to the odata IDs of their attached-to components.
  absl::flat_hash_map<Node *, std::vector<absl::string_view>>
      node_to_attachedto_odata_ids;

  // Create a Node for each Component.
  for (const Assembly &assembly : assemblies) {
    if (assembly.components.empty()) continue;

    // Find the "board" component of the assembly and grab its odata ID. This
    // will be needed to populate the attached to map for the other components.
    absl::string_view board_odata_id;
    for (const auto &component : assembly.components) {
      if (component->type == kBoard) {
        board_odata_id = component->odata_id;
        break;
      }
    }

    // Construct all the node objects, and fill in the relevant odata ID maps
    // as we go.
    std::vector<std::unique_ptr<Node>> current_nodes;
    for (const auto &component : assembly.components) {
      auto node = std::make_unique<Node>();

      node->name = component->name;
      node->model = component->model;
      node->local_devpath = component->local_devpath;
      node->type = component->type;
      node_topology.devpath_to_node_map[node->local_devpath] = node.get();

      node->associated_uris = std::move(component->associated_uris);
      for (const auto &uri : node->associated_uris) {
        node_topology.uri_to_associated_node_map[uri].push_back(node.get());
      }

      // Track the node in our odata ID map.
      odata_id_to_node[component->odata_id] = node.get();

      // Add in attached-to information for the node. For the board node it gets
      // the attached-to information for the assembly. For everything else it is
      // considered "attached to" the board.
      if (component->type == kBoard) {
        auto &attached_to = node_to_attachedto_odata_ids[node.get()];
        for (const auto &odata_id : assembly.upstream_odata_ids) {
          attached_to.push_back(odata_id);
        }
      } else {
        // If we don't have a board odata ID then just point the node at
        // nothing. This makes it an orphan, but there's not really much else we
        // can do with such "bad" data.
        if (board_odata_id.empty()) {
          node_to_attachedto_odata_ids[node.get()] = {};
        } else {
          node_to_attachedto_odata_ids[node.get()] = {board_odata_id};
        }
      }

      current_nodes.push_back(std::move(node));
    }

    // If this Node is a board, determine if it is actually a cable. We do this
    // by assuming all Redfish cables have only 2 components: one "board"
    // representing the cable plugin and one downstream connector named
    // "DOWNLINK". The "board" representing the cable plugin will have the same
    // name as the Assembly representing the cable plugin.
    Node *assembly_primary_node;
    for (const auto &nodes : current_nodes) {
      if (nodes->name == assembly.name) {
        assembly_primary_node = nodes.get();
      }
    }
    if (!assembly_primary_node) continue;
    if (assembly_primary_node->type == kBoard && current_nodes.size() == 2) {
      for (const auto &nodes : current_nodes) {
        if (nodes->name == "DOWNLINK") {
          assembly_primary_node->type = kCable;
        }
      }
    }

    for (auto &node : current_nodes) {
      node_topology.nodes.push_back(std::move(node));
    }
  }

  // Now that we have all the nodes we can use the odata maps we constructed to
  // populate the attached-to and attached-from maps.
  for (const auto &[node, attached_to] : node_to_attachedto_odata_ids) {
    auto &attached_to_nodes = node_topology.node_to_parents[node];
    for (absl::string_view odata_id : attached_to) {
      auto attached_to_node = odata_id_to_node.find(odata_id);
      if (attached_to_node != odata_id_to_node.end()) {
        attached_to_nodes.push_back(attached_to_node->second);
        node_topology.node_to_children[attached_to_node->second].push_back(
            node);
      }
    }
  }

  return node_topology;
}

RedfishNodeTopologyRepresentation GetNodeTopologyReprensentation(
    RedfishInterface *redfish_intf) {
  auto service_root = redfish_intf->GetRoot().AsObject();
  if (service_root) {
    if (auto oem =
            (*service_root)[kRfPropertyOem][kRfOemPropertyGoogle].AsObject();
        oem) {
      if (auto topology_type =
              oem->GetNodeValue<OemGooglePropertyTopologyRepresentation>();
          topology_type.has_value()) {
        if (topology_type.value() == ecclesia::kTopologyRepresentationV1) {
          return REDFISH_TOPOLOGY_V1;
        }
        if (topology_type.value() == ecclesia::kTopologyRepresentationV2) {
          return REDFISH_TOPOLOGY_V2;
        }
      }
    }
  }
  return REDFISH_TOPOLOGY_UNSPECIFIED;
}

NodeTopology CreateTopologyFromRedfishHelper(
    RedfishInterface *redfish_intf,
    RedfishNodeTopologyRepresentation &default_redfish_topology_reprensentation,
    std::optional<absl::string_view> topology_config_name) {
  auto redfish_topology_version = GetNodeTopologyReprensentation(redfish_intf);
  // If the Redfish Agent specifies it's using REDFISH_TOPOLOGY_V1, or if it's
  // unspecified in the Redfish Agent but the default topology is
  // REDFISH_TOPOLOGY_V1, use REDFISH_TOPOLOGY_V1 to create the assemblies and
  // node topology.
  if (redfish_topology_version == REDFISH_TOPOLOGY_V1 ||
      (redfish_topology_version == REDFISH_TOPOLOGY_UNSPECIFIED &&
       default_redfish_topology_reprensentation == REDFISH_TOPOLOGY_V1)) {
    std::vector<Assembly> assemblies =
        CreateAssembliesFromRedfish(redfish_intf);
    return CreateNodeTopologyFromAssemblies(assemblies);
  }
  if (topology_config_name.has_value()) {
    return CreateTopologyFromRedfishV2(redfish_intf, *topology_config_name);
  }
  return CreateTopologyFromRedfishV2(redfish_intf);
}
}  // namespace

NodeTopology CreateTopologyFromRedfish(
    RedfishInterface *redfish_intf,
    RedfishNodeTopologyRepresentation
        default_redfish_topology_reprensentation) {
  return CreateTopologyFromRedfishHelper(
      redfish_intf, default_redfish_topology_reprensentation, std::nullopt);
}

NodeTopology CreateTopologyFromRedfish(
    RedfishInterface *redfish_intf, absl::string_view topology_config_name,
    RedfishNodeTopologyRepresentation
        default_redfish_topology_reprensentation) {
  return CreateTopologyFromRedfishHelper(
      redfish_intf, default_redfish_topology_reprensentation,
      topology_config_name);
}

// Hash & equality functions for Node pointers. This is useful for using Node
// pointers in hash sets and maps so we can avoid having to copy the Node object
// itself into the data structure.
struct NodePtrHashFunc {
  size_t operator()(Node *a) const {
    if (a == nullptr) return absl::HashOf(a);
    return absl::HashOf(*a);
  }
};
struct NodePtrEquals {
  bool operator()(Node *a, Node *b) const {
    if (a == nullptr && b == nullptr) return true;
    if (a == nullptr || b == nullptr) return false;
    return *a == *b;
  }
};

bool NodeTopologiesHaveTheSameNodes(const NodeTopology &n1,
                                    const NodeTopology &n2) {
  // Short circuit: if the container sizes are mismatched then infer a delta.
  if (n1.nodes.size() != n2.nodes.size()) return false;

  // Check that all nodes exist in both topologies. This assumes that there are
  // no duplicate nodes that exist in either set of nodes.
  // This would be a failed precondition and imply that something is wrong with
  // the Redfish Assemblies.
  absl::flat_hash_set<Node *, NodePtrHashFunc, NodePtrEquals> n1_nodes;
  for (const std::unique_ptr<Node> &node : n1.nodes) {
    n1_nodes.insert(node.get());
  }
  // NOLINTNEXTLINE(readability-use-anyofallof)
  for (const std::unique_ptr<Node> &node : n2.nodes) {
    auto itr = n1_nodes.find(node.get());
    if (itr == n1_nodes.end()) return false;
  }
  return true;
}

}  // namespace ecclesia
