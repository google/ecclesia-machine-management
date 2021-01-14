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

// Define a system event reader to read BIOS Event Log records from. This
// implementation only supports memory mapped access to the Elog, and supports
// the Google OEM Elog header format (0x81).

#ifndef ECCLESIA_MAGENT_LIB_EVENT_READER_ELOG_READER_H_
#define ECCLESIA_MAGENT_LIB_EVENT_READER_ELOG_READER_H_

#include <memory>
#include <queue>
#include <string>

#include "absl/types/optional.h"
#include "ecclesia/lib/smbios/system_event_log.h"
#include "ecclesia/magent/lib/event_reader/event_reader.h"

namespace ecclesia {

class ElogReader : public SystemEventReader {
 public:
  // The constructor takes in the smbios system event log structure and path to
  // a device file (usually /dev/mem) to read the Elog from
  ElogReader(std::unique_ptr<SystemEventLog> system_event_log,
             const std::string &mem_file);

  absl::optional<SystemEventRecord> ReadEvent() override {
    if (elogs_.empty()) return absl::nullopt;
    SystemEventRecord event{elogs_.front()};
    elogs_.pop();
    return event;
  }

 private:
  std::queue<SystemEventRecord> elogs_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_LIB_EVENT_READER_ELOG_READER_H_
