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

// Provide basic operations for interacting with PCI devices. This library
// provides several abstractions at different layers:
//   * PciLocation, a value object representing a PCI BDF-style address
//   * PciRegion, a low-level interface accessing a PCI address space
//   * PciConfigSpace and PciDevice, objects that provide higher-level
//     operations built on top of PciRegion

#ifndef ECCLESIA_LIB_IO_PCI_DISCOVERY_H_
#define ECCLESIA_LIB_IO_PCI_DISCOVERY_H_

#include <cstddef>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "ecclesia/lib/io/pci/location.h"
#include "ecclesia/lib/io/pci/pci.h"
#include "ecclesia/lib/types/fixed_range_int.h"

namespace ecclesia {

// This interface class defines some methods for discovering the PCI topologies.
class PciTopologyInterface {
 public:
  virtual ~PciTopologyInterface() = default;

  // This class represents a node in the PCI topology tree.
  class Node {
   public:
    Node(const PciDbdfLocation& location, size_t depth, Node* parent)
        : location_(location), depth_(depth), parent_(parent) {}

    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;
    Node(Node&&) = delete;
    Node& operator=(Node&&) = delete;

    const PciDbdfLocation& Location() const { return location_; }

    // This gets the depth in the PCI tree. For a root node, the depth is 0.
    size_t Depth() const { return depth_; }

    // This gets the parent of this node, for a root node, this
    // function returns nullptr.
    Node* Parent() const { return parent_; }

    // This gets the children of this node, for a endpoint node without
    // children, the return vector is empty.
    absl::Span<const Node* const> Children() const { return children_; }

    void AddChild(Node* node) { children_.push_back(node); }

    void SetChildren(std::vector<Node*> children) {
      children_ = std::move(children);
    }

   private:
    const PciDbdfLocation location_;
    size_t depth_;
    Node* parent_;
    std::vector<Node*> children_;
  };

  using PciNodeMap =
      absl::flat_hash_map<PciDbdfLocation, std::unique_ptr<Node>>;
  virtual absl::StatusOr<PciNodeMap> EnumerateAllNodes() const = 0;

  // A method to convert a PCI location to a PCI device instance. The created
  // PCI device can facilitate the accesses to the PCI device with the given
  // location. If no such PCI device with the given location exists, return
  // nullptr.
  virtual std::unique_ptr<PciDevice> CreateDevice(
      const PciDbdfLocation& location) const = 0;

  // This struct associates the root PCI bus with the corresponding ACPI path.
  struct PciAcpiPath {
    PciDomain domain;
    PciBusNum bus;
    // ACPI path corresponds to the content of the firmware_node/path file in
    // sysfs, e.g., "\_SB_.PC00"
    std::string acpi_path;

    bool operator==(const PciAcpiPath& other) const {
      return std::tie(domain, bus, acpi_path) ==
             std::tie(other.domain, other.bus, other.acpi_path);
    }
    bool operator!=(const PciAcpiPath& other) const {
      return !(*this == other);
    }
  };

  // Enumerate the PCI buses that corresponds to ACPI device paths.
  virtual absl::StatusOr<std::vector<PciAcpiPath>> EnumeratePciAcpiPaths()
      const = 0;

  // This struct associates the root PCI bus with the corresponding
  // platform path in /sys/devices/platform/.
  struct PciPlatformPath {
    PciDomain domain;
    PciBusNum bus;
    // platform_path corresponds to the directory name in
    // /sys/devices/platform/, e.g., "2041800000.pcie"
    std::string platform_path;

    bool operator==(const PciPlatformPath& other) const {
      return std::tie(domain, bus, platform_path) ==
             std::tie(other.domain, other.bus, other.platform_path);
    }
    bool operator!=(const PciPlatformPath& other) const {
      return !(*this == other);
    }
  };

  // Enumerate the PCI buses that corresponds to platform paths.
  virtual absl::StatusOr<std::vector<PciPlatformPath>>
  EnumeratePciPlatformPaths() const = 0;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_IO_PCI_DISCOVERY_H_
