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

// Provides classes to represent the "signature" of a PCI device in terms of its
// vendor, device and subsystems IDs.
//
// These classes all follow the same basic pattern: they're composed of some
// number of ID numbers, they have Make and TryMake functions that can construct
// them with static or dynamic range checks on a tuple of integers, and they
// support basic equality comparisons. They don't support relational comparisons
// as it doesn't make sense to think of values as being ordered.

#ifndef ECCLESIA_LIB_IO_PCI_SIGNATURE_H_
#define ECCLESIA_LIB_IO_PCI_SIGNATURE_H_

#include <tuple>

#include "absl/types/optional.h"
#include "ecclesia/lib/types/fixed_range_int.h"

namespace ecclesia {

// Representation of a PCI "ID number". All of the IDs used by PCI are 16-bit
// values so this single type should cover all of them.
//
// Technically we could just use a uint16_t instead of this more complex type
// but this provides more consistency with the values used with location types
// and it has useful existing functions for converting from arbitrary ints.
class PciIdNum : public FixedRangeInteger<PciIdNum, int, 0, 0xffff> {
 public:
  explicit constexpr PciIdNum(BaseType value) : BaseType(value) {}
};

// Basic device signature, the Vendor ID and Device ID of a device.
class PciBaseSignature {
 public:
  constexpr PciBaseSignature(PciIdNum vendor_id, PciIdNum device_id)
      : vendor_id_(vendor_id), device_id_(device_id) {}

  PciBaseSignature(const PciBaseSignature &) = default;
  PciBaseSignature &operator=(const PciBaseSignature &) = default;

  template <int vid, int did>
  static constexpr PciBaseSignature Make() {
    return PciBaseSignature(PciIdNum::Make<vid>(), PciIdNum::Make<did>());
  }

  static absl::optional<PciBaseSignature> TryMake(int vid, int did) {
    auto maybe_vendor_id = PciIdNum::TryMake(vid);
    auto maybe_device_id = PciIdNum::TryMake(did);

    if (!maybe_vendor_id || !maybe_device_id) {
      return absl::nullopt;
    }

    return PciBaseSignature(*maybe_vendor_id, *maybe_device_id);
  }

  const PciIdNum vendor_id() const { return vendor_id_; }
  const PciIdNum device_id() const { return device_id_; }

  friend bool operator==(const PciBaseSignature &lhs,
                         const PciBaseSignature &rhs) {
    return std::tie(lhs.vendor_id_, lhs.device_id_) ==
           std::tie(rhs.vendor_id_, rhs.device_id_);
  }
  friend bool operator!=(const PciBaseSignature &lhs,
                         const PciBaseSignature &rhs) {
    return !(lhs == rhs);
  }

 private:
  PciIdNum vendor_id_;
  PciIdNum device_id_;
};

// Subsystem signature. Similar to, but separate from the base ID. On bridges
// this is not actually from the base config, but from a capability.
class PciSubsystemSignature {
 public:
  constexpr PciSubsystemSignature(PciIdNum vendor_id, PciIdNum id)
      : vendor_id_(vendor_id), id_(id) {}

  PciSubsystemSignature(const PciSubsystemSignature &) = default;
  PciSubsystemSignature &operator=(const PciSubsystemSignature &) = default;

  template <int ssvid, int ssid>
  static constexpr PciSubsystemSignature Make() {
    return PciSubsystemSignature(PciIdNum::Make<ssvid>(),
                                 PciIdNum::Make<ssid>());
  }

  static absl::optional<PciSubsystemSignature> TryMake(int ssvid, int ssid) {
    auto maybe_vendor_id = PciIdNum::TryMake(ssvid);
    auto maybe_id = PciIdNum::TryMake(ssid);

    if (!maybe_vendor_id || !maybe_id) {
      return absl::nullopt;
    }

    return PciSubsystemSignature(*maybe_vendor_id, *maybe_id);
  }

  const PciIdNum vendor_id() const { return vendor_id_; }
  const PciIdNum id() const { return id_; }

  friend bool operator==(const PciSubsystemSignature &lhs,
                         const PciSubsystemSignature &rhs) {
    return std::tie(lhs.vendor_id_, lhs.id_) ==
           std::tie(rhs.vendor_id_, rhs.id_);
  }
  friend bool operator!=(const PciSubsystemSignature &lhs,
                         const PciSubsystemSignature &rhs) {
    return !(lhs == rhs);
  }

 private:
  PciIdNum vendor_id_;
  PciIdNum id_;
};

// Full signature that combines the base and subsystem IDs. This is usually what
// is used to fully identify a specific PCI device.
class PciFullSignature {
 public:
  constexpr PciFullSignature(PciBaseSignature base,
                             PciSubsystemSignature subsystem)
      : base_(base), subsystem_(subsystem) {}

  PciFullSignature(const PciFullSignature &) = default;
  PciFullSignature &operator=(const PciFullSignature &) = default;

  template <int vid, int did, int ssvid, int ssid>
  static constexpr PciFullSignature Make() {
    return PciFullSignature(PciBaseSignature::Make<vid, did>(),
                            PciSubsystemSignature::Make<ssvid, ssid>());
  }

  static absl::optional<PciFullSignature> TryMake(int vid, int did, int ssvid,
                                                  int ssid) {
    auto maybe_base = PciBaseSignature::TryMake(vid, did);
    auto maybe_subsystem = PciSubsystemSignature::TryMake(ssvid, ssid);

    if (!maybe_base || !maybe_subsystem) {
      return absl::nullopt;
    }

    return PciFullSignature(*maybe_base, *maybe_subsystem);
  }

  const PciBaseSignature base() const { return base_; }
  const PciSubsystemSignature subsystem() const { return subsystem_; }

  friend bool operator==(const PciFullSignature &lhs,
                         const PciFullSignature &rhs) {
    return std::tie(lhs.base_, lhs.subsystem_) ==
           std::tie(rhs.base_, rhs.subsystem_);
  }
  friend bool operator!=(const PciFullSignature &lhs,
                         const PciFullSignature &rhs) {
    return !(lhs == rhs);
  }

 private:
  PciBaseSignature base_;
  PciSubsystemSignature subsystem_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_IO_PCI_SIGNATURE_H_
