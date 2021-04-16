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

// The library provides classes for constructing a generic representation of a
// PCI devices, managing the underlying low-level configuration space and
// resources.

#ifndef ECCLESIA_LIB_IO_PCI_PCI_H_
#define ECCLESIA_LIB_IO_PCI_PCI_H_

#include <cstdint>
#include <memory>
#include <utility>

#include "absl/status/statusor.h"
#include "ecclesia/lib/io/pci/config.h"
#include "ecclesia/lib/io/pci/location.h"
#include "ecclesia/lib/io/pci/region.h"
#include "ecclesia/lib/types/fixed_range_int.h"

namespace ecclesia {

// PCIe link capability information.
struct PcieLinkCapabilities {
  PcieLinkSpeed speed = PcieLinkSpeed::kUnknown;
  PcieLinkWidth width = PcieLinkWidth::kUnknown;
};

class PciResources {
 public:
  // Type of address used in a BAR.
  enum BarType { kBarTypeMem, kBarTypeIo };

  // An identifier representing a BAR ID.
  class BarNum : public FixedRangeInteger<BarNum, int, 0, 5> {
   public:
    explicit constexpr BarNum(BaseType value) : BaseType(value) {}
  };

  explicit PciResources(PciDbdfLocation loc) : loc_(std::move(loc)) {}
  virtual ~PciResources() = default;

  // Check if Pci exists.
  virtual bool Exists() = 0;

  // Get information about a BAR.
  struct BarInfo {
    BarType type;
    uint64_t address;
  };
  absl::StatusOr<BarInfo> GetBaseAddress(BarNum bar_id) const {
    return GetBaseAddressImpl(bar_id);
  }
  // Templated version for when the BAR number is known at compile time.
  template <int BarId>
  absl::StatusOr<BarInfo> GetBaseAddress() const {
    return GetBaseAddressImpl(BarNum::Make<BarId>());
  }

 protected:
  PciDbdfLocation loc_;

 private:
  // The underlying implementation of GetBaseAddress.
  virtual absl::StatusOr<BarInfo> GetBaseAddressImpl(BarNum bar_id) const = 0;
};

// A wrappper class for interacting with a PCI device.
class PciDevice {
 public:
  // Create a PCI device using the provided region for all config space access.
  PciDevice(const PciDbdfLocation &location,
            std::unique_ptr<PciRegion> config_region,
            std::unique_ptr<PciResources> resources_intf)
      : location_(location),
        config_region_(std::move(config_region)),
        config_space_(std::make_unique<PciConfigSpace>(config_region_.get())),
        resources_intf_(std::move(resources_intf)) {}

  virtual ~PciDevice() = default;

  PciDevice(const PciDevice &) = delete;
  PciDevice &operator=(const PciDevice &) = delete;

  PciDevice(PciDevice &&) = default;
  PciDevice &operator=(PciDevice &&) = default;

  // Get this device address.
  const PciDbdfLocation &Location() const { return location_; }

  // Get the device config space. The virtual declaration is to facilitate mock
  // up for testing.
  virtual PciConfigSpace *ConfigSpace() const { return config_space_.get(); }

  // Get the device resource information.
  PciResources &Resources() { return *resources_intf_; }
  const PciResources &Resources() const { return *resources_intf_; }

  // These methods are commonly used for checking the PCIe link status.
  virtual absl::StatusOr<PcieLinkCapabilities> PcieLinkMaxCapabilities()
      const = 0;

  virtual absl::StatusOr<PcieLinkCapabilities> PcieLinkCurrentCapabilities()
      const = 0;

 private:
  PciDbdfLocation location_;

  std::unique_ptr<PciRegion> config_region_;
  std::unique_ptr<PciConfigSpace> config_space_;

  std::unique_ptr<PciResources> resources_intf_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_IO_PCI_PCI_H_
