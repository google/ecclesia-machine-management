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

#ifndef ECCLESIA_MAGENT_SYSMODEL_X86_CHASSIS_H_
#define ECCLESIA_MAGENT_SYSMODEL_X86_CHASSIS_H_

#include <optional>
#include <string>
#include <vector>

#include "ecclesia/lib/io/usb/usb.h"

namespace ecclesia {

// The supported Chassis IDs. This enum will be expanded if more chassis are
// going to be supported.
enum class ChassisId {
  kIndus,
  kSleipnir,
  kUnknown,
};

inline std::string ChassisIdToString(ChassisId chassis_id) {
  switch (chassis_id) {
    case ChassisId::kIndus:
      return "Indus";
    case ChassisId::kSleipnir:
      return "Sleipnir";
    default:
      return "Unknown";
  }
}

// These Chassis types come from the Redfish ChassisType. This enum is
// incomplete, will add more if needed.
enum class ChassisType {
  kExpansion,
  kRackMount,
  kSidecar,
  kStorageEnclosure,
  kUnknown,
};

inline std::string ChassisTypeToString(ChassisType chassis_type) {
  switch (chassis_type) {
    case ChassisType::kExpansion:
      return "Expansion";
    case ChassisType::kRackMount:
      return "RackMount";
    case ChassisType::kSidecar:
      return "Sidecar";
    case ChassisType::kStorageEnclosure:
      return "StorageEnclosure";
    default:
      return "Unknown";
  }
}

inline ChassisType GetChassisType(ChassisId chassis_id) {
  switch (chassis_id) {
    case ChassisId::kIndus:
      return ChassisType::kRackMount;
    case ChassisId::kSleipnir:
      return ChassisType::kStorageEnclosure;
    default:
      return ChassisType::kUnknown;
  }
}

inline std::string GetChassisTypeAsString(ChassisId chassis_id) {
  return ChassisTypeToString(GetChassisType(chassis_id));
}

// This method detects and returns USB-based Chassis (expansion tray) if any
// matched Chassis is found or nullopt if none was found.
std::optional<ChassisId> DetectChassisByUsb(
    UsbDiscoveryInterface *usb_discover_intf);

// This function returns a vector of Chassis IDs in the system. Those Redfish
// Chassis resources and their "always-present" assemblies will be created
// based on the existence of the corresponding chassis ID.
std::vector<ChassisId> CreateChassis(UsbDiscoveryInterface *usb_discover_intf);

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_SYSMODEL_X86_CHASSIS_H_
