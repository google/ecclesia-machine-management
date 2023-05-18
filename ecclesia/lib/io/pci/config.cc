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

#include "ecclesia/lib/io/pci/config.h"

#include <cstdint>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/codec/bits.h"
#include "ecclesia/lib/io/pci/location.h"
#include "ecclesia/lib/io/pci/region.h"
#include "ecclesia/lib/io/pci/signature.h"
#include "ecclesia/lib/status/macros.h"

namespace ecclesia {
namespace {

// Given a capability pointer, convert it to the offset in config space where
// the capability is located. Capabilities are always aligned on 32-bit
// boundaries but the lower two bits off the offset are technically reserved
// rather than guaranteed to be zero and so they need to be masked off.
uint8_t CapabilityPointerToOffset(uint8_t cap_ptr) { return cap_ptr & 0xfc; }

}  // namespace

PcieLinkSpeed NumToPcieLinkSpeed(double speed_gts) {
  // Multiply the double type speed value in unit of GT/s by 10 and covert to an
  // int, in order to facilitate the switch look up.
  int speed_gts_x10 = static_cast<int>(speed_gts * 10);

  switch (speed_gts_x10) {
    case 25:
      return PcieLinkSpeed::kGen1Speed2500MT;
    case 50:
      return PcieLinkSpeed::kGen2Speed5GT;
    case 80:
      return PcieLinkSpeed::kGen3Speed8GT;
    case 160:
      return PcieLinkSpeed::kGen4Speed16GT;
    case 320:
      return PcieLinkSpeed::kGen5Speed32GT;
    default:
      return PcieLinkSpeed::kUnknown;
  }
}

PcieLinkSpeed PcieGenToLinkSpeed(absl::string_view gen) {
  if (gen == "Gen1") {
    return PcieLinkSpeed::kGen1Speed2500MT;
  }
  if (gen == "Gen2") {
    return PcieLinkSpeed::kGen2Speed5GT;
  }
  if (gen == "Gen3") {
    return PcieLinkSpeed::kGen3Speed8GT;
  }
  if (gen == "Gen4") {
    return PcieLinkSpeed::kGen4Speed16GT;
  }
  if (gen == "Gen5") {
    return PcieLinkSpeed::kGen5Speed32GT;
  }
  return PcieLinkSpeed::kUnknown;
}

int PcieLinkSpeedToMts(PcieLinkSpeed speed) {
  switch (speed) {
    case PcieLinkSpeed::kGen1Speed2500MT:
      return 2500;
    case PcieLinkSpeed::kGen2Speed5GT:
      return 5000;
    case PcieLinkSpeed::kGen3Speed8GT:
      return 8000;
    case PcieLinkSpeed::kGen4Speed16GT:
      return 16000;
    case PcieLinkSpeed::kGen5Speed32GT:
      return 32000;
    case PcieLinkSpeed::kUnknown:
    default:
      return 0;
  }
}

PcieLinkWidth NumToPcieLinkWidth(int width) {
  switch (width) {
    case 1:
      return PcieLinkWidth::kWidth1;
    case 2:
      return PcieLinkWidth::kWidth2;
    case 4:
      return PcieLinkWidth::kWidth4;
    case 8:
      return PcieLinkWidth::kWidth8;
    case 12:
      return PcieLinkWidth::kWidth12;
    case 16:
      return PcieLinkWidth::kWidth16;
    case 32:
      return PcieLinkWidth::kWidth32;
    default:
      return PcieLinkWidth::kUnknown;
  }
}

// Converts a link width constant to an integer count of lanes.
int PcieLinkWidthToInt(PcieLinkWidth width) {
  switch (width) {
    case PcieLinkWidth::kWidth1:
      return 1;
    case PcieLinkWidth::kWidth2:
      return 2;
    case PcieLinkWidth::kWidth4:
      return 4;
    case PcieLinkWidth::kWidth8:
      return 8;
    case PcieLinkWidth::kWidth12:
      return 12;
    case PcieLinkWidth::kWidth16:
      return 16;
    case PcieLinkWidth::kWidth32:
      return 32;
    case PcieLinkWidth::kUnknown:
    default:
      return 0;
  }
}

absl::StatusOr<uint8_t> PciCapability::CapabilityId() const {
  return Read8(kCapIdOffset);
}
absl::StatusOr<uint8_t> PciCapability::NextCapabilityPointer() const {
  return Read8(kNextCapPtrOffset);
}

absl::StatusOr<PciCapability> PciCapability::NextCapability() const {
  ECCLESIA_ASSIGN_OR_RETURN(uint8_t next_ptr, NextCapabilityPointer());
  // A next value of 0 means there are no more capabilities.
  if (next_ptr == 0) return absl::NotFoundError("no more capabilities");
  return PciCapability(region_, CapabilityPointerToOffset(next_ptr));
}

absl::StatusOr<PciSubsystemSignature>
PciSubsystemCapability::SubsystemSignature() const {
  ECCLESIA_ASSIGN_OR_RETURN(uint16_t ssvid, Read16(kSsvidOffset));
  ECCLESIA_ASSIGN_OR_RETURN(uint16_t ssid, Read16(kSsidOffset));
  return PciSubsystemSignature(PciIdNum::Make(ssvid), PciIdNum::Make(ssid));
}

absl::StatusOr<PciExpressCapability::LinkCapabilities>
PciExpressCapability::ReadLinkCapabilities() const {
  ECCLESIA_ASSIGN_OR_RETURN(uint32_t link_caps_val, Read32(kLinkCapsOffset));
  LinkCapabilities link_caps;
  link_caps.max_speed =
      static_cast<PcieLinkSpeed>(ExtractBits(link_caps_val, BitRange(3, 0)));
  link_caps.max_width =
      static_cast<PcieLinkWidth>(ExtractBits(link_caps_val, BitRange(9, 4)));
  link_caps.dll_active_capable = ExtractBits(link_caps_val, BitRange(20));
  return link_caps;
}

absl::StatusOr<PciExpressCapability::LinkStatus>
PciExpressCapability::ReadLinkStatus() const {
  ECCLESIA_ASSIGN_OR_RETURN(uint16_t link_status_val,
                            Read16(kLinkStatusOffset));
  LinkStatus link_status;
  link_status.current_speed =
      static_cast<PcieLinkSpeed>(ExtractBits(link_status_val, BitRange(3, 0)));
  link_status.current_width =
      static_cast<PcieLinkWidth>(ExtractBits(link_status_val, BitRange(9, 4)));
  link_status.dll_active = ExtractBits(link_status_val, BitRange(13));
  return link_status;
}

absl::StatusOr<PciBaseSignature> PciConfigSpace::BaseSignature() const {
  ECCLESIA_ASSIGN_OR_RETURN(uint16_t vid, region_->Read16(kVidOffset));
  ECCLESIA_ASSIGN_OR_RETURN(uint16_t did, region_->Read16(kDidOffset));
  return PciBaseSignature(PciIdNum::Make(vid), PciIdNum::Make(did));
}

// Class code occupies the three bytes after the revision ID. To read the value
// we do a 32-bit read starting at revision ID and then pull out the 24 bits
// that form the class code.
absl::StatusOr<uint32_t> PciConfigSpace::ClassCode() const {
  ECCLESIA_ASSIGN_OR_RETURN(uint32_t rev_id,
                            region_->Read32(kRevisionIdOffset));
  return rev_id >> 8;
}

absl::StatusOr<uint8_t> PciConfigSpace::HeaderType() const {
  ECCLESIA_ASSIGN_OR_RETURN(uint8_t header_type,
                            region_->Read8(kHeaderTypeOffset));
  return ExtractBits(header_type, BitRange(6, 0));
}

PciCapabilityIterator PciConfigSpace::Capabilities() const {
  auto maybe_ptr = region_->Read8(kCapPointerOffset);
  if (!maybe_ptr.ok()) return PciCapabilityIterator();
  uint8_t ptr = CapabilityPointerToOffset(*maybe_ptr);
  if (ptr == 0) return PciCapabilityIterator();
  return PciCapabilityIterator(PciCapability(region_, ptr));
}

absl::StatusOr<PciSubsystemSignature> PciConfigSpace::SubsystemSignature(
    uint8_t header_type) const {
  return WithSpecificType<PciSubsystemSignature>(
      [](PciType0ConfigSpace type0) { return type0.SubsystemSignatureImpl(); },
      [](PciType1ConfigSpace type1) { return type1.SubsystemSignatureImpl(); },
      header_type);
}

absl::StatusOr<PciSubsystemSignature> PciConfigSpace::SubsystemSignature()
    const {
  absl::StatusOr<uint8_t> header_type = HeaderType();
  if (!header_type.ok()) return header_type.status();
  return SubsystemSignature(*header_type);
}

absl::StatusOr<PciSubsystemSignature>
PciType0ConfigSpace::SubsystemSignatureImpl() const {
  // For endpoint types the SSVID and SSID come out of the core config.
  ECCLESIA_ASSIGN_OR_RETURN(uint16_t ssvid,
                            region_->Read16(kSubsysVendorIdOffset));
  ECCLESIA_ASSIGN_OR_RETURN(uint16_t ssid, region_->Read16(kSubsysIdOffset));
  return PciSubsystemSignature(PciIdNum::Make(ssvid), PciIdNum::Make(ssid));
}

absl::StatusOr<PciBusNum> PciType1ConfigSpace::SecondaryBusNum() const {
  ECCLESIA_ASSIGN_OR_RETURN(uint8_t num, region_->Read8(kSecBusNumOffset));
  return PciBusNum::Make(num);
}

absl::StatusOr<PciBusNum> PciType1ConfigSpace::SubordinateBusNum() const {
  ECCLESIA_ASSIGN_OR_RETURN(uint8_t num, region_->Read8(kSubBusNumOffset));
  return PciBusNum::Make(num);
}

absl::StatusOr<PciSubsystemSignature>
PciType1ConfigSpace::SubsystemSignatureImpl() const {
  // For bridge types we need to look for a subsystem capability.
  for (const PciCapability &capability : Capabilities()) {
    auto maybe_subsys_cap = capability.GetIf<PciSubsystemCapability>();
    if (!maybe_subsys_cap.ok()) continue;
    return maybe_subsys_cap->SubsystemSignature();
  }
  return absl::UnimplementedError("no subsystem capability found on bridge");
}

}  // namespace ecclesia
