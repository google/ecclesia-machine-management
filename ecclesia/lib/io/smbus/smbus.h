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

// Basic read/write routines for SMBus devices.

#ifndef ECCLESIA_LIB_IO_SMBUS_SMBUS_H_
#define ECCLESIA_LIB_IO_SMBUS_SMBUS_H_

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <tuple>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "ecclesia/lib/types/fixed_range_int.h"

namespace ecclesia {

// An indentifier representing an I2C/SMBus. These are identifiers used and
// allocated by the kernel; they have no meaning at the protocol level.
class SmbusBus : public FixedRangeInteger<SmbusBus, int, 0, 255> {
 public:
  explicit constexpr SmbusBus(BaseType value) : BaseType(value) {}
};

// An I2C/SMBus address. This corresponds to the actual address on the wire.
// Uses the 7-bit form of the address.
class SmbusAddress : public FixedRangeInteger<SmbusAddress, int, 0x00, 0x7f> {
 public:
  explicit constexpr SmbusAddress(BaseType value) : BaseType(value) {}
};

// Device location information for an I2C/SMBus device.
class SmbusLocation {
 public:
  constexpr SmbusLocation(SmbusBus bus, SmbusAddress address)
      : bus_(bus), address_(address) {}

  SmbusLocation(const SmbusLocation &) = default;
  SmbusLocation &operator=(const SmbusLocation &) = default;

  // Create an SmbusLocation whose range is statically checked at compile time.
  template <int bus, int address>
  static constexpr SmbusLocation Make() {
    return SmbusLocation(SmbusBus::Make<bus>(), SmbusAddress::Make<address>());
  }

  // Create an SmbusLocation whose range is checked at run time.
  static absl::optional<SmbusLocation> TryMake(int bus, int address) {
    auto maybe_bus = SmbusBus::TryMake(bus);
    auto maybe_address = SmbusAddress::TryMake(address);
    if (!maybe_bus || !maybe_address) {
      return absl::nullopt;
    }
    return SmbusLocation(*maybe_bus, *maybe_address);
  }

  // Try to create SmbusLocation from bus+address string representation
  static absl::optional<SmbusLocation> FromString(
      absl::string_view smbus_location);

  // Accessors for reading the bus and address.
  SmbusBus bus() const { return bus_; }
  SmbusAddress address() const { return address_; }

  // Relational operators. Order is equivalent to <bus, address> tuple.
  friend bool operator==(const SmbusLocation &lhs, const SmbusLocation &rhs) {
    return std::tie(lhs.bus_, lhs.address_) == std::tie(rhs.bus_, rhs.address_);
  }
  friend bool operator!=(const SmbusLocation &lhs, const SmbusLocation &rhs) {
    return !(lhs == rhs);
  }
  friend bool operator<(const SmbusLocation &lhs, const SmbusLocation &rhs) {
    return std::tie(lhs.bus_, lhs.address_) < std::tie(rhs.bus_, rhs.address_);
  }
  friend bool operator>(const SmbusLocation &lhs, const SmbusLocation &rhs) {
    return rhs < lhs;
  }
  friend bool operator<=(const SmbusLocation &lhs, const SmbusLocation &rhs) {
    return !(rhs < lhs);
  }
  friend bool operator>=(const SmbusLocation &lhs, const SmbusLocation &rhs) {
    return !(lhs < rhs);
  }

  // Support hashing of locations.
  template <typename H>
  friend H AbslHashValue(H h, const SmbusLocation &loc) {
    return H::combine(std::move(h), loc.bus_, loc.address_);
  }

  // String conversion. This deliberately follows the bus+address format that
  // the kernel uses in sysfs.
  friend std::ostream &operator<<(std::ostream &os,
                                  const SmbusLocation &location) {
    return os << absl::StreamFormat("%d-%04x", location.bus_.value(),
                                    location.address_.value());
  }

 private:
  SmbusBus bus_;
  SmbusAddress address_;
};

// An interface to access SMBus devices.
class SmbusAccessInterface {
 public:
  SmbusAccessInterface() {}
  virtual ~SmbusAccessInterface() = default;

  // Probe for presence of a device at the requested location.
  // NOTE: This is *NOT* guaranteed to detect all types of devices.
  virtual absl::Status ProbeDevice(const SmbusLocation &loc) const = 0;

  // Sends/Receives a byte of data to/from a SMBus device.
  virtual absl::Status WriteQuick(const SmbusLocation &loc,
                                  uint8_t data) const = 0;
  virtual absl::Status SendByte(const SmbusLocation &loc,
                                uint8_t data) const = 0;
  virtual absl::Status ReceiveByte(const SmbusLocation &loc,
                                   uint8_t *data) const = 0;

  // Reads/Writes data to a SMBus device specifying the command code
  // (or register location).
  virtual absl::Status Write8(const SmbusLocation &loc, int command,
                              uint8_t data) const = 0;
  virtual absl::Status Read8(const SmbusLocation &loc, int command,
                             uint8_t *data) const = 0;
  virtual absl::Status Write16(const SmbusLocation &loc, int command,
                               uint16_t data) const = 0;
  virtual absl::Status Read16(const SmbusLocation &loc, int command,
                              uint16_t *data) const = 0;

  // Reads/Writes a block of data to a SMBus device specifying the
  // command code (or register location).
  // Args:
  //   command: command code or register location
  //   data: buffer to put/receive data
  //   len: output parameter for number of bytes read
  virtual absl::Status WriteBlockI2C(
      const SmbusLocation &loc, int command,
      absl::Span<const unsigned char> data) const = 0;
  virtual absl::Status ReadBlockI2C(const SmbusLocation &loc, int command,
                                    absl::Span<unsigned char> data,
                                    size_t *len) const = 0;
  virtual bool SupportBlockRead(const SmbusLocation &loc) const = 0;
};

// A class to encapsulate a single Smbus device at a specified location.
class SmbusDevice {
 public:
  // Create an SMBus device using the provided AccessInterface for all device
  // accesses.  The AccessInterface must not be NULL.
  SmbusDevice(SmbusLocation location, const SmbusAccessInterface *access)
      : location_(std::move(location)), access_(access) {}

  // Get a pointer to this Device's AccessInterface.
  const SmbusAccessInterface *access() const { return access_; }

  // Get this Device's address. Subclasses might generate this dynamically.
  const SmbusLocation &location() const { return location_; }

  // Using SmbusAccessInterface to access a SMBus device
  // specifying the command code (or register location).
  absl::Status WriteQuick(uint8_t data) const {
    return access()->WriteQuick(location(), data);
  }
  absl::Status SendByte(uint8_t data) const {
    return access()->SendByte(location(), data);
  }
  absl::Status ReceiveByte(uint8_t *data) const {
    return access()->ReceiveByte(location(), data);
  }
  absl::Status Write8(int command, uint8_t data) const {
    return access()->Write8(location(), command, data);
  }
  absl::Status Read8(int command, uint8_t *data) const {
    return access()->Read8(location(), command, data);
  }
  absl::Status Write16(int command, uint16_t data) const {
    return access()->Write16(location(), command, data);
  }
  absl::Status Read16(int command, uint16_t *data) const {
    return access()->Read16(location(), command, data);
  }
  bool SupportBlockRead() const {
    return access()->SupportBlockRead(location());
  }
  absl::Status WriteBlockI2C(int command,
                             absl::Span<const unsigned char> data) const {
    return access()->WriteBlockI2C(location(), command, data);
  }
  absl::Status ReadBlockI2C(int command, absl::Span<unsigned char> data,
                            size_t *len) const {
    return access()->ReadBlockI2C(location(), command, data, len);
  }

 private:
  SmbusLocation location_;
  const SmbusAccessInterface *access_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_IO_SMBUS_SMBUS_H_
