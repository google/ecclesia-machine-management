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

#ifndef ECCLESIA_LIB_IO_USB_SYSFS_H_
#define ECCLESIA_LIB_IO_USB_SYSFS_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "ecclesia/lib/apifs/apifs.h"
#include "ecclesia/lib/io/usb/usb.h"

namespace ecclesia {

// Convert a usb device location to the sysfs directory name used for that
// device.
std::string UsbLocationToDirectory(const UsbLocation &loc);

class SysfsUsbDiscovery : public UsbDiscoveryInterface {
 public:
  SysfsUsbDiscovery();
  // This constructor is for testing purpose.
  explicit SysfsUsbDiscovery(const ApifsDirectory &api_fs) : api_fs_(api_fs) {}

  SysfsUsbDiscovery(const SysfsUsbDiscovery &other) = delete;
  SysfsUsbDiscovery &operator=(const SysfsUsbDiscovery &other) = delete;

  absl::StatusOr<std::vector<UsbLocation>> EnumerateAllUsbDevices()
      const override;

  std::unique_ptr<UsbDevice> CreateDevice(
      const UsbLocation &location) const override;

 private:
  ApifsDirectory api_fs_;
};

class SysfsUsbDevice : public UsbDevice {
 public:
  SysfsUsbDevice(const UsbLocation &usb_location);

  // This constructor is for testing purpose.
  SysfsUsbDevice(const UsbLocation &usb_location, const ApifsDirectory &api_fs)
      : usb_location_(usb_location), api_fs_(api_fs) {}

  SysfsUsbDevice(const SysfsUsbDevice &other) = delete;
  SysfsUsbDevice &operator=(const SysfsUsbDevice &other) = delete;

  const UsbLocation &Location() const override { return usb_location_; }

  absl::StatusOr<UsbSignature> GetSignature() const override;

 private:
  UsbLocation usb_location_;
  ApifsDirectory api_fs_;
};
}  // namespace ecclesia

#endif  // ECCLESIA_LIB_IO_USB_SYSFS_H_
