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

#include "ecclesia/lib/io/usb/sysfs.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/apifs/apifs.h"
#include "ecclesia/lib/file/path.h"
#include "ecclesia/lib/io/usb/usb.h"
#include "ecclesia/lib/status/macros.h"
#include "re2/re2.h"

namespace ecclesia {
namespace {

constexpr char kUsbDevicesDir[] = "/sys/bus/usb/devices";
// A regex pattern to match a hex string with possible trailing empty space.
constexpr LazyRE2 kHexRegexp = {R"re(([[:xdigit:]]+)\s*)re"};
constexpr LazyRE2 kDecimalRegexp = {R"re((\d+))re"};
constexpr LazyRE2 kUsbDirRegex = {"usb(\\d+)"};
constexpr LazyRE2 kUsbBusPortRegex = {"(\\d+)-(\\d+(?:\\.\\d+)*)"};

// Read a single unsigned integer out of a sysfs file. Writes the value into
// out when success. If is_hex is true then the file must contain a base-16
// integer and on false it must contain a base-10 number.
template <typename IntType>
absl::Status ReadUintFromSysfs(const ApifsFile &api_fs, bool is_hex,
                               IntType *out) {
  // Read in the contents of the file. This function only supports files
  // containing a single integer.
  ECCLESIA_ASSIGN_OR_RETURN(std::string contents, api_fs.Read());

  // The value should just be a number that we can parse and fit into an int.
  if (is_hex) {
    if (RE2::FullMatch(contents, *kHexRegexp, RE2::Hex(out))) {
      return absl::OkStatus();
    }
  } else {
    if (RE2::FullMatch(contents, *kDecimalRegexp, out)) {
      return absl::OkStatus();
    }
  }
  return absl::OutOfRangeError(absl::StrCat("sysfs file ", api_fs.GetPath(),
                                            " contained '", contents,
                                            "' and not a valid integer"));
}

// Convert a sysfs device directory name into a usb device location. Returns
// nullopt if the conversion fails.
//
// For every USB device connected to the system there will be a sysfs entry
// created based on what USB bus # the device lives on as well as the sequence
// of ports between the root controller and said device. The sysfs entry will
// follow the pattern $BUS-$PORT0.$PORT1.$PORT2...
//
// So for example a device connected directly into port 2 on root controller
// 3 will have an entry at 3-2, while a device connected into port 6 on a hub
// connected into port 3 on root controller 4 will have an entry at 4-3.6, and
// so on. Note that this sequence cannot go on indefinitely as a maximum chain
// length is set by the USB standard (6 for USB 3.0).
//
// There is one special case, the root controllers themselves. These each get
// a device entry at "usbX" where X is the number of the USB controller (i.e.
// the bus number). This is generally a symlink to the actual device entry in
// the PCI part of sysfs.
std::optional<UsbLocation> DirectoryToUsbLocation(absl::string_view dirname) {
  int bus;
  std::string port_substr;
  if (RE2::FullMatch(dirname, *kUsbDirRegex, &bus)) {
    auto usb_bus = UsbBusLocation::TryMake(bus);
    if (!usb_bus.has_value()) {
      return std::nullopt;
    }
    return UsbLocation(usb_bus.value(), UsbPortSequence());
  }
  if (RE2::FullMatch(dirname, *kUsbBusPortRegex, &bus, &port_substr)) {
    auto maybe_usb_bus = UsbBusLocation::TryMake(bus);
    if (!maybe_usb_bus.has_value()) {
      return std::nullopt;
    }

    std::vector<int> ports;
    std::vector<absl::string_view> ports_str =
        absl::StrSplit(port_substr, '.', absl::SkipEmpty());
    UsbPortSequence seq;
    for (const auto &port_str : ports_str) {
      int port;
      if (absl::SimpleAtoi(port_str, &port)) {
        if (auto maybe_usb_port = UsbPort::TryMake(port);
            maybe_usb_port.has_value()) {
          if (auto maybe_seq = seq.Downstream(maybe_usb_port.value());
              maybe_seq.has_value()) {
            seq = maybe_seq.value();
          }
        }
      } else {
        return std::nullopt;
      }
    }
    return UsbLocation(maybe_usb_bus.value(), seq);
  }
  return std::nullopt;
}
}  // namespace

std::string UsbLocationToDirectory(const UsbLocation &loc) {
  if (loc.NumPorts() == 0) {
    return absl::StrFormat("usb%d", loc.Bus().value());
  }

  std::vector<std::string> port_strings(loc.NumPorts());
  for (size_t i = 0; i < loc.NumPorts(); ++i) {
    port_strings[i] = absl::StrCat(loc.Port(i).value().value());
  }
  std::string port_substr(absl::StrJoin(port_strings, "."));
  return absl::StrFormat("%d-%s", loc.Bus().value(), port_substr.c_str());
}

SysfsUsbDiscovery::SysfsUsbDiscovery()
    : SysfsUsbDiscovery(ApifsDirectory(kUsbDevicesDir)) {}

absl::StatusOr<std::vector<UsbLocation>>
SysfsUsbDiscovery::EnumerateAllUsbDevices() const {
  ECCLESIA_ASSIGN_OR_RETURN(std::vector<std::string> usb_device_entries,
                            api_fs_.ListEntryPaths());

  std::vector<UsbLocation> devices;
  for (absl::string_view entry : usb_device_entries) {
    auto maybe_usb_location = DirectoryToUsbLocation(GetBasename(entry));

    if (maybe_usb_location.has_value()) {
      devices.push_back(maybe_usb_location.value());
    }
  }
  return devices;
}

std::unique_ptr<UsbDevice> SysfsUsbDiscovery::CreateDevice(
    const UsbLocation &location) const {
  return std::make_unique<SysfsUsbDevice>(location);
}

SysfsUsbDevice::SysfsUsbDevice(const UsbLocation &usb_location)
    : SysfsUsbDevice(usb_location, ApifsDirectory(absl::StrCat(
                                       kUsbDevicesDir, "/",
                                       UsbLocationToDirectory(usb_location)))) {
}

absl::StatusOr<UsbSignature> SysfsUsbDevice::GetSignature() const {
  UsbSignature signature;
  ECCLESIA_RETURN_IF_ERROR(ReadUintFromSysfs(ApifsFile(api_fs_, "idVendor"),
                                             true, &signature.vendor_id));
  ECCLESIA_RETURN_IF_ERROR(ReadUintFromSysfs(ApifsFile(api_fs_, "idProduct"),
                                             true, &signature.product_id));
  return signature;
}

}  // namespace ecclesia
