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

#include "ecclesia/lib/smbios/reader.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>  // IWYU pragma: keep
#include <memory>
#include <string>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/lib/smbios/baseboard_information.h"
#include "ecclesia/lib/smbios/bios.h"
#include "ecclesia/lib/smbios/entry_point.emb.h"
#include "ecclesia/lib/smbios/internal.h"
#include "ecclesia/lib/smbios/memory_device.h"
#include "ecclesia/lib/smbios/processor_information.h"
#include "ecclesia/lib/smbios/structures.emb.h"
#include "ecclesia/lib/smbios/system_event_log.h"
#include "ecclesia/lib/smbios/system_information.h"
#include "ecclesia/lib/strings/natural_sort.h"
#include "runtime/cpp/emboss_enum_view.h"
#include "runtime/cpp/emboss_prelude.h"

namespace ecclesia {

namespace {

// Define the maximum size of the entry point structure.
// 32-bit entry point length is 0x1f, 64-bit entry point length is 0x18
// So pick the greater of the two entry point structures.
constexpr uint8_t ENTRY_POINT_MAX_SIZE = 0x1f;

// Read the contents of a binary file into a vector of bytes
// The SMBIOS table is on an order of a few KBs (< 10). Therefore it's safe to
// return the entire contents of the file at once.
std::vector<uint8_t> GetBinaryFileContents(const std::string &file_path,
                                           std::size_t max_size) {
  std::ifstream file(file_path,
                     std::ios::in | std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    ErrorLog() << "failed to open file " << file_path;
    return std::vector<uint8_t>();
  }
  auto size = file.tellg();
  if (size > max_size) return std::vector<uint8_t>();
  std::vector<uint8_t> contents(size);
  file.seekg(0, std::ios::beg);
  file.read(reinterpret_cast<char *>(contents.data()), size);
  file.close();
  return contents;
}

// The data array includes the checksum. 8-bit addition of all the bytes should
// be equal to zero.
template <typename View>
inline bool VerifyChecksum(View message_view) {
  uint8_t sum = 0;
  for (auto byte_view :
       message_view.entry_point_32bit().Ok()
           ? message_view.entry_point_32bit().checksum_bytes()
           : message_view.entry_point_64bit().checksum_bytes()) {
    sum += byte_view.Read();
  }
  return (sum == 0);
}

// Extract the information for a single SMBIOS structure into `info`
bool ExtractSmbiosStructure(uint8_t *start_address, std::size_t max_length,
                            SmbiosStructureInfo *info) {
  auto smbios_structure_view =
      MakeSmbiosStructureView(start_address, max_length);

  if (!smbios_structure_view.Ok()) {
    ErrorLog() << "Failure parsing smbios structure";
    return false;
  }

  info->formatted_data_start = start_address;

  // End of a structure should have double-null characters.
  // Search for the end of structure
  std::size_t index = smbios_structure_view.SizeInBytes();
  info->unformed_data_start = &start_address[index];
  while (index < (max_length - 1)) {
    if (!(start_address[index] | start_address[index + 1])) {
      // Found the end of structure
      // index is at the first null character of the double-null sequence
      info->structure_size = index + 2;
      return true;
    }
    index++;
  }
  return false;
}

std::vector<TableEntry> BuildEntries(std::vector<uint8_t> &table_data) {
  std::vector<TableEntry> entries;

  // Start extracting the SMBIOS structures one by one
  uint8_t *start_address = table_data.data();
  const uint8_t *end_address = start_address + table_data.size() - 1;

  while (start_address <= end_address) {
    std::size_t max_length = end_address - start_address + 1;
    SmbiosStructureInfo info;
    if (!ExtractSmbiosStructure(start_address, max_length, &info)) {
      ErrorLog() << "Error extracting SMBIOS structure";
      return entries;
    }
    entries.emplace_back(info);
    start_address += info.structure_size;
  }
  return entries;
}
}  // namespace

SmbiosReader::SmbiosReader(std::string entry_point_path,
                           std::string tables_path) {
  std::vector<uint8_t> entry_point_data =
      GetBinaryFileContents(entry_point_path, ENTRY_POINT_MAX_SIZE);

  auto entry_point_view =
      MakeEntryPointView(entry_point_data.data(), entry_point_data.size());

  if (!(entry_point_view.entry_point_32bit().Ok() ||
        entry_point_view.entry_point_64bit().Ok())) {
    ErrorLog() << "Failure parsing entry point structure";
    return;
  }

  if (!VerifyChecksum(entry_point_view)) {
    ErrorLog() << "Entry point checksum verification failed.";
    return;
  }

  std::size_t structure_table_max_size =
      entry_point_view.entry_point_32bit().Ok()
          ? entry_point_view.entry_point_32bit().structure_table_length().Read()
          : entry_point_view.entry_point_64bit()
                .structure_table_max_size()
                .Read();
  // Read the SMBIOS tables
  std::vector<uint8_t> table_data =
      GetBinaryFileContents(tables_path, structure_table_max_size);

  entries_ = BuildEntries(table_data);
}

SmbiosReader::SmbiosReader(std::vector<uint8_t> &table_data) {
  entries_ = BuildEntries(table_data);
}

std::unique_ptr<BiosInformation> SmbiosReader::GetBiosInformation() const {
  for (auto &entry : entries_) {
    if (entry.GetSmbiosStructureView().structure_type().Read() ==
        StructureType::BIOS_INFORMATION) {
      return std::make_unique<BiosInformation>(&entry);
    }
  }
  return nullptr;
}

std::unique_ptr<SystemInformation> SmbiosReader::GetSystemInformation() const {
  for (auto &entry : entries_) {
    if (entry.GetSmbiosStructureView().structure_type().Read() ==
        StructureType::SYSTEM_INFORMATION) {
      return std::make_unique<SystemInformation>(&entry);
    }
  }
  return nullptr;
}

std::unique_ptr<BaseboardInformation> SmbiosReader::GetBaseboardInformation()
    const {
  for (auto &entry : entries_) {
    if (entry.GetSmbiosStructureView().structure_type().Read() ==
        StructureType::BASEBOARD_INFORMATION) {
      return std::make_unique<BaseboardInformation>(&entry);
    }
  }
  return nullptr;
}

std::vector<MemoryDevice> SmbiosReader::GetAllMemoryDevices() const {
  std::vector<MemoryDevice> memory_devices;

  for (auto &entry : entries_) {
    if (entry.GetSmbiosStructureView().structure_type().Read() ==
        StructureType::MEMORY_DEVICE) {
      memory_devices.emplace_back(&entry);
    }
  }
  // Sort the memory devices based on the device locator string
  std::sort(memory_devices.begin(), memory_devices.end(), [](auto &x, auto &y) {
    return NaturalSortLessThan(
        x.GetString(x.GetMessageView().device_locator_snum().Read()),
        y.GetString(y.GetMessageView().device_locator_snum().Read()));
  });
  return memory_devices;
}

std::unique_ptr<SystemEventLog> SmbiosReader::GetSystemEventLog() const {
  for (auto &entry : entries_) {
    if (entry.GetSmbiosStructureView().structure_type().Read() ==
        StructureType::SYSTEM_EVENT_LOG) {
      return std::make_unique<SystemEventLog>(&entry);
    }
  }
  return nullptr;
}

std::vector<ProcessorInformation> SmbiosReader::GetAllProcessors() const {
  std::vector<ProcessorInformation> processor_information;

  for (auto &entry : entries_) {
    if (entry.GetSmbiosStructureView().structure_type().Read() ==
        StructureType::PROCESSOR_INFORMATION) {
      processor_information.emplace_back(&entry);
    }
  }
  // Sort the processors based on the socket designation
  std::sort(
      processor_information.begin(), processor_information.end(),
      [](auto &x, auto &y) {
        return x.GetString(
                   x.GetMessageView().socket_designation_snum().Read()) <
               y.GetString(y.GetMessageView().socket_designation_snum().Read());
      });
  return processor_information;
}

absl::StatusOr<int32_t> SmbiosReader::GetBootNumberFromSystemBootInformation()
    const {
  for (const auto &entry : entries_) {
    const auto &structure_view = entry.GetSmbiosStructureView();
    if (structure_view.structure_type().Read() ==
            StructureType::SYSTEM_BOOT_INFORMATION &&
        structure_view.system_boot_information().Ok()) {
      const auto &system_boot_info = structure_view.system_boot_information();
      if (system_boot_info.boot_count().Ok()) {
        return system_boot_info.boot_count().Read();
      } else {
        return absl::NotFoundError(
            "System Boot Information did not contain boot count");
      }
    }
  }
  return absl::NotFoundError("No System boot information found");
}

}  // namespace ecclesia
