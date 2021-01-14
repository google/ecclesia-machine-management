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

#ifndef ECCLESIA_LIB_IO_PCI_REGISTERS_H_
#define ECCLESIA_LIB_IO_PCI_REGISTERS_H_

#include <cstdint>

namespace ecclesia {

// Power Management Capability.
constexpr uint8_t kPciCapIdPowerManagement = 0x01;
constexpr uint8_t kPciCapPowerManagementPmc = 0x02;
constexpr uint8_t kPciCapPowerManagementPmcsr = 0x04;
constexpr uint8_t kPciCapPowerManagementData = 0x07;

// Pci-x Capability.
constexpr uint8_t kPciCapIdPcix = 0x07;
constexpr uint8_t kPciCapPcixCmdOffset = 0x02;
constexpr uint8_t kPciCapPcixStatusOffset = 0x04;

// Pci Msi-x Capability
constexpr uint8_t kPciCapIdMsix = 0x11;
constexpr uint8_t kPciCapIdMsixMcOffset = 0x02;
constexpr uint8_t kPciCapIdMsixTableOffset = 0x04;
constexpr uint8_t kPciCapIdMsixPbaOffset = 0x08;

// Pci Express Extended Capabilities.
constexpr uint16_t kPcieExtCapStartReg = 0x100;

// Pci Express Advanced Error Reporting Capability.
constexpr uint16_t kPcieExtCapIdAer = 0x0001;
constexpr uint16_t kPcieExtCapVersionAer = 0x1;
constexpr uint16_t kPcieExtCapAerNoncomplexSize = 44;
constexpr uint16_t kPcieExtCapAerComplexSize = 56;

// Pci Express Device Serial Number Capability.
constexpr uint16_t kPcieExtCapIdDsn = 0x0003;
constexpr uint16_t kPcieExtCapVersionDsn = 0x1;
constexpr uint16_t kPcieExtCapDsnLower = 0x4;
constexpr uint16_t kPcieExtCapDsnUpper = 0x8;

// Pci Express Access Control Services Capability
constexpr uint16_t kPcieExtCapIdAcs = 0x00;
constexpr uint16_t kPcieExtCapVersionAcs = 0x1;
constexpr uint16_t kPcieExtCapAcsComplexSize = 8;

// Secondary Pci Express Extended Capability.
constexpr uint16_t kSecondaryPcieExtCapId = 0x0019;
constexpr uint16_t kSecondaryPcieExtCapVersion = 0x1;
constexpr uint16_t kSecondaryPcieExtCapComplexSize = 140;

// Pci Express Link Status Accessors.
constexpr uint32_t kPciCapPcieLinkStatusSpeedMask = (0x7 << 0);
constexpr uint32_t kPciCapPcieLinkStatusWidthMask = (0x3f << 4);
constexpr uint32_t kPciCapPcieLinkStatusTrainingMask = (1 << 11);
constexpr uint32_t kPciCapPcieLinkStatusSlotClockConfigMask = (1 << 12);
constexpr uint32_t kPciCapPcieLinkStatusDataLinkLayerLinkActiveMask = (1 << 13);
constexpr uint32_t kPciCapPcieLinkStatusBandwidthManamgementMask = (1 << 14);
constexpr uint32_t kPciCapPcieLinkStatusAutonomousBandwidthMask = (1 << 15);

constexpr uint32_t kPciCapPcieLinkSpeed25 = 0x1;
constexpr uint32_t kPciCapPcieLinkSpeed50 = 0x2;

constexpr uint16_t kPciCapPcieLinkWidthX1 = 0x010;
constexpr uint16_t kPciCapPcieLinkWidthX2 = 0x020;
constexpr uint16_t kPciCapPcieLinkWidthX4 = 0x040;
constexpr uint16_t kPciCapPcieLinkWidthX8 = 0x080;
constexpr uint16_t kPciCapPcieLinkWidthX16 = 0x100;
constexpr uint16_t kPciCapPcieLinkWidthX32 = 0x200;

// Hypertransport Capability.
constexpr uint32_t kPciCapIdHypertransport = 0x08;
constexpr uint32_t kPciCapHypertransportCmdOffset = 0x02;

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_IO_PCI_REGISTERS_H_
