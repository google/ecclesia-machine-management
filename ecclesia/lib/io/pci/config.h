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

// Defines objects for interacting with PCI config space, via generic config
// space classes as well as similar classes representing specific capabilities.
// These abstractions are all built on top of the generic PCI region API.
//
// This library provides two generic classes for reading from config space:
//
//   * PciConfigSpace -> provides a general base for accessing the entire space
//       and providing any universal operations.
//   * PciCapability -> provides a general base for accessing a capability
//       within config space.
//
// On top of these classes there are multiple subclasses which provide access to
// more specific operations. For PciConfigSpace there are different subclasses
// for different device types (endpoints and bridges) and for capabilities there
// are different subclasses for different capability types.
//
// Note that all of these classes are intended to be used as value classes, not
// interface classes. They provide no synchronization or state, and do not use
// virtual functions to allow behavior to be customized or overridden. In order
// to mitigate the danger of the types not having virtual destructors we require
// that they all are trivially destructible.
//
// The subclasses all define a static constexpr member which is used to specify
// which particular subtype they intend to support. These constants match the
// identifier values as specified by the PCI standard.
//
//   * PciConfigSpace -> kHeaderType (the config space header type)
//   * PciCapability -> kCapabilityId (the capability ID)
//

#ifndef ECCLESIA_LIB_IO_PCI_CONFIG_H_
#define ECCLESIA_LIB_IO_PCI_CONFIG_H_

#include <cstdint>
#include <type_traits>

#include "absl/functional/function_ref.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/io/pci/location.h"
#include "ecclesia/lib/io/pci/region.h"
#include "ecclesia/lib/io/pci/signature.h"
#include "ecclesia/lib/status/macros.h"

namespace ecclesia {

// Link speed. These correspond to the "supported link speeds" vector which is
// used by multiple registers in decoding the PciExpress capability. Please note
// that the PCIe Generation and the field value below are not guaranteed to be
// matched in the future. It's not encouraged to just cast the value to get the
// generation version.
enum class PcieLinkSpeed : uint8_t {
  kUnknown = 0,
  kGen1Speed2500MT = 1,  // 2.5 GT/s
  kGen2Speed5GT = 2,     // 5 GT/s
  kGen3Speed8GT = 3,     // 8 GT/s
  kGen4Speed16GT = 4,    // 16 GT/s
  kGen5Speed32GT = 5,    // 32 GT/s
  kGen6Speed64GT = 6,    // 64 GT/s
  kGen7Speed128GT = 7,   // 128 GT/s
};

// The width of a link. This bit pattern is used by multiple registers in
// decoding the PciExpress capability. Please note that the link width and the
// field value below are not guaranteed to be matched in the future. It's not
// encouraged to just cast the value to get the link width.
enum class PcieLinkWidth : uint8_t {
  kUnknown = 0,
  kWidth1 = 1,
  kWidth2 = 2,
  kWidth4 = 4,
  kWidth8 = 8,
  kWidth12 = 12,
  kWidth16 = 16,
  kWidth32 = 32,
};

// A helper function to convert the speed in unit of GT/s to PcieLinkSpeed enum.
// For invalid speed, it will return kSpeedUnknown.
PcieLinkSpeed NumToPcieLinkSpeed(double speed_gts);

// A helper function to convert the pcie link speed generation(in string format)
// to pcie link speed. For invalid generation, it would return kSpeedUnknown.
PcieLinkSpeed PcieGenToLinkSpeed(absl::string_view gen);

// A helper function to convert the PcieLinkSpeedType to speed in unit of MT/s.
int PcieLinkSpeedToMts(PcieLinkSpeed speed);

// Helper functions to convert the width (number of lanes) to/from PcieLinkWidth
// enum. Invalid or width-0 link will be mapped to PcieLinkWidth::kUnknown.
PcieLinkWidth NumToPcieLinkWidth(int width);
int PcieLinkWidthToInt(PcieLinkWidth width);

// An object that provides access for reading a PCI capability. It is a simple
// layer on top of a PCI region with an offset.
class PciCapability {
 public:
  static constexpr uint8_t kCapIdOffset = 0x00;
  static constexpr uint8_t kNextCapPtrOffset = 0x01;

  PciCapability(PciRegion *region, uint16_t offset)
      : region_(region), offset_(offset) {}

  // Read the raw capability ID and next pointer.
  absl::StatusOr<uint8_t> CapabilityId() const;
  absl::StatusOr<uint8_t> NextCapabilityPointer() const;

  // Construct a capability object for the next capability after this one. If
  // there are no more capability after this one a NotFoundError will be
  // returned. Other errors can be returned if the PCI read fails.
  absl::StatusOr<PciCapability> NextCapability() const;

  // Check if this capability matches the given capability type, and construct
  // and return an instance if it does. Otherwise returns a not-OK status.
  template <typename CapabilitySubtype>
  absl::StatusOr<CapabilitySubtype> GetIf() const {
    ECCLESIA_ASSIGN_OR_RETURN(uint8_t cap_id, CapabilityId());
    if (cap_id == CapabilitySubtype::kCapabilityId) {
      return CapabilitySubtype(*this);
    }
    return absl::NotFoundError("Capability is not the requested type.");
  }

 protected:
  // Helper functions that provide the same basic interface as PciRegion but
  // which apply the capability offset to all reads. Capability functions should
  // use these functions rather than accessing region_ directly.
  absl::StatusOr<uint8_t> Read8(PciRegion::OffsetType offset) const {
    return region_->Read8(offset + offset_);
  }
  absl::Status Write8(PciRegion::OffsetType offset, uint8_t data) {
    return region_->Write8(offset + offset_, data);
  }
  absl::StatusOr<uint16_t> Read16(PciRegion::OffsetType offset) const {
    return region_->Read16(offset + offset_);
  }
  absl::Status Write16(PciRegion::OffsetType offset, uint16_t data) {
    return region_->Write16(offset + offset_, data);
  }
  absl::StatusOr<uint32_t> Read32(PciRegion::OffsetType offset) const {
    return region_->Read32(offset + offset_);
  }
  absl::Status Write32(PciRegion::OffsetType offset, uint32_t data) {
    return region_->Write32(offset + offset_, data);
  }

 private:
  PciRegion *region_;
  uint8_t offset_;
};
static_assert(std::is_trivially_destructible_v<PciCapability>);

// Capability for PCI Bridge Subsystem Vendor ID.
class PciSubsystemCapability : public PciCapability {
 public:
  static constexpr uint8_t kCapabilityId = 0x0d;

  static constexpr uint8_t kSsvidOffset = 0x04;
  static constexpr uint8_t kSsidOffset = 0x06;

  explicit PciSubsystemCapability(const PciCapability &base)
      : PciCapability(base) {}

  // Read the subsystem signature.
  absl::StatusOr<PciSubsystemSignature> SubsystemSignature() const;
};
static_assert(std::is_trivially_destructible_v<PciSubsystemCapability>);

// Capability for PCI Express.
class PciExpressCapability : public PciCapability {
 public:
  static constexpr uint8_t kCapabilityId = 0x10;

  static constexpr uint8_t kPcieCapsOffset = 0x02;
  static constexpr uint8_t kDeviceCapsOffset = 0x04;
  static constexpr uint8_t kDeviceCtrlOffset = 0x08;
  static constexpr uint8_t kDeviceStatusOffset = 0x0a;
  static constexpr uint8_t kLinkCapsOffset = 0x0c;
  static constexpr uint8_t kLinkCtrlOffset = 0x10;
  static constexpr uint8_t kLinkStatusOffset = 0x12;
  static constexpr uint8_t kSlotCapsOffset = 0x14;
  static constexpr uint8_t kSlotCtrlOffset = 0x18;
  static constexpr uint8_t kSlotStatusOffset = 0x1a;
  static constexpr uint8_t kRootCtrlOffset = 0x1c;
  static constexpr uint8_t kRootCapsOffset = 0x1e;
  static constexpr uint8_t kRootStatusOffset = 0x20;
  static constexpr uint8_t kDeviceCaps2Offset = 0x24;
  static constexpr uint8_t kDeviceCtrl2Offset = 0x28;
  static constexpr uint8_t kDeviceStatus2Offset = 0x2a;
  static constexpr uint8_t kLinkCaps2Offset = 0x2c;
  static constexpr uint8_t kLinkCtrl2Offset = 0x30;
  static constexpr uint8_t kLinkStatus2Offset = 0x32;
  static constexpr uint8_t kSlotCaps2Offset = 0x34;
  static constexpr uint8_t kSlotCtrl2Offset = 0x38;
  static constexpr uint8_t kSlotStatus2Offset = 0x3a;

  explicit PciExpressCapability(const PciCapability &base)
      : PciCapability(base) {}

  // Link Capabilities Register info.

  struct LinkCapabilities {
    PcieLinkSpeed max_speed;
    PcieLinkWidth max_width;
    // The link supports data link layer active reporting.
    bool dll_active_capable;
    uint8_t port_number;
  };
  absl::StatusOr<LinkCapabilities> ReadLinkCapabilities() const;

  // Link Status Register info.
  struct LinkStatus {
    PcieLinkSpeed current_speed;
    PcieLinkWidth current_width;
    // The data link layer active bit.
    bool dll_active;
  };
  absl::StatusOr<LinkStatus> ReadLinkStatus() const;
};
static_assert(std::is_trivially_destructible_v<PciExpressCapability>);

// An object for reading and writing information from config space. It requires
// a low-level interface for actually doing raw reads and writes.
class PciConfigSpace {
 public:
  // Common Configuration Space Registers Between Type 0 And Type 1 Devices.
  static constexpr uint8_t kVidOffset = 0x00;
  static constexpr uint8_t kDidOffset = 0x02;
  static constexpr uint8_t kCommandOffset = 0x04;
  static constexpr uint8_t kStatusOffset = 0x06;
  static constexpr uint8_t kRevisionIdOffset = 0x08;
  // In PCI config space, the "class code" spans three registers with offsets
  // 0x09-0x0b, where upper byte (0x0b) is the base class code; middle byte
  // (0x0a) is the sub-class code; lower byte (0x09) is the Prog IF.
  static constexpr uint8_t kClassCodeOffset = 0x09;
  static constexpr uint8_t kCacheLineSizeOffset = 0x0c;
  static constexpr uint8_t kLatencyTimerOffset = 0x0d;
  static constexpr uint8_t kHeaderTypeOffset = 0x0e;
  static constexpr uint8_t kBistOffset = 0x0f;
  static constexpr uint8_t kCapPointerOffset = 0x34;
  static constexpr uint8_t kInterruptLineOffset = 0x3c;
  static constexpr uint8_t kInterruptPinOffset = 0x3d;
  // PCI spec defines the Configuration Space as 256 bytes, with 64 bytes
  // dedicated to the header and another 192 bytes available for a list of
  // capabilities. Each capability is 32-bit aligned, implying a max of
  // 192 / 4 = 48 possible capabilities.
  static constexpr uint8_t kMaxPciCapabilities = 48;

  // Construct a config space provided by the underlying memory region.
  explicit PciConfigSpace(PciRegion *region) : region_(region) {}

  PciRegion *Region() { return region_; }

  // Functions to look up the base and subsystem signatures.
  absl::StatusOr<PciBaseSignature> BaseSignature() const;
  absl::StatusOr<PciSubsystemSignature> SubsystemSignature() const;
  absl::StatusOr<PciSubsystemSignature> SubsystemSignature(
      uint8_t header_type) const;

  // Read the 24-bit class code.
  absl::StatusOr<uint32_t> ClassCode() const;

  // Read the header type of the config space.
  absl::StatusOr<uint8_t> HeaderType() const;

  // Perform callback on each defined device capability.
  absl::Status ForEachCapability(
      absl::FunctionRef<absl::Status(const PciCapability &)> callback) const;

  // Check if this config space matches the given header type, and construct and
  // return an instance if it does. Otherwise returns a not-OK status.
  //
  // Note that for code which is trying to handle multiple header types it is
  // more efficient to use WithSpecificType instead of calling GetIf multiple
  // times in an if-else block.
  template <typename ConfgSpaceSubtype>
  absl::StatusOr<ConfgSpaceSubtype> GetIf() const {
    absl::StatusOr<uint8_t> header_type = HeaderType();
    if (!header_type.ok()) return header_type.status();
    if (*header_type == ConfgSpaceSubtype::kHeaderType) {
      return ConfgSpaceSubtype(*this);
    }
    return absl::NotFoundError("config space does not match this type");
  }

  // Helper template that takes two functions, one that handles a type 0 header
  // and one that handles a type 1 header. This function will read the header
  // type and then execute the function that matches the actual type.
  //
  // The value type returned needs to be an explicitly specified template
  // parameter. The two functions should the return the same type that the
  // template does, or something convertible to it, and the return value will
  // be forwarded back to the caller.
  template <typename T, typename T0Func, typename T1Func>
  absl::StatusOr<T> WithSpecificType(T0Func t0_func, T1Func t1_func,
                                     uint8_t header_type) const;

  template <typename T, typename T0Func, typename T1Func>
  absl::StatusOr<T> WithSpecificType(T0Func t0_func, T1Func t1_func) const;

 protected:
  PciRegion *region_;
};
static_assert(std::is_trivially_destructible_v<PciConfigSpace>);

// Implements config space operations for type 0 (endpoint) devices.
class PciType0ConfigSpace : public PciConfigSpace {
 public:
  static constexpr uint8_t kHeaderType = 0x00;

  // Config space offsets specific to type 0 headers.
  static constexpr uint8_t kBar0Offset = 0x10;
  static constexpr uint8_t kBar1Offset = 0x14;
  static constexpr uint8_t kBar2Offset = 0x18;
  static constexpr uint8_t kBar3Offset = 0x1c;
  static constexpr uint8_t kBar4Offset = 0x20;
  static constexpr uint8_t kBar5Offset = 0x24;
  static constexpr uint8_t kCardbusCisOffset = 0x28;
  static constexpr uint8_t kSubsysVendorIdOffset = 0x2c;
  static constexpr uint8_t kSubsysIdOffset = 0x2e;
  static constexpr uint8_t kExpRomBarOffset = 0x30;
  static constexpr uint8_t kMinGntOffset = 0x3e;
  static constexpr uint8_t kMaxLatOffset = 0x3f;

 private:
  friend class PciConfigSpace;
  explicit PciType0ConfigSpace(const PciConfigSpace &base)
      : PciConfigSpace(base) {}

  // Type-specific implementations of generic functions.
  absl::StatusOr<PciSubsystemSignature> SubsystemSignatureImpl() const;
};
static_assert(std::is_trivially_destructible_v<PciType0ConfigSpace>);

// Implements config space operations for type 1 (bridge) devices.
class PciType1ConfigSpace : public PciConfigSpace {
 public:
  static constexpr uint8_t kHeaderType = 0x01;

  // Config space offsets specific to type 1 headers.
  static constexpr uint8_t kBar0Offset = 0x10;
  static constexpr uint8_t kBar1Offset = 0x14;
  static constexpr uint8_t kPriBusNumOffset = 0x18;
  static constexpr uint8_t kSecBusNumOffset = 0x19;
  static constexpr uint8_t kSubBusNumOffset = 0x1a;
  static constexpr uint8_t kSecLatTmrOffset = 0x1b;
  static constexpr uint8_t kIoBaseOffset = 0x1c;
  static constexpr uint8_t kIoLimitOffset = 0x1d;
  static constexpr uint8_t kSecStatusOffset = 0x1e;
  static constexpr uint8_t kMemBaseOffset = 0x20;
  static constexpr uint8_t kMemLimitOffset = 0x22;
  static constexpr uint8_t kPrefetchMemBaseOffset = 0x24;
  static constexpr uint8_t kPrefetchMemLimitOffset = 0x26;
  static constexpr uint8_t kPrefetchMemBaseUpperOffset = 0x28;
  static constexpr uint8_t kPrefetchMemLimitUpperOffset = 0x2c;
  static constexpr uint8_t kIoBaseUpperOffset = 0x30;
  static constexpr uint8_t kIoLimitUpperOffset = 0x32;
  static constexpr uint8_t kExpRomBarOffset = 0x38;
  static constexpr uint8_t kBridgeControlOffset = 0x3e;

  // Reports the bus numbers of the secondary and subordinate buses.
  //
  // These define the range of buses downstream of the bridge: the secondary bus
  // is the one immediately downstream of it, and the subordinate bus is the
  // largest bus number downstream of it. If subordinate > secondary this
  // implies that there are additional bridges downstream of this one.
  absl::StatusOr<PciBusNum> SecondaryBusNum() const;
  absl::StatusOr<PciBusNum> SubordinateBusNum() const;

  explicit PciType1ConfigSpace(const PciConfigSpace &base)
      : PciConfigSpace(base) {}

 private:
  friend class PciConfigSpace;

  // Type-specific implementations of generic functions.
  absl::StatusOr<PciSubsystemSignature> SubsystemSignatureImpl() const;
};
static_assert(std::is_trivially_destructible_v<PciType1ConfigSpace>);

// Definition of the WithSpecificType template. This is defined outside of the
// class definition because it references the specific subclass types which are
// defined after PciConfigSpace.
template <typename T, typename T0Func, typename T1Func>
inline absl::StatusOr<T> PciConfigSpace::WithSpecificType(
    T0Func t0_func, T1Func t1_func, uint8_t header_type) const {
  switch (header_type) {
    case PciType0ConfigSpace::kHeaderType:
      return t0_func(PciType0ConfigSpace(*this));
    case PciType1ConfigSpace::kHeaderType:
      return t1_func(PciType1ConfigSpace(*this));
    default:
      return absl::UnimplementedError("unsupported header type");
  }
}

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_IO_PCI_CONFIG_H_
