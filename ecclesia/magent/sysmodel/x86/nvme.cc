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
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/apifs/apifs.h"
#include "ecclesia/lib/file/path.h"
#include "ecclesia/lib/io/pci/location.h"
#include "ecclesia/magent/lib/nvme/nvme_linux_access.h"

namespace ecclesia {

namespace {

constexpr char kNvme[] = "nvme";

// The following paths are relative paths to the root directory "/".
constexpr char kPciDeviceDirPath[] = "sys/bus/pci/devices";

constexpr char kRootDevPath[] = "dev";

constexpr char kMntNvmeDirPath[] = "mnt/devtmpfs";

}  // namespace

absl::StatusOr<std::unique_ptr<NvmeInterface>> NvmeInterface::CreateFromPci(
    const PciDbdfLocation &pci_location, absl::string_view root_path) {
  // Firstly, find the devname in
  // /sys/bus/pci/devices/<pci_location>/nvme/<devname>
  std::string pci_nvme_path =
      JoinFilePaths(std::string(root_path), kPciDeviceDirPath,
                    pci_location.ToString(), kNvme);
  std::string devname;
  if (auto api_fs = ApifsDirectory(pci_nvme_path); api_fs.Exists()) {
    auto maybe_entries = api_fs.ListEntryNames();
    // There expects to be only one entry under
    // /sys/bus/pci/devices/<pci_location>/nvme/. And we use that as this device
    // name.
    if (maybe_entries.ok() && maybe_entries.value().size() == 1) {
      devname = maybe_entries.value().at(0);
    }
  }
  if (devname.empty()) {
    return absl::NotFoundError(
        absl::StrCat("Failed to find the devname for NVMe with PCI location: ",
                     pci_location.ToString()));
  }

  // Secondly, find the corresponding NVMe device node.
  // Check the /dev/<devname>
  std::string nvme_dev_path = JoinFilePaths(root_path, kRootDevPath, devname);
  if (auto api_fs = ApifsDirectory(nvme_dev_path); api_fs.Exists()) {
    return std::make_unique<NvmeInterface>(CreateNvmeLinuxDevice(nvme_dev_path),
                                           devname);
  }
  // Check the /mnt/devtmpfs/<devname>
  nvme_dev_path = JoinFilePaths(root_path, kMntNvmeDirPath, devname);
  if (auto api_fs = ApifsDirectory(nvme_dev_path); api_fs.Exists()) {
    return std::make_unique<NvmeInterface>(CreateNvmeLinuxDevice(nvme_dev_path),
                                           devname);
  }
  return absl::NotFoundError(absl::StrCat(
      "Failed to find the device node for NVMe with devname: ", devname));
}

}  // namespace ecclesia
