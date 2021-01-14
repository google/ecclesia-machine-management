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

#include "ecclesia/lib/io/pci/sysfs.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "absl/strings/substitute.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "ecclesia/lib/apifs/apifs.h"
#include "ecclesia/lib/codec/endian.h"
#include "ecclesia/lib/file/dir.h"
#include "ecclesia/lib/file/path.h"
#include "ecclesia/lib/io/pci/config.h"
#include "ecclesia/lib/io/pci/discovery.h"
#include "ecclesia/lib/io/pci/location.h"
#include "ecclesia/lib/io/pci/pci.h"
#include "ecclesia/lib/io/pci/region.h"
#include "ecclesia/lib/status/macros.h"
#include "ecclesia/lib/types/fixed_range_int.h"
#include "re2/re2.h"

namespace ecclesia {
namespace {

constexpr char kSysPciRoot[] = "/sys/bus/pci/devices";
constexpr size_t kMaxSysFileSize = 4096;

static LazyRE2 kResourceLineRegex = {
    R"((0x[[:xdigit:]]{16}) (0x[[:xdigit:]]{16}) (0x[[:xdigit:]]{16}))"};

static LazyRE2 kPciBusEntryRegex = {
    R"(pci([[:xdigit:]]{4}):([[:xdigit:]]{2}))"};

// A helper function to get the PCIe link speed and width directly from the
// sysfs nodes under the PCIe device directory. The entry_name_width and
// entry_name_speed should be "<current/max>_link_width" and
// "<current/max>_link_speed", respectively.
absl::StatusOr<PcieLinkCapabilities> GetSysfsPcieLinkCapabilities(
    const ApifsDirectory &pci_device_dir, absl::string_view entry_name_width,
    absl::string_view entry_name_speed) {
  ApifsFile file_link_width(pci_device_dir, entry_name_width);
  ECCLESIA_ASSIGN_OR_RETURN(std::string link_width, file_link_width.Read());
  ApifsFile file_link_speed(pci_device_dir, entry_name_speed);
  ECCLESIA_ASSIGN_OR_RETURN(std::string link_speed, file_link_speed.Read());

  int width = 0;
  if (!absl::SimpleAtoi(link_width, &width)) {
    return absl::InvalidArgumentError("Invalid PCIe link width");
  }
  std::string speed_str;
  double speed_gts = 0;
  // A normal valid link speed string looks like "<speed> GT/s" where <speed>
  // can be 2.5, 5, 8, 16, 32, etc. For an invalid link, the result is normally
  // "Unknown speed".
  static LazyRE2 kPcieSpeedRegex = {R"re((\d+(?:\.\d+)?)\sGT/s\s*)re"};
  if (!RE2::FullMatch(link_speed, *kPcieSpeedRegex, &speed_str) ||
      !absl::SimpleAtod(speed_str, &speed_gts)) {
    return absl::InvalidArgumentError("Invalid PCIe link speed");
  }
  PcieLinkCapabilities link_capabilities;
  link_capabilities.width = NumToPcieLinkWidth(width);
  link_capabilities.speed = NumToPcieLinkSpeed(speed_gts);
  return link_capabilities;
}

}  // namespace

SysPciRegion::SysPciRegion(const PciLocation &loc)
    : SysPciRegion(kSysPciRoot, loc) {}

SysPciRegion::SysPciRegion(absl::string_view sys_pci_devices_dir,
                           const PciLocation &loc)
    : PciRegion(kMaxSysFileSize),
      apifs_(absl::StrFormat("%s/%s/config", sys_pci_devices_dir,
                             absl::FormatStreamed(loc))),
      loc_(loc) {}

absl::StatusOr<uint8_t> SysPciRegion::Read8(OffsetType offset) const {
  std::array<char, sizeof(uint8_t)> res;
  ECCLESIA_RETURN_IF_ERROR(apifs_.SeekAndRead(offset, absl::MakeSpan(res)));
  return LittleEndian::Load8(res.data());
}

absl::Status SysPciRegion::Write8(OffsetType offset, uint8_t data) {
  char buffer[1];
  LittleEndian::Store8(data, buffer);
  return apifs_.SeekAndWrite(offset, absl::MakeConstSpan(buffer));
}

absl::StatusOr<uint16_t> SysPciRegion::Read16(OffsetType offset) const {
  std::array<char, sizeof(uint16_t)> res;
  ECCLESIA_RETURN_IF_ERROR(apifs_.SeekAndRead(offset, absl::MakeSpan(res)));
  return LittleEndian::Load16(res.data());
}

absl::Status SysPciRegion::Write16(OffsetType offset, uint16_t data) {
  char buffer[2];
  LittleEndian::Store16(data, buffer);
  return apifs_.SeekAndWrite(offset, absl::MakeConstSpan(buffer));
}

absl::StatusOr<uint32_t> SysPciRegion::Read32(OffsetType offset) const {
  std::array<char, sizeof(uint32_t)> res;
  ECCLESIA_RETURN_IF_ERROR(apifs_.SeekAndRead(offset, absl::MakeSpan(res)));
  return LittleEndian::Load32(res.data());
}

absl::Status SysPciRegion::Write32(OffsetType offset, uint32_t data) {
  char buffer[4];
  LittleEndian::Store32(data, buffer);
  return apifs_.SeekAndWrite(offset, absl::MakeConstSpan(buffer));
}

SysfsPciResources::SysfsPciResources(PciLocation loc)
    : SysfsPciResources(kSysPciRoot, std::move(loc)) {}

SysfsPciResources::SysfsPciResources(absl::string_view sys_pci_devices_dir,
                                     PciLocation loc)
    : PciResources(std::move(loc)),
      apifs_(absl::StrFormat("%s/%s", sys_pci_devices_dir,
                             absl::FormatStreamed(loc))) {}

bool SysfsPciResources::Exists() { return apifs_.Exists(); }

absl::StatusOr<PciResources::BarInfo> SysfsPciResources::GetBaseAddressImpl(
    BarNum bar_id) const {
  ApifsFile resource_file(apifs_, "resource");
  ECCLESIA_ASSIGN_OR_RETURN(std::string contents, resource_file.Read());

  // BAR is total 24 bytes(4bytes x 6 BAR) in config space. We are reading
  // 'resource' file in sysfs. The 'resource' file uses ascii string to
  // represent hex values. Each line is 57 bytes long, '\0' terminated where the
  // first six lines correspond to the standard BAR resources.
  //
  // Example: /sys/bus/pci/devices/0000:3a:05.4/resource on indus.
  // 0x00000000b8700000 0x00000000b8700fff 0x0000000000040200
  // 0x0000000000000000 0x0000000000000000 0x0000000000000000
  // 0x0000000000000000 0x0000000000000000 0x0000000000000000
  // 0x0000000000000000 0x0000000000000000 0x0000000000000000
  // 0x0000000000000000 0x0000000000000000 0x0000000000000000
  // 0x0000000000000000 0x0000000000000000 0x0000000000000000
  // 0x0000000000000000 0x0000000000000000 0x0000000000000000
  // 0x0000000000000000 0x0000000000000000 0x0000000000000000
  // 0x0000000000000000 0x0000000000000000 0x0000000000000000
  // 0x0000000000000000 0x0000000000000000 0x0000000000000000
  // 0x0000000000000000 0x0000000000000000 0x0000000000000000
  // 0x0000000000000000 0x0000000000000000 0x0000000000000000
  // 0x0000000000000000 0x0000000000000000 0x0000000000000000
  std::vector<absl::string_view> lines =
      absl::StrSplit(absl::StripSuffix(contents, "\n"), '\n');

  if (lines.size() < BarNum::kMaxValue) {
    return absl::InternalError(absl::Substitute(
        "No bar information found in $0", resource_file.GetPath()));
  }

  uint64_t base, limit, flags;
  if (!RE2::FullMatch(lines[bar_id.value()], *kResourceLineRegex,
                      RE2::Hex(&base), RE2::Hex(&limit), RE2::Hex(&flags))) {
    return absl::InternalError(
        absl::Substitute("Error reading BAR information."));
  }

  return BarInfo{(flags & kIoResourceFlag) ? kBarTypeIo : kBarTypeMem, base};
}

std::unique_ptr<SysfsPciDevice> SysfsPciDevice::TryCreateDevice(
    const PciLocation &location) {
  return TryCreateDevice(kSysPciRoot, location);
}

std::unique_ptr<SysfsPciDevice> SysfsPciDevice::TryCreateDevice(
    absl::string_view sys_pci_devices_dir, const PciLocation &location) {
  ApifsDirectory apifs((std::string(sys_pci_devices_dir)));
  if (!apifs.Exists(location.ToString())) {
    return nullptr;
  }
  return absl::WrapUnique(new SysfsPciDevice(sys_pci_devices_dir, location));
}

SysfsPciDevice::SysfsPciDevice(absl::string_view sys_pci_devices_dir,
                               const PciLocation &location)
    : PciDevice(
          location,
          std::make_unique<SysPciRegion>(sys_pci_devices_dir, location),
          std::make_unique<SysfsPciResources>(sys_pci_devices_dir, location)),
      pci_device_dir_(JoinFilePaths(sys_pci_devices_dir, location.ToString())) {
}

absl::StatusOr<PcieLinkCapabilities> SysfsPciDevice::PcieLinkMaxCapabilities()
    const {
  return GetSysfsPcieLinkCapabilities(pci_device_dir_, "max_link_width",
                                      "max_link_speed");
}

absl::StatusOr<PcieLinkCapabilities>
SysfsPciDevice::PcieLinkCurrentCapabilities() const {
  return GetSysfsPcieLinkCapabilities(pci_device_dir_, "current_link_width",
                                      "current_link_speed");
}

SysfsPciTopology::SysfsPciTopology() : SysfsPciTopology("/sys/devices") {}

SysfsPciTopology::SysfsPciTopology(const std::string &sys_devices_dir)
    : sys_devices_dir_(sys_devices_dir) {}

absl::StatusOr<PciTopologyInterface::PciNodeMap>
SysfsPciTopology::EnumerateAllNodes() const {
  PciTopologyInterface::PciNodeMap pci_node_map;
  absl::Status status = WithEachFileInDirectory(
      sys_devices_dir_, [&](absl::string_view entry_name) {
        if (RE2::FullMatch(entry_name, *kPciBusEntryRegex)) {
          ScanDirectory(JoinFilePaths(sys_devices_dir_, entry_name), 0, nullptr,
                        &pci_node_map);
        }
      });
  if (status.ok()) {
    return pci_node_map;
  }
  return status;
}

std::vector<PciTopologyInterface::Node *> SysfsPciTopology::ScanDirectory(
    absl::string_view directory_path, size_t depth,
    PciTopologyInterface::Node *parent,
    PciTopologyInterface::PciNodeMap *pci_node_map) const {
  std::vector<Node *> nodes_in_this_dir;
  WithEachFileInDirectory(directory_path, [&](absl::string_view entry_name) {
    auto maybe_loc = PciLocation::FromString(entry_name);
    if (maybe_loc.has_value()) {
      auto node = std::make_unique<PciTopologyInterface::Node>(
          maybe_loc.value(), depth, parent);
      node->SetChildren(ScanDirectory(JoinFilePaths(directory_path, entry_name),
                                      depth + 1, node.get(), pci_node_map));

      nodes_in_this_dir.push_back(node.get());
      pci_node_map->emplace(maybe_loc.value(), std::move(node));
    }
  }).IgnoreError();
  return nodes_in_this_dir;
}

absl::StatusOr<std::vector<PciTopologyInterface::PciAcpiPath>>
SysfsPciTopology::EnumeratePciAcpiPaths() const {
  std::vector<PciTopologyInterface::PciAcpiPath> acpi_nodes;
  absl::Status status = WithEachFileInDirectory(
      sys_devices_dir_, [&](absl::string_view entry_name) {
        int domain;
        int bus;
        if (RE2::FullMatch(entry_name, *kPciBusEntryRegex, RE2::Hex(&domain),
                           RE2::Hex(&bus))) {
          ApifsFile file(JoinFilePaths(sys_devices_dir_, entry_name,
                                       "firmware_node/path"));
          if (file.Exists()) {
            auto maybe_str = file.Read();
            if (maybe_str.ok()) {
              std::string fw_path = maybe_str.value();
              // Trims trailing white space.
              const auto pos = fw_path.find_last_not_of(" \n\r\t\f\v");
              if (pos != std::string::npos) {
                fw_path.erase(pos + 1);
              }
              acpi_nodes.push_back({*PciDomain::TryMake(domain),
                                    *PciBusNum::TryMake(bus), fw_path});
            }
          }
        }
      });
  if (status.ok()) {
    return acpi_nodes;
  }
  return status;
}
}  // namespace ecclesia
