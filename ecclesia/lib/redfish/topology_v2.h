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

#ifndef ECCLESIA_LIB_REDFISH_TOPOLOGY_V2_H_
#define ECCLESIA_LIB_REDFISH_TOPOLOGY_V2_H_

#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/node_topology.h"

namespace libredfish {

// Function to create NodeTopology based on go/redfish-devpath2 design
//
// This funciton will find a root node and uses the redfish linkages to find
// nodes and assign devpaths based on the Location.PartLocation attribute
NodeTopology CreateTopologyFromRedfishV2(RedfishInterface *redfish_intf);

}  // namespace libredfish

#endif  // ECCLESIA_LIB_REDFISH_TOPOLOGY_V2_H_
