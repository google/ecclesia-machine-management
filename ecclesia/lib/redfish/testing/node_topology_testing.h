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

#include "ecclesia/lib/redfish/node_topology.h"
#include "ecclesia/lib/redfish/types.h"

namespace libredfish {

// Define equality of two nodes being only their name, devpath, type. Ignore
// the properties.
bool operator==(const Node &n1, const Node &n2);

// Some print functions defined to simplify debugging with GMock output.
std::string ToString(NodeType type);

void PrintTo(const Node &node, std::ostream *os);

void PrintTo(const std::unique_ptr<Node> &node, std::ostream *os);

}  // namespace libredfish

#endif  // ECCLESIA_LIB_REDFISH_NODE_TOPOLOGY_TESTING_H_
