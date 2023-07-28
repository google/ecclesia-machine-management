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

#ifndef ECCLESIA_LIB_REDFISH_DEVPATH_H_
#define ECCLESIA_LIB_REDFISH_DEVPATH_H_

#include <optional>
#include <string>

#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/node_topology.h"

namespace ecclesia {

// Function to find devpath for an arbitrary URI by searching through
// NodeTopology
std::optional<std::string> GetDevpathForUri(const NodeTopology &topology,
                                            absl::string_view uri);

// Function to find a main uri which is the RedfishObj's uri that creates the
// target devpath by searching through NodeTopology
absl::StatusOr<std::string> GetFirstUriForDevpath(const NodeTopology &topology,
                                                  absl::string_view devpath);

// Attempts to get a devpath for a Redfish object, handling special cases like
// Manager and Sensor resources that rely on different properties to resolve a
// devpath.
std::optional<std::string> GetDevpathForObjectAndNodeTopology(
    const RedfishObject &obj, const NodeTopology &topology);

// Function to find devpath for Sensor resources (Sensor/Power/Thermal) based on
// RelatedItems
//
// This function will use the provided NodeTopology to lookup the "RelatedItem"
// provided by the RedfishObject. If none exists then the function will try and
// find a devpath for the Sensor object itself in the topology. As a final
// fallback, the function will try and find the containing Chassis for these
// resources and provide that devpath.
//
// The RedfishObject passed to this function must be under a subURI of a Chassis
// in order to produce a valid depvath i.e. /redfish/v1/Chassis/<chassis-id>/...
std::optional<std::string> GetSensorDevpathFromNodeTopology(
    const RedfishObject &obj, const NodeTopology &topology);

std::optional<std::string> GetSensorDevpathFromNodeTopology(
    RedfishObject *obj, const NodeTopology &topology);

std::optional<std::string> GetManagerDevpathFromNodeTopology(
    const RedfishObject &obj, const NodeTopology &topology);

std::optional<std::string> GetSlotDevpathFromNodeTopology(
    const RedfishObject &obj, absl::string_view parent_uri,
    const NodeTopology &topology);

std::optional<std::string> GetDevpathFromSlotDevpath(
    absl::string_view slot_devpath);

// Find the replaceable devpath for a devpath inside a Node Topology. If the
// devpath itself is replaceable it'll return itself, otherwise it'll return the
// first parent that is replaceable.
absl::StatusOr<absl::string_view>
GetReplaceableDevpathForDevpathAndNodeTopology(absl::string_view devpath,
                                               const NodeTopology &topology);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_DEVPATH_H_
