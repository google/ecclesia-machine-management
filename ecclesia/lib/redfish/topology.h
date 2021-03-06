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

#ifndef ECCLESIA_LIB_REDFISH_TOPOLOGY_H_
#define ECCLESIA_LIB_REDFISH_TOPOLOGY_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/types/optional.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/node_topology.h"
#include "ecclesia/lib/redfish/types.h"

namespace libredfish {

NodeTopology CreateTopologyFromRedfish(RedfishInterface *redfish_intf);

// Returns true if both provided NodeTopologies have the same nodes. Nodes are
// matched by their name, local_devpath, and type fields only. This does not
// detect changes in the internal maps to nodes.
bool NodeTopologiesHaveTheSameNodes(const NodeTopology &n1,
                                    const NodeTopology &n2);

}  // namespace libredfish

#endif  // ECCLESIA_LIB_REDFISH_TOPOLOGY_H_
