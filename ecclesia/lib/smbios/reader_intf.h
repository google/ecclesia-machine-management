/*
 * Copyright 2021 Google LLC
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

#ifndef ECCLESIA_LIB_SMBIOS_READER_INTF_H_
#define ECCLESIA_LIB_SMBIOS_READER_INTF_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/status/statusor.h"
#include "ecclesia/lib/smbios/bios.h"
#include "ecclesia/lib/smbios/internal.h"
#include "ecclesia/lib/smbios/memory_device.h"
#include "ecclesia/lib/smbios/processor_information.h"
#include "ecclesia/lib/smbios/system_event_log.h"
#include "ecclesia/lib/smbios/system_information.h"

namespace ecclesia {

class SmbiosReaderInterface {
 public:
  virtual ~SmbiosReaderInterface() {}

  // Type 0 (Bios Information)
  virtual std::unique_ptr<BiosInformation> GetBiosInformation() const = 0;

  // Type 1 (System Information)
  virtual std::unique_ptr<SystemInformation> GetSystemInformation() const = 0;

  // Type 17 (Memory Device)
  // The returned vector is sorted on the device locator string
  virtual std::vector<MemoryDevice> GetAllMemoryDevices() const = 0;

  // Type 15 (System Event Log)
  virtual std::unique_ptr<SystemEventLog> GetSystemEventLog() const = 0;

  // Type 4 (Processor Information)
  virtual std::vector<ProcessorInformation> GetAllProcessors() const = 0;

  // Read boot number from Type 32 (System Boot Information)
  virtual absl::StatusOr<int32_t> GetBootNumberFromSystemBootInformation()
      const = 0;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_SMBIOS_READER_INTF_H_
