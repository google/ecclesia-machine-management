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

// Common property definitions.
//
// PropertyDefinition subclasses are to be defined in this header to be used
// with typed extraction methods.
#ifndef ECCLESIA_LIB_REDFISH_PROPERTY_DEFINITIONS_H_
#define ECCLESIA_LIB_REDFISH_PROPERTY_DEFINITIONS_H_

#include <cstdint>
#include <string>

#include "ecclesia/lib/redfish/property.h"

namespace libredfish {

// String definitions of property fields to search programmatically. These
// are separate from the PropertyDefinitions for now as we cannot define the
// Redfish types "Array" and "Object" at the moment.

inline constexpr char kRfPropertyAssemblies[] = "Assemblies";
inline constexpr char kRfPropertyAssembly[] = "Assembly";
inline constexpr char kRfPropertyControllers[] = "Controllers";
inline constexpr char kRfPropertyChassis[] = "Chassis";
inline constexpr char kRfPropertyMemory[] = "Memory";
inline constexpr char kRfPropertyStorage[] = "Storage";
inline constexpr char kRfPropertyDrives[] = "Drives";
inline constexpr char kRfPropertyProtocolOem[] = "OEM";
inline constexpr char kRfPropertyOem[] = "Oem";
inline constexpr char kRfPropertyProcessors[] = "Processors";
inline constexpr char kRfPropertyMetrics[] = "Metrics";
inline constexpr char kRfPropertyEnvironmentMetrics[] = "EnvironmentMetrics";
inline constexpr char kRfPropertySystems[] = "Systems";
inline constexpr char kRfPropertyEthernetInterfaces[] = "EthernetInterfaces";
inline constexpr char kRfPropertyThermal[] = "Thermal";
inline constexpr char kRfPropertyTemperatures[] = "Temperatures";
inline constexpr char kRfPropertyTemperaturesCount[] =
    "Temperatures@odata.count";
inline constexpr char kRfPropertyPower[] = "Power";
inline constexpr char kRfPropertyPowerWatts[] = "PowerWatts";
inline constexpr char kRfPropertyVoltages[] = "Voltages";
inline constexpr char kRfPropertyFans[] = "Fans";
inline constexpr char kRfPropertySensors[] = "Sensors";
inline constexpr char kRfPropertyReadingUnits[] = "ReadingUnits";
inline constexpr char kRfPropertyRelatedItem[] = "RelatedItem";
inline constexpr char kRfPropertyStatus[] = "Status";
inline constexpr char kRfPropertyState[] = "State";
inline constexpr char kRfPropertyStorageControllers[] = "StorageControllers";
inline constexpr char kRfPropertyNvmeControllersProperties[] =
    "NVMeControllerProperties";
inline constexpr char kRfPropertyNvmeSmartCriticalWarnings[] =
    "NVMeSMARTCriticalWarnings";
inline constexpr char kRfPropertyLinks[] = "Links";
inline constexpr char kRfPropertyPcieDevice[] = "PCIeDevice";
inline constexpr char kRfPropertyPcieDevices[] = "PCIeDevices";
inline constexpr char kRfPropertyPcieInterface[] = "PCIeInterface";
inline constexpr char kRfPropertyPcieFunctions[] = "PCIeFunctions";
inline constexpr char kRfPropertyUpstreamPcieFunction[] =
    "UpstreamPCIeFunction";
inline constexpr char kRfPropertyDownstreamPcieFunctions[] =
    "DownstreamPCIeFunctions";
inline constexpr char kRfPropertyPciLocation[] = "PciLocation";
inline constexpr char kRfPropertyId[] = "Id";

inline constexpr char kRfOemPropertyAssociatedWith[] = "AssociatedWith";
inline constexpr char kRfOemPropertyAttachedTo[] = "AttachedTo";
inline constexpr char kRfOemPropertyGoogle[] = "Google";
inline constexpr char kRfOemPropertyComponents[] = "Components";
inline constexpr char kRfOemPropertyMemoryErrorCounts[] = "MemoryErrorCounts";
inline constexpr char kRfOemPropertyProcessorErrorCounts[] =
    "ProcessorErrorCounts";
inline constexpr char kRfPropertyProcessorId[] = "ProcessorId";
inline constexpr char kRfOemPropertySmartAttributes[] = "SMARTAttributes";
inline constexpr char kRfOemPropertyBootNumber[] = "BootNumber";
inline constexpr char kRfOemPropertySystemUptime[] = "SystemUptime";

inline constexpr char kRfPropertyMediaTypeSsd[] = "SSD";
inline constexpr char kRfPropertyMemberId[] = "MemberId";
inline constexpr char kRfPropertyReading[] = "Reading";
inline constexpr char kRfPropertyHealth[] = "Health";

DEFINE_REDFISH_RESOURCE(ResourceSystem, "ComputerSystem");
DEFINE_REDFISH_RESOURCE(ResourceChassis, "Chassis");
DEFINE_REDFISH_RESOURCE(ResourceMemory, "Memory");
DEFINE_REDFISH_RESOURCE(ResourceStorage, "Storage");
DEFINE_REDFISH_RESOURCE(ResourceDrive, "Drive");
DEFINE_REDFISH_RESOURCE(ResourceProcessor, "Processor");
DEFINE_REDFISH_RESOURCE(ResourceEthernetInterface, "EthernetInterface");
DEFINE_REDFISH_RESOURCE(ResourceTemperature, "Temperature");
DEFINE_REDFISH_RESOURCE(ResourceVoltage, "Voltage");
DEFINE_REDFISH_RESOURCE(ResourceFan, "Fan");
DEFINE_REDFISH_RESOURCE(ResourceSensor, "Sensor");
DEFINE_REDFISH_RESOURCE(ResourcePcieFunction, "PCIeFunction");
DEFINE_REDFISH_RESOURCE(ResourceComputerSystem, "ComputerSystem");
// The AssemblyEntry is a single item in the "Assemblies" list of
// Assembly resource
DEFINE_REDFISH_RESOURCE(ResourceAssemblyEntry, "/Assembly#/");

DEFINE_REDFISH_PROPERTY(PropertyOdataId, std::string, "@odata.id");
DEFINE_REDFISH_PROPERTY(PropertyOdataType, std::string, "@odata.type");
DEFINE_REDFISH_PROPERTY(PropertyMembers, std::string, "Members");
DEFINE_REDFISH_PROPERTY(PropertyMembersCount, std::string,
                        "Members@odata.count");
DEFINE_REDFISH_PROPERTY(PropertyCapacityMiB, int, "CapacityMiB");
DEFINE_REDFISH_PROPERTY(PropertyLogicalSizeMiB, int, "LogicalSizeMiB");
DEFINE_REDFISH_PROPERTY(PropertyManufacturer, std::string, "Manufacturer");
DEFINE_REDFISH_PROPERTY(PropertyVendor, std::string, "Vendor");
DEFINE_REDFISH_PROPERTY(PropertyMemoryDeviceType, std::string,
                        "MemoryDeviceType");
DEFINE_REDFISH_PROPERTY(PropertyName, std::string, "Name");
DEFINE_REDFISH_PROPERTY(PropertyOperatingSpeedMhz, int, "OperatingSpeedMhz");
DEFINE_REDFISH_PROPERTY(PropertyPartNumber, std::string, "PartNumber");
DEFINE_REDFISH_PROPERTY(PropertyPhysicalContext, std::string,
                        "PhysicalContext");
DEFINE_REDFISH_PROPERTY(PropertySerialNumber, std::string, "SerialNumber");
DEFINE_REDFISH_PROPERTY(PropertySocket, std::string, "Socket");
DEFINE_REDFISH_PROPERTY(PropertyTotalCores, int, "TotalCores");
DEFINE_REDFISH_PROPERTY(PropertyTotalEnabledCores, int, "TotalEnabledCores");
DEFINE_REDFISH_PROPERTY(PropertyTotalThreads, int, "TotalThreads");
DEFINE_REDFISH_PROPERTY(PropertyMaxSpeedMhz, int, "MaxSpeedMHz");
DEFINE_REDFISH_PROPERTY(PropertyLinkStatus, std::string, "LinkStatus");
DEFINE_REDFISH_PROPERTY(PropertySpeedMbps, int, "SpeedMbps");
DEFINE_REDFISH_PROPERTY(PropertyMacAddress, std::string, "MACAddress");
DEFINE_REDFISH_PROPERTY(PropertyCapacityBytes, int64_t, "CapacityBytes");
DEFINE_REDFISH_PROPERTY(PropertySpareCapacityWornOut, bool,
                        "SpareCapacityWornOut");
DEFINE_REDFISH_PROPERTY(PropertyOverallSubsystemDegraded, bool,
                        "OverallSubsystemDegraded");
DEFINE_REDFISH_PROPERTY(PropertyMediaInReadOnly, bool, "MediaInReadOnly");
DEFINE_REDFISH_PROPERTY(PropertyBlockSizeBytes, int, "BlockSizeBytes");
DEFINE_REDFISH_PROPERTY(PropertyProtocol, std::string, "Protocol");
DEFINE_REDFISH_PROPERTY(PropertyMediaType, std::string, "MediaType");
DEFINE_REDFISH_PROPERTY(PropertyReadingCelsius, double, "ReadingCelsius");
DEFINE_REDFISH_PROPERTY(PropertyThrottlingCelsius, int, "ThrottlingCelsius");
DEFINE_REDFISH_PROPERTY(PropertyReading, double, "Reading");
DEFINE_REDFISH_PROPERTY(PropertyReadingType, std::string, "ReadingType");
DEFINE_REDFISH_PROPERTY(PropertyReadingUnits, std::string, "ReadingUnits");
DEFINE_REDFISH_PROPERTY(PropertyUpperThresholdCritical, int,
                        "UpperThresholdCritical");
DEFINE_REDFISH_PROPERTY(PropertyUpperThresholdNonCritical, int,
                        "UpperThresholdNonCritical");
DEFINE_REDFISH_PROPERTY(PropertyLowerThresholdCritical, int,
                        "LowerThresholdCritical");
DEFINE_REDFISH_PROPERTY(PropertyLowerThresholdNonCritical, int,
                        "LowerThresholdNonCritical");
DEFINE_REDFISH_PROPERTY(PropertyState, std::string, "State");
DEFINE_REDFISH_PROPERTY(PropertyVendorId, std::string, "VendorId");
DEFINE_REDFISH_PROPERTY(PropertyDeviceId, std::string, "DeviceId");
DEFINE_REDFISH_PROPERTY(PropertySubsystemId, std::string, "SubsystemId");
DEFINE_REDFISH_PROPERTY(PropertySubsystemVendorId, std::string,
                        "SubsystemVendorId");
DEFINE_REDFISH_PROPERTY(PropertyPcieType, std::string, "PCIeType");
DEFINE_REDFISH_PROPERTY(PropertyMaxPcieType, std::string, "MaxPCIeType");
DEFINE_REDFISH_PROPERTY(PropertyLanesInUse, int, "LanesInUse");
DEFINE_REDFISH_PROPERTY(PropertyMaxLanes, int, "MaxLanes");
DEFINE_REDFISH_PROPERTY(PropertyProcessorIdStep, std::string, "Step");
DEFINE_REDFISH_PROPERTY(PropertyDataSourceUri, std::string, "DataSourceUri");

// OEM Google properties
DEFINE_REDFISH_PROPERTY(OemGooglePropertyCorrectable, int, "Correctable");
DEFINE_REDFISH_PROPERTY(OemGooglePropertyUncorrectable, int, "Uncorrectable");
DEFINE_REDFISH_PROPERTY(OemGooglePropertyDomain, std::string, "Domain");
DEFINE_REDFISH_PROPERTY(OemGooglePropertyBus, std::string, "Bus");
DEFINE_REDFISH_PROPERTY(OemGooglePropertyDevice, std::string, "Device");
DEFINE_REDFISH_PROPERTY(OemGooglePropertyFunction, std::string, "Function");
DEFINE_REDFISH_PROPERTY(OemGooglePropertyAvailableSpare, int,
                        "AvailableSparePercent");
DEFINE_REDFISH_PROPERTY(OemGooglePropertyAvailableSpareThreshold, int,
                        "AvailableSparePercentThreshold");
DEFINE_REDFISH_PROPERTY(OemGooglePropertyPercentageUsed, int, "PercentageUsed");
DEFINE_REDFISH_PROPERTY(OemGooglePropertyCompositeTemperatureKelvins, int,
                        "CompositeTemperatureKelvins");
DEFINE_REDFISH_PROPERTY(OemGooglePropertyCriticalTemperatureTimeMinute, int,
                        "CriticalTemperatureTimeMinute");
DEFINE_REDFISH_PROPERTY(OemGooglePropertyCriticalWarning, int,
                        "CriticalWarning");
}  // namespace libredfish

#endif  // ECCLESIA_LIB_REDFISH_PROPERTY_DEFINITIONS_H_
