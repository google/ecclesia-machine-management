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
#include <vector>

#include "absl/time/time.h"
#include "ecclesia/lib/redfish/property.h"

namespace ecclesia {

// String definitions of property fields to search programmatically. These
// are separate from the PropertyDefinitions for now as we cannot define the
// Redfish types "Array" and "Object" at the moment.
inline constexpr char kRfPropertyRedfish[] = "redfish";
inline constexpr char kRfPropertyV1[] = "v1";
inline constexpr char kRfPropertyActions[] = "Actions";
inline constexpr char kRfPropertyAllowableValues[] = "AllowableValues";
inline constexpr char kRfPropertyAssemblies[] = "Assemblies";
inline constexpr char kRfPropertyAssembly[] = "Assembly";
inline constexpr char kRfPropertyAssociatedMACAddresses[] =
    "AssociatedMACAddresses";
inline constexpr char kRfPropertyControllers[] = "Controllers";
inline constexpr char kRfPropertyChassis[] = "Chassis";
inline constexpr char kRfPropertyPcieSlots[] = "PCIeSlots";
inline constexpr char kRfPropertyChassisReset[] = "#Chassis.Reset";
inline constexpr char kRfPropertyComponentIntegrity[] = "ComponentIntegrity";
inline constexpr char kRfPropertyComponentIntegritySPDMGetSignedMeasurements[] =
    "#ComponentIntegrity.SPDMGetSignedMeasurements";
inline constexpr char kRfPropertyMemory[] = "Memory";
inline constexpr char kRfPropertyStorage[] = "Storage";
inline constexpr char kRfPropertyDrives[] = "Drives";
inline constexpr char kRfPropertyDriveReset[] = "#Drive.Reset";
inline constexpr char kRfPropertyResetType[] = "ResetType";
inline constexpr char kRfPropertyControlMode[] = "ControlMode";
inline constexpr char kRfPropertyProtocolOem[] = "OEM";
inline constexpr char kRfPropertyOem[] = "Oem";
inline constexpr char kRfPropertyLifeTime[] = "LifeTime";
inline constexpr char kRfPropertyCurrentPeriod[] = "CurrentPeriod";
inline constexpr char kRfPropertyCurrentSpeedGbps[] = "CurrentSpeedGbps";
inline constexpr char kRfPropertyCacheMetricsTotal[] = "CacheMetricsTotal";
inline constexpr char kRfPropertyProcessors[] = "Processors";
inline constexpr char kRfPropertySubProcessors[] = "SubProcessors";
inline constexpr char kRfPropertyMetrics[] = "Metrics";
inline constexpr char kRfPropertyEnvironmentMetrics[] = "EnvironmentMetrics";
inline constexpr char kRfPropertyMemoryMetrics[] = "MemoryMetrics";
inline constexpr char kRfPropertySessions[] = "Sessions";
inline constexpr char kRfPropertySessionService[] = "SessionService";
inline constexpr char kRfPropertySystems[] = "Systems";
inline constexpr char kRfPropertySystemReset[] = "#ComputerSystem.Reset";
inline constexpr char kRfPropertyManagers[] = "Managers";
inline constexpr char kRfPropertyManagerReset[] = "#Manager.Reset";
inline constexpr char kRfPropertyManagerInChassis[] = "ManagerInChassis";
inline constexpr char kRfPropertyManagerDiagnosticData[] =
    "ManagerDiagnosticData";
inline constexpr char kRfPropertyDedicatedNetworkPorts[] =
    "DedicatedNetworkPorts";
inline constexpr char kRfPropertyNetworking[] = "Networking";
inline constexpr char kRfPropertyBootInfo[] = "BootInfo";
inline constexpr char kRfPropertyEthernetInterfaces[] = "EthernetInterfaces";
inline constexpr char kRfPropertyEthernet[] = "Ethernet";
inline constexpr char kRfPropertyThermal[] = "Thermal";
inline constexpr char kRfPropertyTemperatures[] = "Temperatures";
inline constexpr char kRfPropertyTemperatureCelsius[] = "TemperatureCelsius";
inline constexpr char kRfPropertyTemperaturesCount[] =
    "Temperatures@odata.count";
inline constexpr char kRfPropertyParameters[] = "Parameters";
inline constexpr char kRfPropertyPorts[] = "Ports";
inline constexpr char kRfPropertyPower[] = "Power";
inline constexpr char kRfPropertyPowerLimitWatts[] = "PowerLimitWatts";
inline constexpr char kRfPropertyPowerState[] = "PowerState";
inline constexpr char kRfPropertyPowerWatts[] = "PowerWatts";
inline constexpr char kRfPropertyProcessorStatistics[] = "ProcessorStatistics";
inline constexpr char kRfPropertyTopProcesses[] = "TopProcesses";
inline constexpr char kRfPropertyBootTimeStatistics[] = "BootTimeStatistics";
inline constexpr char kRfPropertyVoltages[] = "Voltages";
inline constexpr char kRfPropertyFans[] = "Fans";
inline constexpr char kRfPropertySensors[] = "Sensors";
inline constexpr char kRfPropertyReadingUnits[] = "ReadingUnits";
inline constexpr char kRfPropertyRelatedItem[] = "RelatedItem";
inline constexpr char kRfPropertyStatus[] = "Status";
inline constexpr char kRfPropertyState[] = "State";
inline constexpr char kRfPropertyThresholds[] = "Thresholds";
inline constexpr char kRfPropertyLowerCaution[] = "LowerCaution";
inline constexpr char kRfPropertyLowerCritical[] = "LowerCritical";
inline constexpr char kRfPropertyUpperCaution[] = "UpperCaution";
inline constexpr char kRfPropertyUpperCritical[] = "UpperCritical";
inline constexpr char kRfPropertyStorageControllers[] = "StorageControllers";
inline constexpr char kRfPropertyMemoryStatistics[] = "MemoryStatistics";
inline constexpr char kRfPropertyNvmeControllersProperties[] =
    "NVMeControllerProperties";
inline constexpr char kRfPropertyNvmeSmartCriticalWarnings[] =
    "NVMeSMARTCriticalWarnings";
inline constexpr char kRfPropertySATAControllersProperties[] =
    "SATAControllerProperties";
inline constexpr char kRfPropertyLinks[] = "Links";
inline constexpr char kRfPropertyLinkState[] = "LinkState";
inline constexpr char kRfPropertyLinkStatus[] = "LinkStatus";
inline constexpr char kRfPropertyComputerSystems[] = "ComputerSystems";
inline constexpr char kRfPropertyContainedBy[] = "ContainedBy";
inline constexpr char kRfPropertyContains[] = "Contains";
inline constexpr char kRfPropertyManagedBy[] = "ManagedBy";
inline constexpr char kRfPropertyDownstreamChassis[] = "DownstreamChassis";
inline constexpr char kRfPropertyUpstreamChassis[] = "UpstreamChassis";
inline constexpr char kRfPropertyOriginOfCondition[] = "OriginOfCondition";
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
inline constexpr char kRfPropertyLogServices[] = "LogServices";
inline constexpr char kRfPropertyRelatedLogEntries[] = "RelatedLogEntries";
inline constexpr char kRfPropertyEntries[] = "Entries";
inline constexpr char kRfPropertyClearLog[] = "ClearLog";
inline constexpr char kRfPropertyMembers[] = "Members";
inline constexpr char kRfPropertyLocation[] = "Location";
inline constexpr char kRfPropertyPhysicalLocation[] = "PhysicalLocation";
inline constexpr char kRfPropertyPartLocation[] = "PartLocation";
inline constexpr char kRfPropertyCables[] = "Cables";
inline constexpr char kRfPropertyCableUpstreamResources[] = "UpstreamResources";
inline constexpr char kRfPropertySlots[] = "Slots";
inline constexpr char kRfPropertyMemorySummary[] = "MemorySummary";
inline constexpr char kRfPropertySettingsObject[] = "SettingsObject";
inline constexpr char kRfPropertyVolumes[] = "Volumes";
inline constexpr char kRfPropertyNVMeNamespaceProperties[] =
    "NVMeNamespaceProperties";

inline constexpr char kRfOemPropertyAssociatedWith[] = "AssociatedWith";
inline constexpr char kRfOemPropertyAttachedTo[] = "AttachedTo";
inline constexpr char kRfOemPropertyGoogle[] = "Google";
inline constexpr char kRfOemPropertyGoogleComputerSystemBootGuestOs[] =
    "#GoogleComputerSystem.BootGuestOS";
inline constexpr char kRfOemPropertyComponents[] = "Components";
inline constexpr char kRfOemPropertyMemoryErrorCounts[] = "MemoryErrorCounts";
inline constexpr char kRfOemPropertyProcessorErrorCounts[] =
    "ProcessorErrorCounts";
inline constexpr char kRfPropertyProcessorId[] = "ProcessorId";
inline constexpr char kRfOemPropertySmartAttributes[] = "SMARTAttributes";
inline constexpr char kRfOemPropertyBootNumber[] = "BootNumber";
inline constexpr char kRfOemPropertySystemUptime[] = "SystemUptime";
inline constexpr char kRfOemPropertyTelemetry[] = "Telemetry";
inline constexpr char kRfOemPropertyNvmeTelemetry[] = "NvmeTelemetry";
inline constexpr char kRfOemPropertyTcgOpalSecurityState[] =
    "TcgOpalSecurityState";
inline constexpr char kRfOemPropertyBareMetalStatus[] = "BareMetalStatus";
inline constexpr char kRfOemPropertyNerfStatus[] = "NERFStatus";
inline constexpr char kRfOemPropertyNvme[] = "NVMe";
inline constexpr char kRfOemPropertyKnuckleController[] = "KnuckleController";
inline constexpr char kRfOemPropertyNvmeControllerGetLogPage[] =
    "#NVMeController.GetLogPage";
inline constexpr char kRfOemPropertyNvmeControllerIdentify[] =
    "#NVMeController.Identify";
inline constexpr char kRfOemPropertyNvmeControllerKnuckleIdentify[] =
    "#NVMeController.KnuckleIdentify";
inline constexpr char kRfOemPropertyKnuckleLinkStatus[] = "KnuckleLinkStatus";
inline constexpr char kRfOemPropertyPf0[] = "PF0";
inline constexpr char kRfOemPropertyPf1[] = "PF1";
inline constexpr char kRfOemPropertyLocationContext[] = "LocationContext";
inline constexpr char kRfOemPropertyLatencyHistogram[] = "LatencyHistogram";

inline constexpr char kRfPropertyMediaTypeSsd[] = "SSD";
inline constexpr char kRfPropertyMemberId[] = "MemberId";
inline constexpr char kRfPropertyReading[] = "Reading";
inline constexpr char kRfPropertyHealth[] = "Health";
inline constexpr char kRfPropertyResponseError[] = "error";
inline constexpr char kRfPropertyResponseCode[] = "code";
inline constexpr char kRfPropertyResponseMessage[] = "message";

inline constexpr char kRfPropertyUpdateService[] = "UpdateService";
inline constexpr char kRfPropertyFirmwareInventory[] = "FirmwareInventory";
inline constexpr char kRfPropertyFirmwareVersion[] = "FirmwareVersion";
inline constexpr char kRfPropertyAdditionalDataUri[] = "AdditionalDataURI";
inline constexpr char kRfPropertyAbsent[] = "Absent";

inline constexpr char kRfPropertyCertificates[] = "Certificates";
inline constexpr char kRfPropertySPDM[] = "SPDM";
inline constexpr char kRfPropertyIdentityAuthentication[] =
    "IdentityAuthentication";
inline constexpr char kRfPropertyResponderAuthentication[] =
    "ResponderAuthentication";
inline constexpr char kRfPropertyComponentCertificate[] =
    "ComponentCertificate";
inline constexpr char kRfPropertyRootOfTrustCollection[] =
    "RootOfTrustCollection";
inline constexpr char kRfPropertyRootOfTrustSendCommand[] =
    "#RootOfTrust.SendCommand";
inline constexpr char kRfPropertySwitches[] = "Switches";
inline constexpr char kRfPropertyEnabled[] = "Enabled";
inline constexpr char kRfPropertyDisabled[] = "Disabled";
inline constexpr char kRfPropertyFabrics[] = "Fabrics";
inline constexpr char kRfPropertyConditions[] = "Conditions";
inline constexpr char kRfPropertyMessageArgs[] = "MessageArgs";

inline constexpr char kRfResourceTypeProcessorMetrics[] = "ProcessorMetrics";

inline constexpr char kProtocolFeaturesSupported[] =
    "ProtocolFeaturesSupported";
inline constexpr char kExpandQuery[] = "ExpandQuery";
inline constexpr char kTopSkipQuery[] = "TopSkipQuery";
inline constexpr char kFilterQuery[] = "FilterQuery";

inline constexpr char kRfPropertyTaskPayload[] = "Payload";
inline constexpr char kRfPropertyTaskHttpHeaders[] = "HttpHeaders";

inline constexpr char kRfAnnotationRedfishSettings[] = "@Redfish.Settings";

inline constexpr char kRfPropertyTelemetryService[] = "TelemetryService";
inline constexpr char kRfPropertyMetricReport[] = "MetricReport";
inline constexpr char kRfPropertyMetricReportDefinitions[] =
    "MetricReportDefinitions";
inline constexpr char kRfPropertyMetricValues[] = "MetricValues";
inline constexpr char kRfPropertyMetricProperty[] = "MetricProperty";
inline constexpr char kRfPropertyEndpoints[] = "Endpoints";
inline constexpr char kRfPropertyConnectedPorts[] = "ConnectedPorts";
inline constexpr char kRfPropertyConnectedSwitchPorts[] =
    "ConnectedSwitchPorts";
inline constexpr char kRfPropertyActiveSoftwareImage[] = "ActiveSoftwareImage";

DEFINE_REDFISH_RESOURCE(ResourceCertificate, "Certificate");
DEFINE_REDFISH_RESOURCE(ResourceSystem, "ComputerSystem");
DEFINE_REDFISH_RESOURCE(ResourceChassis, "Chassis");
DEFINE_REDFISH_RESOURCE(ResourceComponentIntegrity, "ComponentIntegrity");
DEFINE_REDFISH_RESOURCE(ResourceMemory, "Memory");
DEFINE_REDFISH_RESOURCE(ResourceStorage, "Storage");
DEFINE_REDFISH_RESOURCE(ResourceDrive, "Drive");
DEFINE_REDFISH_RESOURCE(ResourceLegacyStorageController, "StorageController");
DEFINE_REDFISH_RESOURCE(ResourceStorageController, "StorageController");
DEFINE_REDFISH_RESOURCE(ResourceProcessor, "Processor");
DEFINE_REDFISH_RESOURCE(ResourceEthernetInterface, "EthernetInterface");
DEFINE_REDFISH_RESOURCE(ResourceTemperature, "Temperature");
DEFINE_REDFISH_RESOURCE(ResourceThermal, "Thermal");
DEFINE_REDFISH_RESOURCE(ResourceVoltage, "Voltage");
DEFINE_REDFISH_RESOURCE(ResourceFan, "Fan");
DEFINE_REDFISH_RESOURCE(ResourceSensor, "Sensor");
DEFINE_REDFISH_RESOURCE(ResourceSensorCollection, "SensorCollection");
DEFINE_REDFISH_RESOURCE(ResourcePcieFunction, "PCIeFunction");
DEFINE_REDFISH_RESOURCE(ResourceComputerSystem, "ComputerSystem");
DEFINE_REDFISH_RESOURCE(ResourceLogService, "LogService");
DEFINE_REDFISH_RESOURCE(ResourceLogEntry, "LogEntry");
DEFINE_REDFISH_RESOURCE(ResourceSoftwareInventory, "SoftwareInventory");
// The AssemblyEntry is a single item in the "Assemblies" list of
// Assembly resource
DEFINE_REDFISH_RESOURCE(ResourceAssemblyEntry, "/Assembly#/");
DEFINE_REDFISH_RESOURCE(ResourceManager, "Manager");
DEFINE_REDFISH_RESOURCE(ResourceManagerDiagnosticData, "ManagerDiagnosticData");
DEFINE_REDFISH_RESOURCE(ResourceDedicatedNetworkPorts, "DedicatedNetworkPorts");
DEFINE_REDFISH_RESOURCE(ResourcePcieSlots, "PCIeSlots");
DEFINE_REDFISH_RESOURCE(ResourceSwitch, "Switch");

// Physical LPU is an abstraction resource concept based on thread-granularity
// SubProcessor resource. It uses the same Processor resource schema. But the
// Redfish path of thread-granularity SubProcessor resource is different from a
// regular Processor resource.
DEFINE_REDFISH_RESOURCE(AbstractionPhysicalLpu, "PhysicalLpu");

// OEM Google resources
DEFINE_REDFISH_RESOURCE(OemResourceRootOfTrustCollection,
                        "RootOfTrustCollection");
DEFINE_REDFISH_RESOURCE(OemResourceRootOfTrust, "RootOfTrust");

DEFINE_REDFISH_PROPERTY(PropertyOdataId, std::string, "@odata.id");
DEFINE_REDFISH_PROPERTY(PropertyOdataType, std::string, "@odata.type");
DEFINE_REDFISH_PROPERTY(PropertyUuid, std::string, "UUID");
DEFINE_REDFISH_PROPERTY(PropertyMembers, std::string, "Members");
DEFINE_REDFISH_PROPERTY(PropertyMembersCount, int, "Members@odata.count");
DEFINE_REDFISH_PROPERTY(PropertyCapacityMiB, int, "CapacityMiB");
DEFINE_REDFISH_PROPERTY(PropertyLogicalSizeMiB, int, "LogicalSizeMiB");
DEFINE_REDFISH_PROPERTY(PropertyManufacturer, std::string, "Manufacturer");
DEFINE_REDFISH_PROPERTY(PropertyProducer, std::string, "Producer");
DEFINE_REDFISH_PROPERTY(PropertyEncryptionStatus, std::string,
                        "EncryptionStatus");
DEFINE_REDFISH_PROPERTY(PropertyPredictedMediaLifeLeftPercent, int,
                        "PredictedMediaLifeLeftPercent");
DEFINE_REDFISH_PROPERTY(PropertyChassisType, std::string, "ChassisType");
DEFINE_REDFISH_PROPERTY(PropertyVendor, std::string, "Vendor");
DEFINE_REDFISH_PROPERTY(PropertyMemoryDeviceType, std::string,
                        "MemoryDeviceType");
DEFINE_REDFISH_PROPERTY(PropertyName, std::string, "Name");
DEFINE_REDFISH_PROPERTY(PropertyId, std::string, "Id");
DEFINE_REDFISH_PROPERTY(PropertyOperatingSpeedMhz, int, "OperatingSpeedMhz");
DEFINE_REDFISH_PROPERTY(PropertyOperatingSpeedMHz, int, "OperatingSpeedMHz");
DEFINE_REDFISH_PROPERTY(PropertyPartNumber, std::string, "PartNumber");
DEFINE_REDFISH_PROPERTY(PropertyPhysicalContext, std::string,
                        "PhysicalContext");
DEFINE_REDFISH_PROPERTY(PropertySerialNumber, std::string, "SerialNumber");
DEFINE_REDFISH_PROPERTY(PropertySocket, std::string, "Socket");
DEFINE_REDFISH_PROPERTY(PropertyTotalCores, int, "TotalCores");
DEFINE_REDFISH_PROPERTY(PropertyTotalEnabledCores, int, "TotalEnabledCores");
DEFINE_REDFISH_PROPERTY(PropertyTotalThreads, int, "TotalThreads");
DEFINE_REDFISH_PROPERTY(PropertyMaxSpeedMHz, int, "MaxSpeedMHz");
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
DEFINE_REDFISH_PROPERTY(PropertyReadingVolts, double, "ReadingVolts");
DEFINE_REDFISH_PROPERTY(PropertyReading, double, "Reading");
DEFINE_REDFISH_PROPERTY(PropertyReadingType, std::string, "ReadingType");
DEFINE_REDFISH_PROPERTY(PropertyReadingUnits, std::string, "ReadingUnits");
DEFINE_REDFISH_PROPERTY(PropertyUpperThresholdCritical, double,
                        "UpperThresholdCritical");
DEFINE_REDFISH_PROPERTY(PropertyUpperThresholdNonCritical, double,
                        "UpperThresholdNonCritical");
DEFINE_REDFISH_PROPERTY(PropertyLowerThresholdCritical, double,
                        "LowerThresholdCritical");
DEFINE_REDFISH_PROPERTY(PropertyLowerThresholdNonCritical, double,
                        "LowerThresholdNonCritical");
DEFINE_REDFISH_PROPERTY(PropertyMetricProperty, std::string, "MetricProperty");
DEFINE_REDFISH_PROPERTY(PropertyMetricValue, std::string, "MetricValue");
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
DEFINE_REDFISH_PROPERTY(PropertyProcessorIdFamily, std::string,
                        "EffectiveFamily");
DEFINE_REDFISH_PROPERTY(PropertyProcessorIdModel, std::string,
                        "EffectiveModel");
DEFINE_REDFISH_PROPERTY(PropertyProcessorIdMicrocode, std::string,
                        "MicrocodeInfo");
DEFINE_REDFISH_PROPERTY(PropertyDataSourceUri, std::string, "DataSourceUri");
DEFINE_REDFISH_PROPERTY(PropertyRedfishActionInfo, std::string,
                        "@Redfish.ActionInfo");
DEFINE_REDFISH_PROPERTY(PropertyRedfishAllowableValues,
                        std::vector<std::string>, "@Redfish.AllowableValues");
DEFINE_REDFISH_PROPERTY(PropertyTarget, std::string, "target");
DEFINE_REDFISH_PROPERTY(PropertyResponseCode, std::string, "code");
DEFINE_REDFISH_PROPERTY(PropertyResponseMessage, std::string, "message");
DEFINE_REDFISH_PROPERTY(PropertyDateTime, absl::Time, "DateTime");
DEFINE_REDFISH_PROPERTY(PropertyLastResetTime, absl::Time, "LastResetTime");
DEFINE_REDFISH_PROPERTY(PropertyEventId, std::string, "EventId");
DEFINE_REDFISH_PROPERTY(PropertyUpdateable, bool, "Updateable");
DEFINE_REDFISH_PROPERTY(PropertyWriteProtected, bool, "WriteProtected");
DEFINE_REDFISH_PROPERTY(PropertyEntryType, std::string, "EntryType");
DEFINE_REDFISH_PROPERTY(PropertyOemRecordFormat, std::string,
                        "OemRecordFormat");
DEFINE_REDFISH_PROPERTY(PropertyMessage, std::string, "Message");
DEFINE_REDFISH_PROPERTY(PropertyPartLocationContext, std::string,
                        "PartLocationContext");
DEFINE_REDFISH_PROPERTY(PropertyVersion, std::string, "Version");
DEFINE_REDFISH_PROPERTY(PropertyLocationType, std::string, "LocationType");
DEFINE_REDFISH_PROPERTY(PropertyServiceLabel, std::string, "ServiceLabel");
DEFINE_REDFISH_PROPERTY(PropertyReplaceable, bool, "Replaceable");
DEFINE_REDFISH_PROPERTY(PropertyCorrectableECCErrorCount, int,
                        "CorrectableECCErrorCount");
DEFINE_REDFISH_PROPERTY(PropertyUncorrectableECCErrorCount, int,
                        "UncorrectableECCErrorCount");
DEFINE_REDFISH_PROPERTY(PropertyIndetermineCorrectableErrorCount, int,
                        "IndeterminateCorrectableErrorCount");
DEFINE_REDFISH_PROPERTY(PropertyIndetermineUncorrectableErrorCount, int,
                        "IndeterminateUncorrectableErrorCount");
DEFINE_REDFISH_PROPERTY(PropertyUserName, std::string, "UserName");
DEFINE_REDFISH_PROPERTY(PropertyPassword, std::string, "Password");
DEFINE_REDFISH_PROPERTY(PropertyTargetComponentURI, std::string,
                        "TargetComponentURI");

DEFINE_REDFISH_PROPERTY(PropertyCertificateString, std::string,
                        "CertificateString");
DEFINE_REDFISH_PROPERTY(PropertyCertificateType, std::string,
                        "CertificateType");
DEFINE_REDFISH_PROPERTY(PropertySlotId, int, "SlotId");
DEFINE_REDFISH_PROPERTY(PropertyNonce, std::string, "Nonce");
DEFINE_REDFISH_PROPERTY(PropertySignedMeasurements, std::string,
                        "SignedMeasurements");
DEFINE_REDFISH_PROPERTY(PropertyHashingAlgorithm, std::string,
                        "HashingAlgorithm");
DEFINE_REDFISH_PROPERTY(PropertySigningAlgorithm, std::string,
                        "SigningAlgorithm");
DEFINE_REDFISH_PROPERTY(PropertyMeasurementIndices, std::vector<int64_t>,
                        "MeasurementIndices");
DEFINE_REDFISH_PROPERTY(PropertyCommand, std::string, "Command");
DEFINE_REDFISH_PROPERTY(PropertyCommandResponse, std::string,
                        "CommandResponse");
DEFINE_REDFISH_PROPERTY(PropertyModel, std::string, "Model");
DEFINE_REDFISH_PROPERTY(PropertyProcessorType, std::string, "ProcessorType");
DEFINE_REDFISH_PROPERTY(PropertyCorrectableCoreErrorCount, int,
                        "CorrectableCoreErrorCount");
DEFINE_REDFISH_PROPERTY(PropertyUncorrectableCoreErrorCount, int,
                        "UncorrectableCoreErrorCount");
DEFINE_REDFISH_PROPERTY(PropertyCorrectableOtherErrorCount, int,
                        "CorrectableOtherErrorCount");
DEFINE_REDFISH_PROPERTY(PropertyUncorrectableOtherErrorCount, int,
                        "UncorrectableOtherErrorCount");
DEFINE_REDFISH_PROPERTY(PropertyDiagnosticData, std::string, "DiagnosticData");
DEFINE_REDFISH_PROPERTY(PropertyAdditionalDataURI, std::string,
                        "AdditionalDataURI");
DEFINE_REDFISH_PROPERTY(PropertyCreatedTime, std::string, "Created");
DEFINE_REDFISH_PROPERTY(PropertySlotType, std::string, "SlotType");
DEFINE_REDFISH_PROPERTY(PropertyReleaseDate, std::string, "ReleaseDate");
DEFINE_REDFISH_PROPERTY(PropertyServiceEntryPointUuid, std::string,
                        "ServiceEntryPointUUID");
DEFINE_REDFISH_PROPERTY(PropertyAllowableMaxMHz, int, "AllowableMaxMHz");
DEFINE_REDFISH_PROPERTY(PropertyAllowedSpeedsMHz, std::vector<int>,
                        "AllowedSpeedsMHz");
DEFINE_REDFISH_PROPERTY(PropertyAllowableMax, int, "AllowableMax");
DEFINE_REDFISH_PROPERTY(PropertyAllowableMin, int, "AllowableMin");
DEFINE_REDFISH_PROPERTY(PropertySetPoint, int, "SetPoint");
DEFINE_REDFISH_PROPERTY(PropertyECCModeEnabled, bool, "ECCModeEnabled");
DEFINE_REDFISH_PROPERTY(PropertyTotalMemorySizeMiB, int, "TotalMemorySizeMiB");
DEFINE_REDFISH_PROPERTY(PropertyDramBandwidthGbps, double, "DRAMBandwidthGbps");
DEFINE_REDFISH_PROPERTY(PropertySwitchType, std::string, "SwitchType");
DEFINE_REDFISH_PROPERTY(PropertyHealthRollup, std::string, "HealthRollup");
DEFINE_REDFISH_PROPERTY(PropertyMessageId, std::string, "MessageId");
DEFINE_REDFISH_PROPERTY(PropertySeverity, std::string, "Severity");
DEFINE_REDFISH_PROPERTY(PropertyTimestamp, absl::Time, "Timestamp");
DEFINE_REDFISH_PROPERTY(PropertyBandwidthPercent, double, "BandwidthPercent");
DEFINE_REDFISH_PROPERTY(PropertyRxBytes, int, "RXBytes");
DEFINE_REDFISH_PROPERTY(PropertyTxBytes, int, "TXBytes");
DEFINE_REDFISH_PROPERTY(PropertyNamespaceId, std::string, "NamespaceId");

// ManagerDiagnosticData properties
DEFINE_REDFISH_PROPERTY(PropertyKernelPercent, double, "KernelPercent");
DEFINE_REDFISH_PROPERTY(PropertyUserPercent, double, "UserPercent");
DEFINE_REDFISH_PROPERTY(PropertyLatency, double, "Latency");
DEFINE_REDFISH_PROPERTY(PropertyAvailableBytes, int64_t, "AvailableBytes");
DEFINE_REDFISH_PROPERTY(PropertyBootCount, int, "BootCount");
DEFINE_REDFISH_PROPERTY(PropertyCrashCount, int, "CrashCount");
DEFINE_REDFISH_PROPERTY(PropertyCurrentSpeedGbps, double, "CurrentSpeedGbps");
DEFINE_REDFISH_PROPERTY(PropertyLinkState, std::string, "LinkState");
DEFINE_REDFISH_PROPERTY(PropertyRXDiscards, int, "RXDiscards");
DEFINE_REDFISH_PROPERTY(PropertyTXDiscards, int, "TXDiscards");
DEFINE_REDFISH_PROPERTY(PropertyRestartCount, int, "RestartCount");
DEFINE_REDFISH_PROPERTY(PropertyUptimeSeconds, double, "UptimeSeconds");
DEFINE_REDFISH_PROPERTY(PropertyCommandLine, std::string, "CommandLine");
DEFINE_REDFISH_PROPERTY(PropertyServiceRootUptimeSeconds, double,
                        "ServiceRootUptimeSeconds");
DEFINE_REDFISH_PROPERTY(PropertyResidentSetSizeBytes, int64_t,
                        "ResidentSetSizeBytes");
DEFINE_REDFISH_PROPERTY(PropertyUserTimeSeconds, double, "UserTimeSeconds");
DEFINE_REDFISH_PROPERTY(PropertyKernelTimeSeconds, double, "KernelTimeSeconds");
DEFINE_REDFISH_PROPERTY(PropertyNFileDescriptors, int64_t, "NFileDescriptors");
DEFINE_REDFISH_PROPERTY(PropertyFirmwareTimeSeconds, double,
                        "FirmwareTimeSeconds");
DEFINE_REDFISH_PROPERTY(PropertyLoaderTimeSeconds, double, "LoaderTimeSeconds");
DEFINE_REDFISH_PROPERTY(PropertyInitrdTimeSeconds, double, "InitrdTimeSeconds");
DEFINE_REDFISH_PROPERTY(PropertyUserSpaceTimeSeconds, double,
                        "UserSpaceTimeSeconds");

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
DEFINE_REDFISH_PROPERTY(OemGooglePropertyTopologyRepresentation, std::string,
                        "TopologyRepresentation");
DEFINE_REDFISH_PROPERTY(OemGooglePropertyTcgOpalAdditionalDataStoreRawData,
                        std::string, "TcgOpalAdditionalDataStoreRawData");
DEFINE_REDFISH_PROPERTY(OemGooglePropertyBareMetal, std::string, "BareMetal");
DEFINE_REDFISH_PROPERTY(OemGooglePropertyNerf, std::string, "NERF");
DEFINE_REDFISH_PROPERTY(OemPropertyRootOfTrust, std::string, "RootOfTrust");
DEFINE_REDFISH_PROPERTY(OemPropertyEmbeddedLocationContext,
                        std::vector<std::string>, "EmbeddedLocationContext");

// Redfish Task properties.
DEFINE_REDFISH_PROPERTY(PropertyTaskStatus, std::string, "TaskStatus");
DEFINE_REDFISH_PROPERTY(PropertyTaskStatusOk, std::string, "Ok");
DEFINE_REDFISH_PROPERTY(PropertyTaskState, std::string, "TaskState");
DEFINE_REDFISH_PROPERTY(PropertyTaskStateNew, std::string, "New");
DEFINE_REDFISH_PROPERTY(PropertyTaskStateStarting, std::string, "Starting");
DEFINE_REDFISH_PROPERTY(PropertyTaskStateRunning, std::string, "Running");
DEFINE_REDFISH_PROPERTY(PropertyTaskStateSuspended, std::string, "Suspended");
DEFINE_REDFISH_PROPERTY(PropertyTaskStateInterruped, std::string,
                        "Interrupted");
DEFINE_REDFISH_PROPERTY(PropertyTaskStatePending, std::string, "Pending");
DEFINE_REDFISH_PROPERTY(PropertyTaskStateStopping, std::string, "Stopping");
DEFINE_REDFISH_PROPERTY(PropertyTaskStateCompleted, std::string, "Completed");
DEFINE_REDFISH_PROPERTY(PropertyTaskStateException, std::string, "Exception");
DEFINE_REDFISH_PROPERTY(PropertyTaskStateService, std::string, "Service");
DEFINE_REDFISH_PROPERTY(PropertyTaskStateCancelling, std::string, "Cancelling");
DEFINE_REDFISH_PROPERTY(PropertyTaskStateCancelled, std::string, "Cancelled");

// Redfish agent expand support capabilities
DEFINE_REDFISH_PROPERTY(ExpandQueryExpandAll, bool, "ExpandAll");
DEFINE_REDFISH_PROPERTY(ExpandQueryLevels, bool, "Levels");
DEFINE_REDFISH_PROPERTY(ExpandQuerykLinks, bool, "Links");
DEFINE_REDFISH_PROPERTY(ExpandQuerykMaxLevels, int, "MaxLevels");
DEFINE_REDFISH_PROPERTY(ExpandQuerykNoLinks, bool, "NoLinks");

// Redfish agent top and skip support capabilities
DEFINE_REDFISH_PROPERTY(TopSkipQuery, bool, "TopSkipQuery");

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_PROPERTY_DEFINITIONS_H_
