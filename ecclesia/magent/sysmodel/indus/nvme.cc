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

#include "ecclesia/magent/sysmodel/indus/nvme.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "ecclesia/lib/io/pci/config.h"
#include "ecclesia/lib/io/pci/discovery.h"
#include "ecclesia/lib/io/pci/location.h"
#include "ecclesia/lib/io/pci/pci.h"
#include "ecclesia/magent/sysmodel/x86/nvme.h"

namespace ecclesia {
namespace {

using PciNodeMap = PciTopologyInterface::PciNodeMap;

constexpr int kPciClassCodeStorageNvme = 0x010802;

// This helper function scans the NVMe devices as PCI plugins along the PCI root
// bus. If matched devices are found, their locations (PCI location and
// connector name) are added to the nvme_locations vector.
void ScanNvmeAsPciPlugins(
    const PciTopologyInterface *pci_topology, const PciNodeMap &pci_nodes_map,
    const PciDomain &pci_domain, const PciBusNum &pci_bus,
    const std::function<std::string(int)> &connector_name_func,
    std::vector<NvmeLocation> *nvme_locations) {
  for (int pci_dev = 0; pci_dev <= 3; ++pci_dev) {
    std::optional<PciDbdfLocation> pci_location = PciDbdfLocation::TryMake(
        pci_domain.value(), pci_bus.value(), pci_dev, 0);
    if (!pci_location.has_value() ||
        !pci_nodes_map.contains(pci_location.value())) {
      continue;
    }
    auto *root_node = pci_nodes_map.at(pci_location.value()).get();
    for (const auto *child_node : root_node->Children()) {
      auto pci_device = pci_topology->CreateDevice(child_node->Location());
      auto maybe_class_code = pci_device->ConfigSpace()->ClassCode();
      if (maybe_class_code.ok() &&
          maybe_class_code.value() == kPciClassCodeStorageNvme) {
        // The NVMe physical location is set to the connector name U2_x.
        NvmeLocation nvme_location{child_node->Location(),
                                   connector_name_func(pci_dev)};
        nvme_locations->push_back(nvme_location);
        break;
      }
    }
  }
}

}  // namespace

absl::StatusOr<std::vector<NvmeLocation>>
IndusNvmeDiscover::GetAllNvmeLocations() const {
  std::vector<NvmeLocation> nvme_locations;
  auto maybe_all_pci_nodes = pci_topology_->EnumerateAllNodes();
  if (!maybe_all_pci_nodes.ok()) {
    return maybe_all_pci_nodes.status();
  }
  const PciNodeMap &pci_nodes_map = maybe_all_pci_nodes.value();
  auto maybe_acpi_pci_paths = pci_topology_->EnumeratePciAcpiPaths();
  if (!maybe_acpi_pci_paths.ok()) {
    return maybe_acpi_pci_paths.status();
  }

  const auto &acpi_pci_paths = maybe_acpi_pci_paths.value();
  for (const auto &acpi_pci : acpi_pci_paths) {
    auto pci_domain = acpi_pci.domain;
    auto pci_bus = acpi_pci.bus;
    // Identifies the PCI bus with the specified ACPI path.
    if (acpi_pci.acpi_path == "\\_SB_.PC08") {
      // Check the 4 PCI buses go through the PE2
      // The connectors U2_x, x=0,1,2,3 are mapped to the PCI buses with PCI dev
      // 0,1,2,3.
      ScanNvmeAsPciPlugins(
          pci_topology_, pci_nodes_map, pci_domain, pci_bus,
          [](int pci_dev) { return absl::StrCat("U2_", pci_dev); },
          &nvme_locations);
    } else if (acpi_pci.acpi_path == "\\_SB_.PC09") {
      // Check the 4 PCI buses go through the PE3
      // The connectors U2_x, x=4,5,6,7 are mapped to the PCI buses with PCI dev
      // 3,2,1,0.
      ScanNvmeAsPciPlugins(
          pci_topology_, pci_nodes_map, pci_domain, pci_bus,
          [](int pci_dev) { return absl::StrCat("U2_", 7 - pci_dev); },
          &nvme_locations);
    }
  }
  return nvme_locations;
}

}  // namespace ecclesia
