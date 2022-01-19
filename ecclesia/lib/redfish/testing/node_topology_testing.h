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

// Library for aiding in testing topology library

#ifndef ECCLESIA_LIB_REDFISH_NODE_TOPOLOGY_TESTING_H_
#define ECCLESIA_LIB_REDFISH_NODE_TOPOLOGY_TESTING_H_

#include <memory>
#include <ostream>
#include <string>
#include <tuple>

#include "gmock/gmock.h"
#include "ecclesia/lib/redfish/node_topology.h"
#include "ecclesia/lib/redfish/types.h"

namespace ecclesia {

// Matcher that compares a Node against a name, devpath and type.
MATCHER_P3(RedfishNodeIdIs, name, devpath, type, "") {
  return std::tie(arg.name, arg.local_devpath, arg.type) ==
         std::tie(name, devpath, type);
}

// Matcher that compares a tuple of two nodes using only the node name, devpath
// and type (i.e. ignoring associated URIs). Intended for use in combination
// with Pointwise or UnorderedPointwise to compare containers of nodes.
MATCHER(RedfishNodeEqId, "") {
  const Node &lhs = std::get<0>(arg);
  const Node &rhs = std::get<1>(arg);
  return std::tie(lhs.name, lhs.local_devpath, lhs.type) ==
         std::tie(rhs.name, rhs.local_devpath, rhs.type);
}

// Some print functions defined to simplify debugging with GMock output.
std::string ToString(NodeType type);

void PrintTo(const Node &node, std::ostream *os);

void PrintTo(const std::unique_ptr<Node> &node, std::ostream *os);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_NODE_TOPOLOGY_TESTING_H_
