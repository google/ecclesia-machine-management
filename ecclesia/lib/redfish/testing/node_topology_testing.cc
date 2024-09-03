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

#include "absl/strings/str_format.h"
#include "ecclesia/lib/redfish/node_topology.h"
#include "ecclesia/lib/redfish/types.h"

namespace ecclesia {

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
  std::string part_location_context;
  std::string service_label;
  if (node.supplemental_location_info.has_value()) {
    part_location_context =
        node.supplemental_location_info->part_location_context;
    service_label = node.supplemental_location_info->service_label;
  }
  *os << absl::StrFormat(
      "\n{name: \"%s\" model: \"%s\" devpath: \"%s\" type: %s replaceable: "
      "%u part_location_context: \"%s\" service_label: \"%s\")",
      node.name, node.model, node.local_devpath, ToString(node.type),
      node.replaceable, part_location_context, service_label);
}

void PrintTo(const std::unique_ptr<Node> &node, std::ostream *os) {
  if (node) {
    PrintTo(*node, os);
  } else {
    *os << "\n(null)";
  }
}

}  // namespace ecclesia
