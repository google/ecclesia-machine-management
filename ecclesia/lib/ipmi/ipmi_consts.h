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

#ifndef ECCLESIA_LIB_IPMI_IPMI_CONSTS_H_
#define ECCLESIA_LIB_IPMI_IPMI_CONSTS_H_

#include <cstdint>

namespace ecclesia {

// These constants are defined in the IPMI spec and not subject to change.
inline constexpr uint8_t kIpmiNetFnChassis = 0x0;
inline constexpr uint8_t kIpmiNetFnSe = 0x4;
inline constexpr uint8_t kIpmiNetFnApp = 0x6;
inline constexpr uint8_t kIpmiNetFnOem = 0x2e;

inline constexpr int kIpmiOemGoogle = 11129;  // IPMI_OEM_GOOGLE
inline constexpr int kIpmiOemLenovo = 19046;  // IPMI_OEM_LENOVO

// Default is name-only lookup, from ipmitool's ipmi_main.c
inline constexpr uint8_t kIpmiDefaultLookupBit = 0x10;

// Request header size.
inline constexpr int kNetFnCommandLength = 2;
// Response header size.
inline constexpr int kNetFnCmdCodeLength = 3;
inline constexpr int kLogicalUnitNumberMask = 0x3;
inline constexpr int kNetworkFunctionShift = 2;
inline constexpr int kNetworkFunctionResponseFlag = 0x1;

// NetFn+Lun | Cmd | OEM (3 bytes)
inline constexpr int kMinimumOemHeaderLength = 5;
inline constexpr int kOemNumberOffset = 2;
inline constexpr int kNetFnLunOffset = 0;
inline constexpr int kCommandOffset = 1;
inline constexpr int kCompletionCodeOffset = 2;

inline constexpr int OEN_SIZE = 3;

// There are four bytes of header on requests, and five on replies.
// To be more conservative on anything else, reduce by another byte.
// KCS is set at 256 with 3 bytes down and 4 up for header. However, the
// openbmc ipmid currently uses only 64 bytes for replies regardless of
// mechanism.
inline constexpr int kMaximumPipelineBandwidth = 58;

// OEM Group identifier for OpenBMC.
inline constexpr uint8_t OEN_OPENBMC[OEN_SIZE] = {0xcf, 0xc2, 0x00};
inline constexpr uint8_t OEN_GOOGLE[OEN_SIZE] = {0x79, 0x2b, 0x00};
inline constexpr uint8_t OEN_LENOVO[OEN_SIZE] = {0x66, 0x4a, 0x00};

// IPMI Completion Codes.
inline constexpr uint8_t IPMI_OK_CODE = 0x00;
inline constexpr uint8_t IPMI_INVALID_CMD_COMPLETION_CODE = 0xC1;
inline constexpr uint8_t IPMI_TIMEOUT_COMPLETION_CODE = 0xC3;

// The length needs to be at least two bytes [netfn][cmd]
inline constexpr int kMinimumIpmiPacketLength = 2;

// IPMI commands from the spec.
enum class IpmiCommand : uint8_t {
  kGetDeviceId = 0x1,
  kBmcColdReset = 0x2,
  kBmcWarmReset = 0x3,
  kGetWatchdogTimer = 0x25,
  kLanSetConfig = 0x01,
  kLanGetConfig = 0x02,
  kSetChannelAccess = 0x40,

  kOemFanControlCommand = 4,           // phosphor-pid-control
  kOemEthernetStatisticsCommand = 48,  // phosphor-ipmi-ethstats
  kOemGsysCommand = 50,                // google-ipmi-sys
  kOemNemoraSettingsCommand = 52,

  kOemFlashUpdateCommand = 127,  // google-ipmi-flash
  kOemCodeLabEchoCommand = 126,
};

// IPMI network functions from the spec.
enum class IpmiNetworkFunction : uint8_t {
  kChassis = 0x00,
  kSensorOrEvent = 0x04,
  kApp = 0x6,
  kStorage = 0x0a,
  kTransport = 0x0c,
  kGroup = 0x2c,
  kOem = 0x2e,
};

// Enumerations of command IDs in the "App" network function class, as defined
// in Appendix G.
enum class SensorOrEventCommandId {
  kGetDeviceSdr = 0x21,
  kGetDeviceSdrInfo = 0x20,
  kReserveDeviceSdrRepository = 0x22,
  kGetSensorThreshold = 0x27,
  kGetSensorReading = 0x2d,
  kGetSensorType = 0x2f,
  kSetSensorReadingAndEventStatus = 0x30,
};

// Enumerations of command IDs in the "App" network function class, as defined
// in Appendix G.
enum class AppCommandId {
  kGetDeviceId = 0x1,
  kBmcColdReset = 0x2,
  kBmcWarmReset = 0x3,
  kGetDeviceGuid = 0x8,
  kResetWatchdogTimer = 0x22,
  kSetWatchdogTimer = 0x24,
  kGetWatchdogTimer = 0x25,
  kSetBmcGlobalEnables = 0x2e,
  kGetBmcGlobalEnables = 0x2f,
  kClearMessageFlags = 0x30,
  kReadEventMessageBuffer = 0x35,
  kGetChannelInfo = 0x42,
};

// Enumerations of command IDs in the "Storage" network function class, as
// defined in Appendix G.
enum class StorageCommandId {
  kGetFruInventoryAreaInfo = 0x10,
  kReadFruData = 0x11,
  kWriteFruData = 0x12,
  kClearSdrRepository = 0x27,
  kGetSdr = 0x23,
  kGetSdrRepositoryInfo = 0x20,
  kGetSensorHysteresis = 0x25,
  kReserveSdrRepository = 0x22,
  kGetSelInfo = 0x40,
  kAddSelEntry = 0x44,
};

enum class TransportCommandId {
  kSetLanConfig = 0x01,
  kGetLanConfig = 0x02,
};

enum class CompletionCodes {
  kOk = 0x00,
  kWatchdogNotInitialized = 0x80,
  kBusy = 0xc0,
  kInvalid = 0xc1,
  kTimeout = 0xc3,
  kInvalidReservationId = 0xc5,
  kRequestDataLengthInvalid = 0xc7,
  kParamOutOfRange = 0xc9,
  kSensorInvalid = 0xcb,
  kInvalidFieldRequest = 0xcc,
  kIllegalCommand = 0xcd,
  kResponseCouldNotBeProvided = 0xce,
  kLanParamNotSupported = 0x80,
  kLanParamSetLocked = 0x81,
  kLanParamReadOnly = 0x82,
};

enum class GroupExtensions {
  kDcmiExtension = 0xdc,
};

enum class DcmiCommandId {
  // Abbreviation of Set Management Controller Identifier String.
  kSetMgmtControllerIdString = 0x0a,
  kSetAssetTag = 0x08,
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_IPMI_IPMI_CONSTS_H_
