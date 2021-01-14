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

#include "ecclesia/magent/lib/io/usb_sysfs.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/types/optional.h"
#include "ecclesia/lib/apifs/apifs.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/magent/lib/io/usb.h"

namespace ecclesia {
namespace {

constexpr char kUsbDevicesDir[] = "/sys/bus/usb/devices";

class SysfsUsbTest : public testing::Test {
 protected:
  SysfsUsbTest()
      : fs_(GetTestTempdirPath()),
        usb_discover_(
            ApifsDirectory(absl::StrCat(GetTestTempdirPath(), kUsbDevicesDir))),
        usb_dir_(absl::StrCat(GetTestTempdirPath(), kUsbDevicesDir)) {
    fs_.CreateDir(kUsbDevicesDir);

    // Create some USB devices.
    CreateDevice("usb1", 0x1d6b, 0x0002, "0000:00:1a.1");
    CreateDevice("usb2", 0x1d6b, 0x0002, "0000:00:1a.2");
    CreateDevice("usb3", 0x1d6b, 0x0002, "0000:00:1d.1");
    CreateDevice("2-1", 0x0424, 0x2517, "0000:00:1a.2/usb2");
    CreateDevice("2-2", 0x0424, 0x2517, "0000:00:1a.2/usb2");
    CreateDevice("2-3", 0x0424, 0x2517, "0000:00:1a.2/usb2");
    CreateDevice("2-2.1", 0x18d1, 0x0002, "0000:00:1a.2/usb2/2-2");
    CreateDevice("2-2.4", 0x18d1, 0x0002, "0000:00:1a.2/usb2/2-2");
    CreateDevice("2-2.5", 0x18d1, 0x0002, "0000:00:1a.2/usb2/2-2");
    CreateDevice("2-2.15", 0x18d1, 0x0002, "0000:00:1a.2/usb2/2-2");
    CreateDevice("2-2.5.10", 0x18d1, 0x0002, "0000:00:1a.2/usb2/2-2");
  }

  void CreateDevice(const std::string &devname, uint16_t vendor,
                    uint16_t product, const std::string &pci) {
    std::string real_devname =
        absl::StrCat("/sys/devices/pciXXXX:XX/", pci, "/", devname);
    fs_.CreateDir(real_devname);
    fs_.CreateSymlink(real_devname, absl::StrCat(kUsbDevicesDir, "/", devname));
    fs_.CreateFile(absl::StrCat(real_devname, "/idVendor"),
                   absl::StrCat(absl::Hex(vendor, absl::kZeroPad4), "\t"));
    fs_.CreateFile(absl::StrCat(real_devname, "/idProduct"),
                   absl::StrCat(absl::Hex(product, absl::kZeroPad4)));
  }

  TestFilesystem fs_;
  SysfsUsbDiscovery usb_discover_;
  std::string usb_dir_;
};

// Helper that return true if a matching DeviceLocation in a vector, or
// returns false if the location isn't found.
bool FindUsbLocation(const std::vector<UsbLocation> &locations,
                     const UsbLocation &loc) {
  for (size_t i = 0; i < locations.size(); ++i) {
    if (loc == locations[i]) {
      return true;
    }
  }
  return false;
}

TEST_F(SysfsUsbTest, TestDeviceEnumeration) {
  auto maybe_usb_locations = usb_discover_.EnumerateAllUsbDevices();
  ASSERT_TRUE(maybe_usb_locations.ok());
  auto &usb_locations = maybe_usb_locations.value();
  EXPECT_EQ(usb_locations.size(), 11);
  auto usb_1 = UsbLocation(UsbBusLocation::Make<1>(), UsbPortSequence());
  EXPECT_TRUE(FindUsbLocation(usb_locations, usb_1));
  EXPECT_TRUE(FindUsbLocation(
      usb_locations,
      UsbLocation(UsbBusLocation::Make<2>(), UsbPortSequence())));
  EXPECT_TRUE(FindUsbLocation(
      usb_locations,
      UsbLocation(UsbBusLocation::Make<3>(), UsbPortSequence())));

  auto usb_2_1 = UsbLocation(UsbBusLocation::Make<2>(),
                             UsbPortSequence::TryMake({1}).value());
  EXPECT_TRUE(FindUsbLocation(usb_locations, usb_2_1));
  EXPECT_TRUE(FindUsbLocation(
      usb_locations, UsbLocation(UsbBusLocation::Make<2>(),
                                 UsbPortSequence::TryMake({2}).value())));
  EXPECT_TRUE(FindUsbLocation(
      usb_locations, UsbLocation(UsbBusLocation::Make<2>(),
                                 UsbPortSequence::TryMake({3}).value())));
  auto usb_2_2_1 = UsbLocation(UsbBusLocation::Make<2>(),
                               UsbPortSequence::TryMake({2, 1}).value());
  EXPECT_TRUE(FindUsbLocation(usb_locations, usb_2_2_1));
  EXPECT_TRUE(FindUsbLocation(
      usb_locations, UsbLocation(UsbBusLocation::Make<2>(),
                                 UsbPortSequence::TryMake({2, 4}).value())));
  EXPECT_TRUE(FindUsbLocation(
      usb_locations, UsbLocation(UsbBusLocation::Make<2>(),
                                 UsbPortSequence::TryMake({2, 5}).value())));
  EXPECT_TRUE(FindUsbLocation(
      usb_locations, UsbLocation(UsbBusLocation::Make<2>(),
                                 UsbPortSequence::TryMake({2, 15}).value())));
  EXPECT_TRUE(FindUsbLocation(
      usb_locations,
      UsbLocation(UsbBusLocation::Make<2>(),
                  UsbPortSequence::TryMake({2, 5, 10}).value())));

  SysfsUsbDevice usb_1_device(
      usb_1, ApifsDirectory(
                 absl::StrCat(usb_dir_, "/", UsbLocationToDirectory(usb_1))));
  auto maybe_usb_1_sig = usb_1_device.GetSignature();
  ASSERT_TRUE(maybe_usb_1_sig.ok());
  EXPECT_EQ(0x1d6b, maybe_usb_1_sig->vendor_id);
  EXPECT_EQ(0x0002, maybe_usb_1_sig->product_id);

  SysfsUsbDevice usb_2_1_device(
      usb_2_1, ApifsDirectory(absl::StrCat(usb_dir_, "/",
                                           UsbLocationToDirectory(usb_2_1))));
  auto maybe_usb_2_1_sig = usb_2_1_device.GetSignature();
  ASSERT_TRUE(maybe_usb_2_1_sig.ok());
  EXPECT_EQ(0x0424, maybe_usb_2_1_sig->vendor_id);
  EXPECT_EQ(0x2517, maybe_usb_2_1_sig->product_id);

  SysfsUsbDevice usb_2_2_1_device(
      usb_2_2_1, ApifsDirectory(absl::StrCat(
                     usb_dir_, "/", UsbLocationToDirectory(usb_2_2_1))));
  auto maybe_usb_2_2_1_sig = usb_2_2_1_device.GetSignature();
  ASSERT_TRUE(maybe_usb_2_2_1_sig.ok());
  EXPECT_EQ(0x18d1, maybe_usb_2_2_1_sig->vendor_id);
  EXPECT_EQ(0x0002, maybe_usb_2_2_1_sig->product_id);
}

}  // namespace
}  // namespace ecclesia
