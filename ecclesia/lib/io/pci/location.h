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

// Defines a basic location type for PCI devices. Corresponds to a standard
// domain-bus-device-function identifier.

#ifndef ECCLESIA_LIB_IO_PCI_LOCATION_H_
#define ECCLESIA_LIB_IO_PCI_LOCATION_H_

#include <iosfwd>
#include <string>
#include <tuple>
#include <utility>

#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "ecclesia/lib/types/fixed_range_int.h"

namespace ecclesia {

// An identifier representing the PCI domain. This is an identifier produced by
// the kernel; they have no meaning at the protocol level.
class PciDomain : public FixedRangeInteger<PciDomain, int, 0, 0xffff> {
 public:
  explicit constexpr PciDomain(BaseType value) : BaseType(value) {}
};

// Number of the bus the device is attached to. An 8-bit value.
class PciBusNum : public FixedRangeInteger<PciBusNum, int, 0, 0xff> {
 public:
  explicit constexpr PciBusNum(BaseType value) : BaseType(value) {}
};

// The device number and function number of an object on the bus. These two
// numbers are a 5-bit and 3-bit value. In traditional PCI there was a logical
// distinction between physical devices and logical functions; in PCI-Express
// there is no significant difference and they combine to form an 8-bit address
// space for logical functions on the bus.
class PciDeviceNum : public FixedRangeInteger<PciDeviceNum, int, 0, 0x1f> {
 public:
  explicit constexpr PciDeviceNum(BaseType value) : BaseType(value) {}
};
class PciFunctionNum : public FixedRangeInteger<PciFunctionNum, int, 0, 0x7> {
 public:
  explicit constexpr PciFunctionNum(BaseType value) : BaseType(value) {}
};

// Device location information for a PCI or PCI Express device.
class PciLocation {
 public:
  constexpr PciLocation(PciDomain domain, PciBusNum bus, PciDeviceNum device,
                        PciFunctionNum function)
      : domain_(domain), bus_(bus), device_(device), function_(function) {}

  PciLocation(const PciLocation &) = default;
  PciLocation &operator=(const PciLocation &) = default;

  // Create a PciLocation whose range is statically checked at compile time.
  template <int domain, int bus, int device, int function>
  static constexpr PciLocation Make() {
    return PciLocation(PciDomain::Make<domain>(), PciBusNum::Make<bus>(),
                       PciDeviceNum::Make<device>(),
                       PciFunctionNum::Make<function>());
  }

  // Create a PciLocation whose range is checked at run time.
  static absl::optional<PciLocation> TryMake(int domain, int bus, int device,
                                             int function) {
    auto maybe_domain = PciDomain::TryMake(domain);
    auto maybe_bus = PciBusNum::TryMake(bus);
    auto maybe_device = PciDeviceNum::TryMake(device);
    auto maybe_function = PciFunctionNum::TryMake(function);

    if (!maybe_domain.has_value() || !maybe_bus.has_value() ||
        !maybe_device.has_value() || !maybe_function.has_value()) {
      return absl::nullopt;
    }

    return PciLocation(maybe_domain.value(), maybe_bus.value(),
                       maybe_device.value(), maybe_function.value());
  }

  std::string ToString() const {
    return absl::StrFormat("%04x:%02x:%02x.%x", domain_.value(), bus_.value(),
                           device_.value(), function_.value());
  }

  const PciDomain domain() const { return domain_; }
  const PciBusNum bus() const { return bus_; }
  const PciDeviceNum device() const { return device_; }
  const PciFunctionNum function() const { return function_; }

  // Convert a string to PciLocation in runtime.
  // string format: 0000:17:08.2
  static absl::optional<PciLocation> FromString(absl::string_view dev_str);

  // PciLocation relational operators.
  // Order is equivalent to that of a <domain, bus, device, function> tuple.
  friend bool operator==(const PciLocation &lhs, const PciLocation &rhs) {
    return std::tie(lhs.domain_, lhs.bus_, lhs.device_, lhs.function_) ==
           std::tie(rhs.domain_, rhs.bus_, rhs.device_, rhs.function_);
  }
  friend bool operator!=(const PciLocation &lhs, const PciLocation &rhs) {
    return !(lhs == rhs);
  }
  friend bool operator<(const PciLocation &lhs, const PciLocation &rhs) {
    return std::tie(lhs.domain_, lhs.bus_, lhs.device_, lhs.function_) <
           std::tie(rhs.domain_, rhs.bus_, rhs.device_, rhs.function_);
  }
  friend bool operator>(const PciLocation &lhs, const PciLocation &rhs) {
    return (rhs < lhs);
  }
  friend bool operator<=(const PciLocation &lhs, const PciLocation &rhs) {
    return !(rhs < lhs);
  }
  friend bool operator>=(const PciLocation &lhs, const PciLocation &rhs) {
    return !(lhs < rhs);
  }

  // Support hashing of locations for use as a key in hash maps.
  template <typename H>
  friend H AbslHashValue(H h, const PciLocation &loc) {
    return H::combine(std::move(h), loc.domain_, loc.bus_, loc.device_,
                      loc.function_);
  }

  // String conversion. This deliberately follows the
  // domain:bus:device.function format that the kernel uses in sysfs.
  friend std::ostream &operator<<(std::ostream &os,
                                  const PciLocation &location) {
    return os << absl::StreamFormat(
               "%04x:%02x:%02x.%x", location.domain_.value(),
               location.bus_.value(), location.device_.value(),
               location.function_.value());
  }

 private:
  PciDomain domain_;
  PciBusNum bus_;
  PciDeviceNum device_;
  PciFunctionNum function_;
};

// A wrapper class with PCI Domain, Bus and Device numbers. A PCI device
// normally corresponds to a set of <domain>:<bus>:<device> but may have
// multiple <func>. Thus, there can bee multiple PciLocations
// (<domain>:<bus>:<device>.<func>) associated one PCI device. This class is
// used to facilitate <domain>:<bus>:<device> comparators and group those
// PciLocations from the same PCIe device into a single one.
class PciDeviceLocation {
 public:
  PciDeviceLocation(const PciLocation &location)
      : domain_(location.domain()),
        bus_(location.bus()),
        device_(location.device()) {}

  friend bool operator==(const PciDeviceLocation &lhs,
                         const PciDeviceLocation &rhs) {
    return std::tie(lhs.domain_, lhs.bus_, lhs.device_) ==
           std::tie(rhs.domain_, rhs.bus_, rhs.device_);
  }
  friend bool operator!=(const PciDeviceLocation &lhs,
                         const PciDeviceLocation &rhs) {
    return !(lhs == rhs);
  }

  // Support hashing of <domain>:<bus>:<device> for use as a key in hash maps.
  template <typename H>
  friend H AbslHashValue(H h, const PciDeviceLocation &dev_id) {
    return H::combine(std::move(h), dev_id.domain_, dev_id.bus_,
                      dev_id.device_);
  }

  // The "<domain>:<bus>:<device>" is used as the unique ID/name for this PCI
  // device.
  std::string ToString() const {
    return absl::StrFormat("%04x:%02x:%02x", domain_.value(), bus_.value(),
                           device_.value());
  }

 private:
  PciDomain domain_;
  PciBusNum bus_;
  PciDeviceNum device_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_IO_PCI_LOCATION_H_
