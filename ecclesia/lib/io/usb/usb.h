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

// An interface for discovering USB devices.

#ifndef ECCLESIA_LIB_IO_USB_USB_H_
#define ECCLESIA_LIB_IO_USB_USB_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <tuple>
#include <type_traits>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "ecclesia/lib/types/fixed_range_int.h"

namespace ecclesia {
// Device signature information for a USB device.
struct UsbSignature {
  // Manufacturer of the device.
  uint16_t vendor_id;
  // Product identifier as allocated by the manufacturer.
  uint16_t product_id;

  bool operator==(const UsbSignature &other) const {
    return std::tie(vendor_id, product_id) ==
           std::tie(other.vendor_id, other.product_id);
  }
  bool operator!=(const UsbSignature &other) const { return !(*this == other); }
};

// The port number a USB device is connected to.
class UsbPort : public FixedRangeInteger<UsbPort, int, 1, 15> {
 public:
  explicit constexpr UsbPort(BaseType value) : BaseType(value) {}
};

// Class that describes the sequence of ports a USB device is connected to.
// Each device attached downstream of a USB root controller can be uniquely
// identified by the sequence of port numbers it's attached through.
//
// In the USB standard a single hub (or root controller) can have up to 15
// downstream ports, numbered 1-15. An endpoint device can have as many as six
// hubs/controllers upstream from it. Thus, we represent the sequence of ports
// as an array of 0-6 integers.
class UsbPortSequence {
 public:
  // The maximum length of a chain of USB devices (through hubs).
  static constexpr int kDeviceChainMaxLength = 6;

 private:
  // Define a storage type that can hold UsbPort values but which can also be
  // default initialized. This type stores the port in a union where by default
  // the empty element is constructed. It is undefined behavior to access the
  // port value without explicitly initializing it first, but since port
  // sequence tracks how many elements in its underlying array have been
  // populated it will prevent uninitialized elements from being accessed.
  struct EmptyStruct {};
  struct UsbPortStorage {
    constexpr UsbPortStorage() : empty({}) {}
    explicit constexpr UsbPortStorage(UsbPort port) : value(port) {}

    union {
      UsbPort value;
      EmptyStruct empty;
    };
  };
  // Make sure UsbPort can be trivially destroyed. Because UsbPortStorage
  // disables the UsbPort destructor we rely on the fact that it has a trivial
  // destructor (and thus is allowed to be omitted when the storage is
  // discarded). If this is not true then we would need to explicitly destroy
  // the stored values.
  static_assert(std::is_trivially_destructible_v<UsbPort>);

  // The underlying array storage type for the port sequence.
  using StoredArray = std::array<UsbPortStorage, kDeviceChainMaxLength>;

 public:
  // Default construtor that creates an empty sequence. Equivalent to
  // UsbPortSequence::Make<>();
  constexpr UsbPortSequence() : size_(0) {}

  // Compile-time factory function. Enforces the input range and length.
  template <UsbPort::StoredType... Values>
  static constexpr UsbPortSequence Make() {
    static_assert(sizeof...(Values) <= kDeviceChainMaxLength,
                  "port sequence length exceeds the allowed maximum");
    StoredArray ports = {UsbPortStorage(UsbPort::Make<Values>())...};
    return UsbPortSequence(ports, sizeof...(Values));
  }

  // Run-time factory function. Returns nullopt if the given value is out of
  // range, either due to out of range values or too many ports.
  static absl::optional<UsbPortSequence> TryMake(absl::Span<const int> ports);

  UsbPortSequence(const UsbPortSequence &other) = default;
  UsbPortSequence &operator=(const UsbPortSequence &other) = default;

  // Determine the size of the entire sequence;
  size_t Size() const;

  // Fetch the port at a particular position in the sequence. Returns nullopt if
  // the index is out of range.
  absl::optional<UsbPort> Port(size_t index) const;

  // Compute the port sequence for a particular port downstream of this
  // sequence. Returns nullopt if the sequence is already at the maximum length.
  absl::optional<UsbPortSequence> Downstream(UsbPort port) const;

  friend bool operator==(const UsbPortSequence &lhs,
                         const UsbPortSequence &rhs);
  friend bool operator!=(const UsbPortSequence &lhs,
                         const UsbPortSequence &rhs);

 private:
  // Unchecked constructor.
  constexpr UsbPortSequence(const StoredArray &ports, size_t size)
      : ports_(ports), size_(size) {}

  // The underlying ports array, and how many of the entries in the array are
  // populated. Values in the array after ports_[size_-1] are uninitialized and
  // it would be undefined behavior to access them.
  StoredArray ports_;
  size_t size_;
};

// This is not a hard limit from any specification, but we're unlikely to
// encounter a system with more than a couple hundred USB host controllers.
class UsbBusLocation : public FixedRangeInteger<UsbBusLocation, int, 1, 255> {
 public:
  explicit constexpr UsbBusLocation(BaseType value) : BaseType(value) {}
};

// Device location information for a USB device. This class has value semantics.
// We can uniquely represent a USB device attached to a system by the bus
// number it lives on plus the sequence of ports you follow to reach it from
// the root controller.
class UsbLocation {
 public:
  constexpr UsbLocation(UsbBusLocation bus, const UsbPortSequence &ports)
      : bus_(bus), ports_(ports) {}

  // Construt a USB location that is statically constructed a compile time. The
  // first parameter is the bus location, and any additional parameters are the
  // port sequence.
  template <UsbBusLocation::StoredType Bus, UsbPort::StoredType... Ports>
  static constexpr UsbLocation Make() {
    return UsbLocation(UsbBusLocation::Make<Bus>(),
                       UsbPortSequence::Make<Ports...>());
  }

  // Read the bus and port numbers.
  UsbBusLocation Bus() const { return bus_; }
  size_t NumPorts() const { return ports_.Size(); }
  absl::optional<UsbPort> Port(size_t index) const {
    return ports_.Port(index);
  }

  friend bool operator==(const UsbLocation &lhs, const UsbLocation &rhs);
  friend bool operator!=(const UsbLocation &lhs, const UsbLocation &rhs);

 private:
  // The number of the bus behind a single controller. 1-255.
  UsbBusLocation bus_;
  // The sequence of ports that the device is connected behind.
  UsbPortSequence ports_;
};

// An interface to access a USB device.
class UsbDevice {
 public:
  virtual ~UsbDevice() = default;

  virtual const UsbLocation &Location() const = 0;

  virtual absl::StatusOr<UsbSignature> GetSignature() const = 0;
};

// An interface to discover USB devices.
class UsbDiscoveryInterface {
 public:
  virtual ~UsbDiscoveryInterface() = default;

  // Enumerate all USB devices.
  virtual absl::StatusOr<std::vector<UsbLocation>> EnumerateAllUsbDevices()
      const = 0;

  // A method to convert a USB location to a USB device instance. A normal
  // useage is enumerating USB locations and then use this method to create a
  // USB device for accessing its functionalities.
  virtual std::unique_ptr<UsbDevice> CreateDevice(
      const UsbLocation &location) const = 0;
};

// A helper function for getting the USB device location with the given
// signature, or status if not found.
absl::StatusOr<UsbLocation> FindUsbDeviceWithSignature(
    const UsbDiscoveryInterface *usb_intf, const UsbSignature &usb_signature);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_IO_USB_USB_H_
