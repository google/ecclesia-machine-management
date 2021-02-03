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

#include "ecclesia/magent/sysmodel/x86/sysmodel.h"

#include <sys/sysinfo.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "ecclesia/lib/io/pci/config.h"
#include "ecclesia/lib/io/pci/discovery.h"
#include "ecclesia/lib/io/pci/location.h"
#include "ecclesia/lib/io/pci/pci.h"
#include "ecclesia/lib/io/pci/sysfs.h"
#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/lib/smbios/reader.h"
#include "ecclesia/lib/time/clock.h"
#include "ecclesia/magent/lib/event_logger/event_logger.h"
#include "ecclesia/magent/lib/event_logger/system_event_visitors.h"
#include "ecclesia/magent/lib/event_reader/elog_reader.h"
#include "ecclesia/magent/lib/event_reader/event_reader.h"
#include "ecclesia/magent/lib/event_reader/mced_reader.h"
#include "ecclesia/magent/lib/io/usb_sysfs.h"
#include "ecclesia/magent/sysmodel/x86/chassis.h"
#include "ecclesia/magent/sysmodel/x86/cpu.h"
#include "ecclesia/magent/sysmodel/x86/dimm.h"
#include "ecclesia/magent/sysmodel/x86/fru.h"
#include "ecclesia/magent/sysmodel/x86/nvme.h"
#include "ecclesia/magent/sysmodel/x86/thermal.h"

namespace ecclesia {

std::size_t SystemModel::NumDimms() const {
  absl::ReaderMutexLock ml(&dimms_lock_);
  return dimms_.size();
}

absl::optional<Dimm> SystemModel::GetDimm(std::size_t index) {
  absl::ReaderMutexLock ml(&dimms_lock_);
  if (index < dimms_.size()) {
    return dimms_[index];
  }
  return absl::nullopt;
}

std::size_t SystemModel::NumDimmThermalSensors() const {
  absl::ReaderMutexLock ml(&dimm_thermal_sensors_lock_);
  return dimm_thermal_sensors_.size();
}

PciThermalSensor *SystemModel::GetDimmThermalSensor(std::size_t index) {
  absl::ReaderMutexLock ml(&dimm_thermal_sensors_lock_);
  if (index < dimm_thermal_sensors_.size()) {
    return &dimm_thermal_sensors_[index];
  }
  return nullptr;
}

std::size_t SystemModel::NumCpus() const {
  absl::ReaderMutexLock ml(&cpus_lock_);
  return cpus_.size();
}

absl::optional<Cpu> SystemModel::GetCpu(std::size_t index) {
  absl::ReaderMutexLock ml(&cpus_lock_);
  if (index < cpus_.size()) {
    return cpus_[index];
  }
  return absl::nullopt;
}

std::size_t SystemModel::NumCpuMarginSensors() const {
  absl::ReaderMutexLock ml(&cpu_margin_sensors_lock_);
  return cpu_margin_sensors_.size();
}

absl::optional<CpuMarginSensor> SystemModel::GetCpuMarginSensor(
    std::size_t index) {
  absl::ReaderMutexLock ml(&cpu_margin_sensors_lock_);
  if (index < cpu_margin_sensors_.size()) {
    return cpu_margin_sensors_[index];
  }
  return absl::nullopt;
}

std::size_t SystemModel::NumFruReaders() const {
  absl::ReaderMutexLock ml(&fru_readers_lock_);
  return fru_readers_.size();
}

SysmodelFruReaderIntf *SystemModel::GetFruReader(
    absl::string_view fru_name) const {
  absl::ReaderMutexLock ml(&fru_readers_lock_);
  auto fru = fru_readers_.find(fru_name);
  if (fru != fru_readers_.end()) {
    return fru->second.get();
  }

  return nullptr;
}

std::vector<ChassisId> SystemModel::GetAllChassis() const {
  absl::ReaderMutexLock ml(&chassis_lock_);
  return chassis_;
}
absl::optional<ChassisId> SystemModel::GetChassisByName(
    absl::string_view chassis_name) const {
  absl::ReaderMutexLock ml(&chassis_lock_);
  for (const auto &chassis_id : chassis_) {
    if (chassis_name == ChassisIdToString(chassis_id)) {
      return chassis_id;
    }
  }
  return absl::nullopt;
}

std::size_t SystemModel::NumNvmePlugins() const {
  absl::ReaderMutexLock ml(&nvme_plugins_lock_);
  return nvme_plugins_.size();
}

std::vector<std::string> SystemModel::GetNvmePhysLocations() const {
  std::vector<std::string> phys_locations;
  absl::ReaderMutexLock ml(&nvme_plugins_lock_);
  for (const auto &nvme : nvme_plugins_) {
    phys_locations.push_back(nvme.first);
  }
  return phys_locations;
}

absl::optional<SystemModel::NvmePluginInfo> SystemModel::GetNvmeByPhysLocation(
    absl::string_view phys_location) const {
  absl::ReaderMutexLock ml(&nvme_plugins_lock_);
  auto iter = nvme_plugins_.find(phys_location);
  if (iter != nvme_plugins_.end()) {
    return NvmePluginInfo{iter->second.location,
                          iter->second.access_intf.get()};
  }
  return absl::nullopt;
}

std::vector<PciStorageLocation> SystemModel::GetPciStorageLocations() const {
  std::vector<PciStorageLocation> locations;
  absl::ReaderMutexLock ml(&pci_storage_locations_lock_);
  for (const auto &loc : pci_storage_locations_) {
    locations.push_back(loc);
  }
  return locations;
}

absl::StatusOr<SystemModel::PciNodeConnections>
SystemModel::GetPciNodeConnections(const PciLocation &pci_location) const {
  absl::ReaderMutexLock ml(&pci_tree_nodes_lock_);
  auto iter = pci_tree_nodes_.find(pci_location);
  if (iter == pci_tree_nodes_.end()) {
    return absl::NotFoundError(absl::StrCat("No PCI node found with location: ",
                                            pci_location.ToString()));
  }
  SystemModel::PciNodeConnections node_connections;
  auto *parent = iter->second->Parent();
  if (parent == nullptr) {
    node_connections.parent = absl::nullopt;
  } else {
    node_connections.parent = parent->Location();
  }

  for (const auto *child : iter->second->Children()) {
    if (child != nullptr) {
      node_connections.children.push_back(child->Location());
    }
  }
  return node_connections;
}

std::unique_ptr<PciDevice> SystemModel::GetPciDevice(
    const PciLocation &pci_location) const {
  return pci_topology_->CreateDevice(pci_location);
}

std::vector<PciLocation> SystemModel::GetPcieDeviceLocations() const {
  absl::ReaderMutexLock ml(&pcie_device_locations_lock_);
  return pcie_device_locations_;
}

void SystemModel::LoadBootNumberFromElogReader(ElogReader &elog_reader) {
  for (auto event = elog_reader.ReadEvent(); event.has_value();
       event = elog_reader.ReadEvent()) {
    auto maybe_elog = absl::get_if<Elog>(&event->record);
    if (!maybe_elog) continue;
    const auto &elog_record_view = maybe_elog->GetElogRecordView();

    if (elog_record_view.Ok()) {
      // 1. System Boot - The boot number will be included in the log
      // 2. Log area reset - The boot number will be included in the log
      //
      // For each event log, we will at least see one of these since the
      // system boot event must be included or it should've been cleared in
      // which case we will get a log area reset event
      switch (elog_record_view.id().Read()) {
        case EventType::SYSTEM_BOOT: {
          if (elog_record_view.system_boot().has_bootnum().ValueOr(false)) {
            boot_number_ = elog_record_view.system_boot().bootnum().Read();
          }
          break;
        }
        case EventType::LOG_AREA_RESET: {
          if (elog_record_view.log_area_reset().has_boot_num().ValueOr(false)) {
            boot_number_ = elog_record_view.log_area_reset().boot_num().Read();
          }
          break;
        }
        default:
          break;
      }
    }
  }
}

absl::optional<uint32_t> SystemModel::GetBootNumber() { return boot_number_; }

absl::StatusOr<int64_t> SystemModel::GetSystemUptimeSeconds() const {
  struct sysinfo sys_info;
  if (sysinfo(&sys_info) != 0) {
    return absl::InternalError("Failed to get sysinfo");
  }
  return static_cast<int64_t>(sys_info.uptime);
}

absl::StatusOr<uint64_t> SystemModel::GetSystemTotalMemoryBytes() const {
  struct sysinfo sys_info;
  if (sysinfo(&sys_info) != 0) {
    return absl::InternalError("Failed to get sysinfo");
  }
  return static_cast<uint64_t>(sys_info.totalram) *
         static_cast<uint32_t>(sys_info.mem_unit);
}

SystemModel::SystemModel(SysmodelParams params)
    : smbios_reader_(absl::make_unique<SmbiosReader>(
          params.smbios_entry_point_path, params.smbios_tables_path)),
      field_translator_(std::move(params.field_translator)),
      pci_topology_(std::make_unique<SysfsPciTopology>()),
      usb_discovery_(std::make_unique<SysfsUsbDiscovery>()),
      dimm_thermal_params_(std::move(params.dimm_thermal_params)),
      cpu_margin_params_(std::move(params.cpu_margin_params)) {
  // Construct system model objects
  auto dimms = CreateDimms(smbios_reader_.get(), field_translator_.get());
  {
    absl::WriterMutexLock ml(&dimms_lock_);
    dimms_ = std::move(dimms);
  }

  auto cpus = CreateCpus(*smbios_reader_);
  {
    absl::WriterMutexLock ml(&cpus_lock_);
    cpus_ = std::move(cpus);
  }

  auto fru_readers = CreateFruReaders(params.fru_factories);
  {
    absl::WriterMutexLock ml(&fru_readers_lock_);
    fru_readers_ = std::move(fru_readers);
  }

  auto dimm_thermal_sensors = CreatePciThermalSensors(dimm_thermal_params_);
  {
    absl::WriterMutexLock ml(&dimm_thermal_sensors_lock_);
    dimm_thermal_sensors_ = std::move(dimm_thermal_sensors);
  }

  auto cpu_margin_sensors = CreateCpuMarginSensors(cpu_margin_params_);
  {
    absl::WriterMutexLock ml(&cpu_margin_sensors_lock_);
    cpu_margin_sensors_ = std::move(cpu_margin_sensors);
  }

  auto chassis = CreateChassis(usb_discovery_.get());
  {
    absl::WriterMutexLock ml(&chassis_lock_);
    chassis_ = std::move(chassis);
  }

  if (params.nvme_discover_getter) {
    std::unique_ptr<NvmeDiscoverInterface> nvme_discover =
        params.nvme_discover_getter(pci_topology_.get());
    if (nvme_discover) {
      auto maybe_nvme_plugins = nvme_discover->GetAllNvmePlugins();
      if (!maybe_nvme_plugins.ok()) {
        ErrorLog() << "Failed to get NVMe plugins. status: "
                   << maybe_nvme_plugins.status();
      } else {
        absl::WriterMutexLock ml(&nvme_plugins_lock_);
        // NvmePlugin contains unique_ptr. Need to transfer the ownership.
        for (auto iter = maybe_nvme_plugins.value().begin();
             iter != maybe_nvme_plugins.value().end(); iter++) {
          nvme_plugins_[iter->location.physical_location] = std::move(*iter);
        }
      }
    }
  }

  if (params.pci_storage_discover_getter) {
    std::unique_ptr<PciStorageDiscoverInterface> storage_discover =
        params.pci_storage_discover_getter(pci_topology_.get());
    if (storage_discover) {
      auto maybe_storage_locations =
          storage_discover->GetAllPciStorageLocations();
      if (!maybe_storage_locations.ok()) {
        ErrorLog() << "Failed to get PCI Storage locations. status: "
                   << maybe_storage_locations.status();
      } else {
        absl::WriterMutexLock ml(&pci_storage_locations_lock_);
        pci_storage_locations_ = std::move(*maybe_storage_locations);
      }
    }
  }

  auto pci_nodes = pci_topology_->EnumerateAllNodes();
  if (pci_nodes.ok()) {
    std::vector<PciLocation> pcie_dev_locations;
    for (const auto &pci_node : *pci_nodes) {
      auto pci_dev = pci_topology_->CreateDevice(pci_node.first);
      auto link_caps = pci_dev->PcieLinkCurrentCapabilities();
      if (link_caps.ok() && link_caps->width != PcieLinkWidth::kUnknown &&
          link_caps->speed != PcieLinkSpeed::kUnknown) {
        pcie_dev_locations.push_back(pci_node.first);
      }
    }
    {
      absl::WriterMutexLock ml(&pcie_device_locations_lock_);
      pcie_device_locations_ = std::move(pcie_dev_locations);
    }
    {
      absl::WriterMutexLock ml(&pci_tree_nodes_lock_);
      pci_tree_nodes_ = std::move(*pci_nodes);
    }
  }

  // Create event readers to feed into the event logger
  std::vector<std::unique_ptr<SystemEventReader>> readers;
  readers.push_back(absl::make_unique<McedaemonReader>(params.mced_socket_path,
                                                       &mcedaemon_socket_));
  if (auto system_event_log = smbios_reader_->GetSystemEventLog()) {
    readers.push_back(absl::make_unique<ElogReader>(
        std::move(system_event_log), params.sysfs_mem_file_path));
  }

  event_logger_ = absl::make_unique<SystemEventLogger>(std::move(readers),
                                                       Clock::RealClock());

  // Load boot number from SMBIOS if it's there
  auto maybe_boot_number =
      smbios_reader_->GetBootNumberFromSystemBootInformation();
  if (maybe_boot_number.ok()) {
    boot_number_ = maybe_boot_number.value();
  } else {
    WarningLog() << "Boot number unavailable in SMBIOS: "
                 << maybe_boot_number.status().message();
    // Attempt to read event log for boot number
    if (auto system_event_log = smbios_reader_->GetSystemEventLog()) {
      auto reader =
          ElogReader(std::move(system_event_log), params.sysfs_mem_file_path);
      LoadBootNumberFromElogReader(reader);
      if (!boot_number_.has_value()) {
        WarningLog() << "Boot number unavailable in BIOS Event Log";
      }
    }
  }
}

}  // namespace ecclesia
