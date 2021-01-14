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

#include "ecclesia/magent/sysmodel/x86/nvme.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ecclesia/lib/file/path.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/io/pci/location.h"
#include "ecclesia/magent/lib/nvme/identify_controller.h"
#include "ecclesia/magent/lib/nvme/mock_nvme_device.h"
#include "ecclesia/magent/lib/nvme/nvme_device.h"
#include "ecclesia/magent/lib/nvme/smart_log_page.h"

namespace ecclesia {
namespace {

TEST(NvmeInterfaceTest, CreateFromPciSuccess) {
  std::string root_path = GetTestTempdirPath();
  TestFilesystem fs(root_path);
  auto pci_location = PciLocation::Make<0, 0xae, 3, 4>();
  std::string nvme_dev_name = "nvme2";
  fs.CreateDir(
      JoinFilePaths("/sys/bus/pci/devices", pci_location.ToString(), "nvme"));
  fs.CreateDir(JoinFilePaths("/sys/bus/pci/devices", pci_location.ToString(),
                             "nvme", nvme_dev_name));
  fs.CreateDir(JoinFilePaths("/mnt/devtmpfs", nvme_dev_name));

  auto maybe_nvme_intf = NvmeInterface::CreateFromPci(pci_location, root_path);
  ASSERT_TRUE(maybe_nvme_intf.ok());
  std::unique_ptr<NvmeInterface> nvme_intf = std::move(maybe_nvme_intf.value());
  ASSERT_NE(nvme_intf, nullptr);
  EXPECT_EQ(nvme_intf->GetKernelName(), nvme_dev_name);
}

TEST(NvmeInterfaceTest, CreateFromPciFailureNoDevName) {
  std::string root_path = GetTestTempdirPath();
  TestFilesystem fs(root_path);
  auto pci_location = PciLocation::Make<0, 0xae, 3, 4>();
  fs.CreateDir(
      JoinFilePaths("/sys/bus/pci/devices", pci_location.ToString(), "nvme"));

  auto maybe_nvme_intf = NvmeInterface::CreateFromPci(pci_location, root_path);
  EXPECT_FALSE(maybe_nvme_intf.ok());

  // The failure is due to no entry in the directory
  // /sys/bus/pci/devices/<pci_location>/nvme/
  EXPECT_EQ(maybe_nvme_intf.status().code(), absl::StatusCode::kNotFound);
}

TEST(NvmeInterfaceTest, CreateFromPciFailureNoDevNode) {
  std::string root_path = GetTestTempdirPath();
  TestFilesystem fs(root_path);
  auto pci_location = PciLocation::Make<0, 0xae, 3, 4>();
  std::string nvme_dev_name = "nvme2";
  fs.CreateDir(
      JoinFilePaths("/sys/bus/pci/devices", pci_location.ToString(), "nvme"));
  fs.CreateDir(JoinFilePaths("/sys/bus/pci/devices", pci_location.ToString(),
                             "nvme", nvme_dev_name));

  auto maybe_nvme_intf = NvmeInterface::CreateFromPci(pci_location, root_path);
  EXPECT_FALSE(maybe_nvme_intf.ok());

  // The failure is due to the absence of node /dev/nvme2 and
  // /mnt/devtmpfs/nvme2
  EXPECT_EQ(maybe_nvme_intf.status().code(), absl::StatusCode::kNotFound);
}

TEST(NvmeInterfaceTest, VerifyInterfaceMethods) {
  auto nvme_device = std::make_unique<MockNvmeDevice>();
  auto* nvme_device_ptr = nvme_device.get();

  // Create IdentifyController and SmartLogPage instances. The fix-length input
  // strings are to make sure the created objects are not nullptr.
  auto identify_controller = IdentifyController::Parse(std::string(4096, ' '));
  EXPECT_CALL(*nvme_device_ptr, Identify).WillOnce([&]() {
    return std::move(identify_controller);
  });
  auto smart_log_page = SmartLogPage::Parse(std::string(512, ' '));
  EXPECT_CALL(*nvme_device_ptr, SmartLog).WillOnce([&]() {
    return std::move(smart_log_page);
  });

  NvmeInterface nvme_intf(std::move(nvme_device), "nvme123");
  EXPECT_EQ(nvme_intf.GetKernelName(), "nvme123");

  auto id_controller = nvme_intf.Identify();
  EXPECT_TRUE(id_controller.ok());
  EXPECT_NE(id_controller.value(), nullptr);

  auto smart_log = nvme_intf.SmartLog();
  EXPECT_TRUE(smart_log.ok());
  EXPECT_NE(smart_log.value(), nullptr);
}

class TestNvmeDiscover : public NvmeDiscoverInterface {
 public:
  TestNvmeDiscover(std::vector<NvmeLocation> nvme_locations)
      : nvme_locations_(std::move(nvme_locations)) {}

  absl::StatusOr<std::vector<NvmeLocation>> GetAllNvmeLocations()
      const override {
    return nvme_locations_;
  }

 private:
  std::vector<NvmeLocation> nvme_locations_;
};

TEST(NvmeDiscoverInterfaceTest, GetAllNvmePlugins) {
  std::string root_path = GetTestTempdirPath();
  TestFilesystem fs(root_path);
  auto pci_location = PciLocation::Make<0, 0xae, 3, 4>();
  std::string nvme_dev_name = "nvme2";
  fs.CreateDir(
      JoinFilePaths("/sys/bus/pci/devices", pci_location.ToString(), "nvme"));
  fs.CreateDir(JoinFilePaths("/sys/bus/pci/devices", pci_location.ToString(),
                             "nvme", nvme_dev_name));
  fs.CreateDir(JoinFilePaths("/mnt/devtmpfs", nvme_dev_name));

  std::vector<NvmeLocation> nvme_locations = {
      NvmeLocation{pci_location, "U2_1"}};

  TestNvmeDiscover nvme_discover(std::move(nvme_locations));
  auto maybe_nvme_plugins =
      nvme_discover.GetAllNvmePlugins(GetTestTempdirPath());
  ASSERT_TRUE(maybe_nvme_plugins.ok());
  const std::vector<NvmePlugin>& nvme_plugins = maybe_nvme_plugins.value();
  ASSERT_EQ(nvme_plugins.size(), 1);
  EXPECT_EQ(nvme_plugins.at(0).location.pci_location, pci_location);
  EXPECT_EQ(nvme_plugins.at(0).location.physical_location, "U2_1");
  EXPECT_NE(nvme_plugins.at(0).access_intf, nullptr);
}

}  // namespace
}  // namespace ecclesia
