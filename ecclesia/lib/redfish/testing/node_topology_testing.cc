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

#include "ecclesia/lib/redfish/testing/node_topology_testing.h"

#include <memory>
#include <ostream>
#include <string>
#include <tuple>

#include "absl/strings/str_format.h"
#include "ecclesia/lib/redfish/node_topology.h"
#include "ecclesia/lib/redfish/types.h"

namespace libredfish {

bool operator==(const Node &n1, const Node &n2) {
  return std::tie(n1.name, n1.local_devpath, n1.type) ==
         std::tie(n2.name, n2.local_devpath, n2.type);
}

std::string ToString(NodeType type) {
  switch (type) {
    case NodeType::kBoard:
      return "Board";
    case NodeType::kConnector:
      return "Connector";
    case NodeType::kDevice:
      return "Device";
    case NodeType::kCable:
      return "Cable";
  }
}

void PrintTo(const Node &node, std::ostream *os) {
  *os << absl::StrFormat("\n{name: \"%s\" devpath: \"%s\" type: %s)", node.name,
                         node.local_devpath, ToString(node.type));
}

void PrintTo(const std::unique_ptr<Node> &node, std::ostream *os) {
  if (node) {
    PrintTo(*node, os);
  } else {
    *os << "\n(null)";
  }
}

}  // namespace libredfish
