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

#ifndef ECCLESIA_MAGENT_SYSMODEL_X86_NVME_H_
#define ECCLESIA_MAGENT_SYSMODEL_X86_NVME_H_

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/io/pci/location.h"
#include "ecclesia/magent/lib/nvme/identify_controller.h"
#include "ecclesia/magent/lib/nvme/nvme_device.h"
#include "ecclesia/magent/lib/nvme/smart_log_page.h"

namespace ecclesia {

// This class mainly wraps the NVMe device interface. We might need to further
// wraps it with async access.
class NvmeInterface {
 public:
  NvmeInterface(std::unique_ptr<NvmeDeviceInterface> device,
                std::string device_name)
      : device_(std::move(device)), device_name_(std::move(device_name)) {}

  // This helper function creates a NVMe interface instance based on its PCI
  // location.
  static absl::StatusOr<std::unique_ptr<NvmeInterface>> CreateFromPci(
      const PciDbdfLocation &pci_location) {
    return CreateFromPci(pci_location, "/");
  }

  // This helper function allows specific root directory path, mostly for
  // testing purpose.
  static absl::StatusOr<std::unique_ptr<NvmeInterface>> CreateFromPci(
      const PciDbdfLocation &pci_location, absl::string_view root_path);

  absl::StatusOr<std::unique_ptr<IdentifyController>> Identify() {
    return device_->Identify();
  }

  absl::StatusOr<std::unique_ptr<SmartLogPageInterface>> SmartLog() {
    return device_->SmartLog();
  }

  std::string GetKernelName() const { return device_name_; }

 private:
  std::unique_ptr<NvmeDeviceInterface> device_;
  const std::string device_name_;
};

struct NvmeLocation {
  PciDbdfLocation pci_location = PciDbdfLocation::Make<0, 0, 0, 0>();
  // This field identifies the physical location of the NVMe device. e.g.,
  // connector name that this NVMe SSD is plugged into. The devpath or
  // equivalent should be able to be derived from this field.
  std::string physical_location;

  bool operator==(const NvmeLocation &other) const {
    return std::tie(pci_location, physical_location) ==
           std::tie(other.pci_location, other.physical_location);
  }
  bool operator!=(const NvmeLocation &other) const { return !(*this == other); }
};

struct NvmePlugin {
  NvmeLocation location;
  std::unique_ptr<NvmeInterface> access_intf;
};

class NvmeDiscoverInterface {
 public:
  virtual ~NvmeDiscoverInterface() = default;

  virtual absl::StatusOr<std::vector<NvmeLocation>> GetAllNvmeLocations()
      const = 0;

  // Get all NVMe plugins in the system.
  virtual absl::StatusOr<std::vector<NvmePlugin>> GetAllNvmePlugins() const {
    return GetAllNvmePlugins("/");
  }

  // Get all NVMe plugins from a given root directory. This function is mainly
  // for testing purpose.
  absl::StatusOr<std::vector<NvmePlugin>> GetAllNvmePlugins(
      absl::string_view root_path) const {
    auto maybe_nvme_locations = GetAllNvmeLocations();
    if (!maybe_nvme_locations.ok()) {
      return maybe_nvme_locations.status();
    }
    std::vector<NvmePlugin> nvme_plugins;
    for (const auto &location : maybe_nvme_locations.value()) {
      auto maybe_nvme_intf =
          NvmeInterface::CreateFromPci(location.pci_location, root_path);
      if (maybe_nvme_intf.ok()) {
        nvme_plugins.push_back(
            NvmePlugin{location, std::move(maybe_nvme_intf.value())});
      }
    }
    return nvme_plugins;
  }
};

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_SYSMODEL_X86_NVME_H_
