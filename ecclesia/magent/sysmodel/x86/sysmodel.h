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

#ifndef ECCLESIA_MAGENT_SYSMODEL_X86_SYSMODEL_H_
#define ECCLESIA_MAGENT_SYSMODEL_X86_SYSMODEL_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "ecclesia/lib/io/pci/discovery.h"
#include "ecclesia/lib/io/pci/location.h"
#include "ecclesia/lib/io/pci/pci.h"
#include "ecclesia/lib/io/usb/usb.h"
#include "ecclesia/lib/smbios/platform_translator.h"
#include "ecclesia/lib/smbios/reader.h"
#include "ecclesia/magent/lib/event_logger/event_logger.h"
#include "ecclesia/magent/lib/event_reader/elog_reader.h"
#include "ecclesia/magent/lib/event_reader/event_reader.h"
#include "ecclesia/magent/lib/event_reader/mced_reader.h"
// #include "ecclesia/magent/lib/ipmi/interface_options.h"
#include "ecclesia/magent/sysmodel/x86/chassis.h"
#include "ecclesia/magent/sysmodel/x86/cpu.h"
#include "ecclesia/magent/sysmodel/x86/dimm.h"
#include "ecclesia/magent/sysmodel/x86/fru.h"
#include "ecclesia/magent/sysmodel/x86/nvme.h"
#include "ecclesia/magent/sysmodel/x86/pci_storage.h"
#include "ecclesia/magent/sysmodel/x86/sleipnir_sensor.h"
#include "ecclesia/magent/sysmodel/x86/thermal.h"

namespace ecclesia {

// Parameters required to construct the system model
struct SysmodelParams {
  std::unique_ptr<SmbiosFieldTranslator> field_translator;
  std::string smbios_entry_point_path;
  std::string smbios_tables_path;
  std::string mced_socket_path;
  std::string sysfs_mem_file_path;
  absl::Span<const SysmodelFruReaderFactory> fru_factories;
  absl::Span<const PciSensorParams> dimm_thermal_params;
  absl::Span<const IpmiInterface::BmcSensorInterfaceInfo> ipmi_sensors;
  std::function<std::unique_ptr<NvmeDiscoverInterface>(PciTopologyInterface *)>
      nvme_discover_getter;
  std::function<std::unique_ptr<PciStorageDiscoverInterface>(
      PciTopologyInterface *)>
      pci_storage_discover_getter;
};

// The SystemModel must be thread safe
class SystemModel {
 public:
  explicit SystemModel(SysmodelParams params);
  ~SystemModel() {}

  std::size_t NumDimms() const;
  absl::optional<Dimm> GetDimm(std::size_t index);

  // The number of DIMM thermal sensors. This should be the same as the number
  // of DIMMs.
  std::size_t NumDimmThermalSensors() const;
  // Return the sensor for DIMM at “index”. Return nullptr if index is out of
  // bounds.
  PciThermalSensor *GetDimmThermalSensor(std::size_t index);

  std::vector<SleipnirSensor> GetAllIpmiSensors() const;

  std::size_t NumCpus() const;
  absl::optional<Cpu> GetCpu(std::size_t index);

  std::size_t NumFruReaders() const;
  template <typename IteratorF>
  void GetFruReaders(IteratorF iterator) const {
    absl::ReaderMutexLock ml(&fru_readers_lock_);
    for (const auto &f : fru_readers_) {
      iterator(f.first, f.second);
    }
  }
  SysmodelFruReaderIntf *GetFruReader(absl::string_view fru_name) const;

  std::vector<ChassisId> GetAllChassis() const;
  absl::optional<ChassisId> GetChassisByName(
      absl::string_view chassis_name) const;

  struct NvmePluginInfo {
    NvmeLocation location;
    NvmeInterface *access_intf;
  };
  std::size_t NumNvmePlugins() const;
  // Get the physical location strings of the NVMe plugins.
  std::vector<std::string> GetNvmePhysLocations() const;
  // Get the NVMe plugin info by physical location.
  absl::optional<NvmePluginInfo> GetNvmeByPhysLocation(
      absl::string_view phys_location) const;

  std::vector<PciStorageLocation> GetPciStorageLocations() const;

  // The event logger logs all of the system events with respect to cpu and dimm
  // errors. This method provides a mechanism to process the events for error
  // reporting.
  void VisitSystemEvents(SystemEventVisitor *visitor) {
    if (event_logger_) {
      event_logger_->Visit(visitor);
    }
  }

  UsbDiscoveryInterface *GetUsbDiscoveryIntf() const {
    return usb_discovery_.get();
  }

  struct PciNodeConnections {
    absl::optional<PciDbdfLocation> parent;
    std::vector<PciDbdfLocation> children;
  };

  absl::StatusOr<PciNodeConnections> GetPciNodeConnections(
      const PciDbdfLocation &pci_location) const;

  std::unique_ptr<PciDevice> GetPciDevice(
      const PciDbdfLocation &pci_location) const;

  std::vector<PciDbdfLocation> GetPcieDeviceLocations() const;

  absl::optional<uint32_t> GetBootNumber();

  // Returns the system uptime in unit of seconds.
  absl::StatusOr<int64_t> GetSystemUptimeSeconds() const;

  // Returns the system total memory size in unit of bytes.
  absl::StatusOr<uint64_t> GetSystemTotalMemoryBytes() const;

  // Returns the enumeration of ACPI paths found in PCI topology
  absl::StatusOr<std::vector<PciTopologyInterface::PciAcpiPath>>
  GetAcpiPathsFromPciTopology() const;

 private:
  // Platform interfaces
  std::unique_ptr<SmbiosReader> smbios_reader_;
  std::unique_ptr<SmbiosFieldTranslator> field_translator_;
  std::unique_ptr<PciTopologyInterface> pci_topology_;
  std::unique_ptr<UsbDiscoveryInterface> usb_discovery_;
  LibcMcedaemonSocket mcedaemon_socket_;

  // System model objects

  mutable absl::Mutex dimms_lock_;
  std::vector<Dimm> dimms_ ABSL_GUARDED_BY(dimms_lock_);

  mutable absl::Mutex cpus_lock_;
  std::vector<Cpu> cpus_ ABSL_GUARDED_BY(cpus_lock_);

  mutable absl::Mutex fru_readers_lock_;
  absl::flat_hash_map<std::string, std::unique_ptr<SysmodelFruReaderIntf>>
      fru_readers_ ABSL_GUARDED_BY(fru_readers_lock_);

  mutable absl::Mutex dimm_thermal_sensors_lock_;
  std::vector<PciThermalSensor> dimm_thermal_sensors_
      ABSL_GUARDED_BY(dimm_thermal_sensors_lock_);

  mutable absl::Mutex sleipnir_sensors_lock_;
  std::vector<SleipnirSensor> sleipnir_sensors_
      ABSL_GUARDED_BY(sleipnir_sensors_lock_);

  mutable absl::Mutex chassis_lock_;
  std::vector<ChassisId> chassis_ ABSL_GUARDED_BY(chassis_lock_);

  mutable absl::Mutex nvme_plugins_lock_;
  // Maps the physical location string to NVMe instance.
  absl::flat_hash_map<std::string, NvmePlugin> nvme_plugins_
      ABSL_GUARDED_BY(nvme_plugins_lock_);

  mutable absl::Mutex pci_storage_locations_lock_;
  std::vector<PciStorageLocation> pci_storage_locations_
      ABSL_GUARDED_BY(pci_storage_locations_lock_);

  mutable absl::Mutex pcie_device_locations_lock_;
  // This vector stores those real PCIe devices connected with valid PCIe links.
  std::vector<PciDbdfLocation> pcie_device_locations_
      ABSL_GUARDED_BY(pcie_device_locations_lock_);

  mutable absl::Mutex pci_tree_nodes_lock_;
  // This maps the pci locations to PCI node.
  PciTopologyInterface::PciNodeMap pci_tree_nodes_
      ABSL_GUARDED_BY(pci_tree_nodes_lock_);

  std::unique_ptr<SystemEventLogger> event_logger_;

  const absl::Span<const PciSensorParams> dimm_thermal_params_;
  const absl::Span<const IpmiInterface::BmcSensorInterfaceInfo> ipmi_sensors_;

  absl::optional<uint32_t> boot_number_;
  // Iterate through BIOS Event Log to load boot number from events
  void LoadBootNumberFromElogReader(ElogReader &elog_reader);
};

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_SYSMODEL_X86_SYSMODEL_H_
