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

#ifndef ECCLESIA_LIB_REDFISH_NODE_TOPOLOGY_H_
#define ECCLESIA_LIB_REDFISH_NODE_TOPOLOGY_H_

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "ecclesia/lib/redfish/types.h"

namespace ecclesia {

// Node represents a single devpath from the Redfish backend and its associated
// system data. There is a one-to-one mapping between Redfish Assembly
// Components and Nodes.
struct Node {
  // Name property value from the Redfish Component representing this Node.
  std::string name;
  // Model property value from the Redfish Component representing this Node.
  std::string model;
  // Local devpath assigned to this Node.
  std::string local_devpath;
  // Node type represents the physical type of this Node.
  NodeType type;
  // Associated URIs that provide logical system information to this Node.
  std::vector<std::string> associated_uris;
  // Whether the Node represents a part can be replaced.
  bool replaceable;

  bool operator==(const Node &o) const {
    return std::tie(name, model, local_devpath, type, associated_uris,
                    replaceable) == std::tie(o.name, o.model, o.local_devpath,
                                             o.type, o.associated_uris,
                                             o.replaceable);
  }
  bool operator!=(const Node &o) const { return !(*this == o); }
  // Support for absl::Hash.
  template <typename H>
  friend H AbslHashValue(H h, const Node &n) {
    return H::combine(std::move(h), n.name, n.model, n.local_devpath, n.type,
                      n.associated_uris, n.replaceable);
  }
};

// NodeTopology represents the collection of Nodes comprising a Redfish backend.
// Additional data structures are provided in order to conveniently look up
// specific nodes.
struct NodeTopology {
  // All nodes found from the redfish backend.
  std::vector<std::unique_ptr<Node>> nodes;

  // Map of a URI to all associated Nodes. If the URI is fetched, these Nodes
  // can have their properties updated from that information.
  absl::flat_hash_map<std::string, std::vector<Node *>>
      uri_to_associated_node_map;
  // Map of a domain devpath to a Node.
  absl::flat_hash_map<std::string, Node *> devpath_to_node_map;

  // Map of a Node to the list of Nodes it is attached to.
  absl::flat_hash_map<Node *, std::vector<Node *>> node_to_parents;
  // Map of a Node to its child nodes. This is just the inverse of the above.
  absl::flat_hash_map<Node *, std::vector<Node *>> node_to_children;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_NODE_TOPOLOGY_H_
