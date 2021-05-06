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

#ifndef ECCLESIA_LIB_SMBIOS_READER_H_
#define ECCLESIA_LIB_SMBIOS_READER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "ecclesia/lib/smbios/bios.h"
#include "ecclesia/lib/smbios/internal.h"
#include "ecclesia/lib/smbios/memory_device.h"
#include "ecclesia/lib/smbios/processor_information.h"
#include "ecclesia/lib/smbios/reader_intf.h"
#include "ecclesia/lib/smbios/system_event_log.h"
#include "ecclesia/lib/smbios/system_information.h"

namespace ecclesia {

// Currently the implementation only supports parsing a 32-bit entry_pont
// structure. This can be easily extended to support a 64-bit entry point if
// needed. The reader should outlive any objects returned by it, since the
// objects contain TableEntry pointers which are owned by the reader.
class SmbiosReader : public SmbiosReaderInterface {
 public:
  // The constructor takes in path to the entry_point and the smbios tables
  // stored in sysfs as binary files.
  // On a standard linux implementation these paths are
  // /sys/firmware/dmi/tables/smbios_entry_point
  // /sys/firmware/dmi/tables/DMI
  SmbiosReader(std::string entry_point_path, std::string tables_path);
  virtual ~SmbiosReader() {}

  SmbiosReader(const SmbiosReader&) = delete;
  SmbiosReader& operator=(const SmbiosReader&) = delete;

  // Type 0 (Bios Information)
  std::unique_ptr<BiosInformation> GetBiosInformation() const override;

  // Type 1 (System Information)
  std::unique_ptr<SystemInformation> GetSystemInformation() const override;

  // Type 17 (Memory Device)
  // The returned vector is sorted on the device locator string
  std::vector<MemoryDevice> GetAllMemoryDevices() const override;

  // Type 15 (System Event Log)
  std::unique_ptr<SystemEventLog> GetSystemEventLog() const override;

  // Type 4 (Processor Information)
  std::vector<ProcessorInformation> GetAllProcessors() const override;

  // Read boot number from Type 32 (System Boot Information)
  absl::StatusOr<int32_t> GetBootNumberFromSystemBootInformation()
      const override;

 private:
  std::vector<TableEntry> entries_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_SMBIOS_READER_H_
