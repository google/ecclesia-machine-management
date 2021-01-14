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

// A class for access PCI devices through sysfs

#ifndef ECCLESIA_LIB_IO_PCI_SYSFS_H_
#define ECCLESIA_LIB_IO_PCI_SYSFS_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/apifs/apifs.h"
#include "ecclesia/lib/io/pci/config.h"
#include "ecclesia/lib/io/pci/discovery.h"
#include "ecclesia/lib/io/pci/location.h"
#include "ecclesia/lib/io/pci/pci.h"
#include "ecclesia/lib/io/pci/region.h"

namespace ecclesia {

class SysPciRegion : public PciRegion {
 public:
  explicit SysPciRegion(const PciLocation &pci_loc);

  // This constructor allows customized sysfs PCI devices directory, mostly for
  // testing purpose.
  SysPciRegion(absl::string_view sys_pci_devices_dir,
               const PciLocation &pci_loc);

  absl::StatusOr<uint8_t> Read8(OffsetType offset) const override;
  absl::Status Write8(OffsetType offset, uint8_t data) override;

  absl::StatusOr<uint16_t> Read16(OffsetType offset) const override;
  absl::Status Write16(size_t offset, uint16_t data) override;

  absl::StatusOr<uint32_t> Read32(OffsetType offset) const override;
  absl::Status Write32(OffsetType offset, uint32_t data) override;

 private:
  ApifsFile apifs_;
  PciLocation loc_;
};

class SysfsPciResources : public PciResources {
 public:
  explicit SysfsPciResources(PciLocation loc);
  SysfsPciResources(absl::string_view sys_pci_devices_dir, PciLocation loc);

  bool Exists() override;

 private:
  // Resource flag that indicate that a resource is an I/O resource.
  static constexpr uint64_t kIoResourceFlag = 0x100;

  absl::StatusOr<BarInfo> GetBaseAddressImpl(BarNum bar_id) const override;

  ApifsDirectory apifs_;
};

class SysfsPciDevice : public PciDevice {
 public:
  // This factory method creates a PCI device given a PCI location. If there
  // exists no such sysfs node with the given PCI location, return nullptr.
  static std::unique_ptr<SysfsPciDevice> TryCreateDevice(
      const PciLocation &location);

  // This creator allows customized sysfs PCI devices directory, mostly for
  // testing purpose.
  static std::unique_ptr<SysfsPciDevice> TryCreateDevice(
      absl::string_view sys_pci_devices_dir, const PciLocation &location);

  // These methods utilize Linux sysfs interfaces to get the PCIe link status
  // directly via entries current_link_speed, current_link_width,
  // max_link_speed, max_link_width under the PCI device directory, which is
  // more efficient than iterating the PCI config space capability register set
  // to get the data.
  absl::StatusOr<PcieLinkCapabilities> PcieLinkMaxCapabilities() const override;

  absl::StatusOr<PcieLinkCapabilities> PcieLinkCurrentCapabilities()
      const override;

 private:
  SysfsPciDevice(absl::string_view sys_pci_devices_dir,
                 const PciLocation &location);
  ApifsDirectory pci_device_dir_;
};

class SysfsPciTopology : public PciTopologyInterface {
 public:
  SysfsPciTopology();

  // This constructor allows customized sysfs devices directory, mostly for
  // testing purpose.
  SysfsPciTopology(const std::string &sys_devices_dir);

  absl::StatusOr<PciNodeMap> EnumerateAllNodes() const override;

  std::unique_ptr<PciDevice> CreateDevice(
      const PciLocation &location) const override {
    return SysfsPciDevice::TryCreateDevice(location);
  }

  // This method scans the /sys/devices/pci<domain>:<bus> and associate the
  // domain:bus with the ACPI path content in
  // /sys/devices/pci<domain>:<bus>/firmware_node/path
  absl::StatusOr<std::vector<PciAcpiPath>> EnumeratePciAcpiPaths()
      const override;

 private:
  // This helper function recursively scans the input directory and its
  // subdirectories for any PCI nodes. The found nodes will be added to the
  // pci_node_map and linked to the PCI topology tree. The return vector is the
  // nodes in this directory only (exclude subdirectories).
  std::vector<PciTopologyInterface::Node *> ScanDirectory(
      absl::string_view directory_path, size_t depth,
      PciTopologyInterface::Node *parent,
      PciTopologyInterface::PciNodeMap *pci_node_map) const;

  // The sysfs dir in Linux is always /sys/devices. This variable is only for
  // testing purpose.
  const std::string sys_devices_dir_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_IO_PCI_SYSFS_H_
