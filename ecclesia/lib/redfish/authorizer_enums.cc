/*
 * Copyright 2022 Google LLC
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

#include "ecclesia/lib/redfish/authorizer_enums.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <string_view>

namespace ecclesia {

namespace {
constexpr std::array<std::string_view,
                     static_cast<size_t>(ResourceEntity::kUndefined) + 1>
    kEntityNames = {
        // go/keep-sorted start
        "AccelerationFunction",
        "AccelerationFunctionCollection",
        "AccountService",
        "ActionInfo",
        "AddressPool",
        "AddressPoolCollection",
        "Aggregate",
        "AggregateCollection",
        "AggregationService",
        "AggregationSource",
        "AggregationSourceCollection",
        "AllowDeny",
        "AllowDenyCollection",
        "Assembly",
        "Battery",
        "BatteryCollection",
        "BatteryMetrics",
        "Bios",
        "BootOption",
        "BootOptionCollection",
        "CXLLogicalDevice",
        "CXLLogicalDeviceCollection",
        "Cable",
        "CableCollection",
        "Certificate",
        "CertificateCollection",
        "CertificateLocations",
        "CertificateService",
        "Chassis",
        "ChassisCollection",
        "Circuit",
        "CircuitCollection",
        "ComponentIntegrity",
        "ComponentIntegrityCollection",
        "CompositionReservation",
        "CompositionReservationCollection",
        "CompositionService",
        "ComputerSystem",
        "ComputerSystemCollection",
        "Connection",
        "ConnectionCollection",
        "ConnectionMethod",
        "ConnectionMethodCollection",
        "Control",
        "ControlCollection",
        "Drive",
        "DriveCollection",
        "Endpoint",
        "EndpointCollection",
        "EndpointGroup",
        "EndpointGroupCollection",
        "EnvironmentMetrics",
        "EthernetInterface",
        "EthernetInterfaceCollection",
        "EventDestination",
        "EventDestinationCollection",
        "EventService",
        "ExternalAccountProvider",
        "ExternalAccountProviderCollection",
        "Fabric",
        "FabricAdapter",
        "FabricAdapterCollection",
        "FabricCollection",
        "Facility",
        "FacilityCollection",
        "Fan",
        "FanCollection",
        "GraphicsController",
        "GraphicsControllerCollection",
        "Heater",
        "HeaterCollection",
        "HeaterMetrics",
        "HostInterface",
        "HostInterfaceCollection",
        "Job",
        "JobCollection",
        "JobService",
        "JsonSchemaFile",
        "JsonSchemaFileCollection",
        "Key",
        "KeyCollection",
        "KeyPolicy",
        "KeyPolicyCollection",
        "KeyService",
        "LogEntry",
        "LogEntryCollection",
        "LogService",
        "LogServiceCollection",
        "Manager",
        "ManagerAccount",
        "ManagerAccountCollection",
        "ManagerCollection",
        "ManagerDiagnosticData",
        "ManagerNetworkProtocol",
        "MediaController",
        "MediaControllerCollection",
        "Memory",
        "MemoryChunks",
        "MemoryChunksCollection",
        "MemoryCollection",
        "MemoryDomain",
        "MemoryDomainCollection",
        "MemoryMetrics",
        "MessageRegistryFile",
        "MessageRegistryFileCollection",
        "MetricDefinition",
        "MetricDefinitionCollection",
        "MetricReport",
        "MetricReportCollection",
        "MetricReportDefinition",
        "MetricReportDefinitionCollection",
        "NVMe",
        "NVMeCollection",
        "NVMeController",
        "NVMeControllerCollection",
        "NetworkAdapter",
        "NetworkAdapterCollection",
        "NetworkAdapterMetrics",
        "NetworkDeviceFunction",
        "NetworkDeviceFunctionCollection",
        "NetworkDeviceFunctionMetrics",
        "NetworkInterface",
        "NetworkInterfaceCollection",
        "NetworkPort",
        "NetworkPortCollection",
        "OperatingConfig",
        "OperatingConfigCollection",
        "Outlet",
        "OutletCollection",
        "OutletGroup",
        "OutletGroupCollection",
        "PCIeDevice",
        "PCIeDeviceCollection",
        "PCIeFunction",
        "PCIeFunctionCollection",
        "PCIeSlots",
        "Port",
        "PortCollection",
        "PortMetrics",
        "Power",
        "PowerDistribution",
        "PowerDistributionCollection",
        "PowerDistributionMetrics",
        "PowerDomain",
        "PowerDomainCollection",
        "PowerEquipment",
        "PowerSubsystem",
        "PowerSupply",
        "PowerSupplyCollection",
        "PowerSupplyMetrics",
        "PrivilegeRegistry",
        "Processor",
        "ProcessorCollection",
        "ProcessorMetrics",
        "ResourceBlock",
        "ResourceBlockCollection",
        "Role",
        "RoleCollection",
        "RootOfTrust",
        "RootOfTrustCollection",
        "RouteEntry",
        "RouteEntryCollection",
        "RouteSetEntry",
        "RouteSetEntryCollection",
        "SecureBoot",
        "SecureBootDatabase",
        "SecureBootDatabaseCollection",
        "Sensor",
        "SensorCollection",
        "SerialInterface",
        "SerialInterfaceCollection",
        "ServiceRoot",
        "Session",
        "SessionCollection",
        "SessionService",
        "Signature",
        "SignatureCollection",
        "SimpleStorage",
        "SimpleStorageCollection",
        "SoftwareInventory",
        "SoftwareInventoryCollection",
        "Storage",
        "StorageCollection",
        "StorageController",
        "StorageControllerCollection",
        "Switch",
        "SwitchCollection",
        "Task",
        "TaskCollection",
        "TaskService",
        "TelemetryService",
        "Thermal",
        "ThermalMetrics",
        "ThermalSubsystem",
        "Triggers",
        "TriggersCollection",
        "USBController",
        "USBControllerCollection",
        "UpdateService",
        "VCATEntry",
        "VCATEntryCollection",
        "VLanNetworkInterface",
        "VLanNetworkInterfaceCollection",
        "VirtualMedia",
        "VirtualMediaCollection",
        "Volume",
        "VolumeCollection",
        "Zone",
        "ZoneCollection",
        // go/keep-sorted end
        "Undefined",
};

constexpr std::array<std::string_view, 7> kOperationNames = {
    "DELETE", "GET", "HEAD", "PATCH", "POST", "PUT", "Undefined"};

}  // namespace

std::string ResourceEntityToString(ResourceEntity entity) {
  static_assert(
      static_cast<int>(ResourceEntity::kUndefined) + 1 == kEntityNames.size(),
      "kEntityNames and ResourceEntity mismatch");
  return std::string(kEntityNames[static_cast<int>(entity)]);
}

std::string OperationToString(Operation operation) {
  static_assert(
      static_cast<int>(Operation::kUndefined) + 1 == kOperationNames.size(),
      "kOperationNames and Operation mismatch");
  return std::string(kOperationNames[static_cast<int>(operation)]);
}

ResourceEntity StringToResourceEntity(std::string_view resource) {
  auto it = std::lower_bound(kEntityNames.cbegin(),
                             std::prev(kEntityNames.cend()), resource);
  if (it == std::prev(kEntityNames.cend()) || *it != resource) {
    return ResourceEntity::kUndefined;
  }
  return static_cast<ResourceEntity>(std::distance(kEntityNames.cbegin(), it));
}

Operation StringToOperation(std::string_view operation) {
  if (operation == "DELETE") {
    return Operation::kDelete;
  }
  if (operation == "GET") {
    return Operation::kGet;
  }
  if (operation == "HEAD") {
    return Operation::kHead;
  }
  if (operation == "PATCH") {
    return Operation::kPatch;
  }
  if (operation == "POST") {
    return Operation::kPost;
  }
  if (operation == "PUT") {
    return Operation::kPut;
  }
  return Operation::kUndefined;
}

}  // namespace ecclesia
