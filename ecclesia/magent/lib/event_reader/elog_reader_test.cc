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

#include "ecclesia/magent/lib/event_reader/elog_reader.h"

#include <assert.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/smbios/internal.h"
#include "ecclesia/lib/smbios/structures.emb.h"
#include "ecclesia/lib/smbios/system_event_log.h"
#include "ecclesia/magent/lib/event_reader/elog.emb.h"
#include "ecclesia/magent/lib/event_reader/event_reader.h"
#include "runtime/cpp/emboss_cpp_util.h"
#include "runtime/cpp/emboss_prelude.h"

namespace ecclesia {
namespace {

constexpr absl::string_view kElogFile =
    "magent/lib/event_reader/test_data/elog.bin";

class ElogReaderTest : public ::testing::Test {
 protected:
  ElogReaderTest() { SetUpSystemEventLog(); }

  // Set of variables to setup the SMBIOS SystemEventLog.
  std::unique_ptr<TableEntry> table_entry_;
  std::unique_ptr<SystemEventLog> system_event_log_;
  std::vector<uint8_t> system_event_log_entry_;

 private:
  void SetUpSystemEventLog() {
    // (Type 15) SMBIOS System Event Log structure. Instead of setting up the
    // entire SMBIOS table, we just setup this structure for the ElogReader

    const size_t smbios_struct_size = SmbiosStructure::MaxSizeInBytes();
    // + 1 for the unformed section that follows the struct
    system_event_log_entry_.resize(smbios_struct_size + 1, 0);

    auto smbios_struct_view = MakeSmbiosStructureView(
        system_event_log_entry_.data(), system_event_log_entry_.size());

    // Setup the SystemEventLog structure to match the dmidecode
    // Handle 0x0038, DMI type 15, 25 bytes
    // System Event Log
    // » Area Length: 65535 bytes
    // » Header Start Offset: 0x0000
    // » Header Length: 12 bytes
    // » Data Start Offset: 0x000C
    // » Access Method: Memory-mapped physical 32-bit address
    // » Access Address: 0x745F7018
    // » Status: Valid, Not Full
    // » Change Token: 0x00000000
    // » Header Format: OEM-specific
    // » Supported Log Type Descriptors: 1
    // » Descriptor 1: <OUT OF SPEC>
    // » Data Format 1: None

    smbios_struct_view.structure_type().Write(StructureType::SYSTEM_EVENT_LOG);
    smbios_struct_view.length().Write(smbios_struct_size);
    auto event_log_view = smbios_struct_view.system_event_log();
    event_log_view.log_area_length().Write(65535);
    event_log_view.log_header_start_offset().Write(0);
    event_log_view.log_data_start_offset().Write(12);
    event_log_view.access_method().Write(AccessMethod::MEMORY_MAPPED_IO);
    event_log_view.log_header_format().Write(0x81);
    event_log_view.log_area_valid().Write(true);
    // The elog.bin begins with the elog header, so the address is 0.
    event_log_view.access_method_address().Write(0);
    // Now that the system event log structure is setup, validation should
    // succeed.
    assert(event_log_view.Ok());
    SmbiosStructureInfo info;
    info.formatted_data_start = system_event_log_entry_.data();
    info.unformed_data_start = info.formatted_data_start + smbios_struct_size;

    table_entry_ = std::make_unique<TableEntry>(info);
    system_event_log_ = std::make_unique<SystemEventLog>(table_entry_.get());
  }
};

// In this test we try to parse the BIOS Elog (elog.bin) which was extracted
// from an Indus server. The expectation is to match the event types given by
// gsys. The output of gsys is below for reference
// Expectation:
// 0 | 2019-12-11 18:08:47 | Log area cleared | 14480 | 0
// 1 | 2019-12-11 18:20:18 | BIOS Filesystem Location | 14480 |
// 0x00000000bf80b000
// 2 | 2019-12-11 18:21:29 | OS Shutdown | 14480 | Clean
// 3 | 2019-12-11 18:25:39 | System boot | 14481
// 4 | 2019-12-11 18:26:23 | BIOS End of POST | 14481
// 5 | 2019-12-12 11:38:59 | OS Shutdown | 14481 | Unintended Shutdown
// 6 | 2019-12-12 11:38:59 | System boot | 14482
// 7 | 2019-12-12 11:39:41 | BIOS End of POST | 14482

TEST_F(ElogReaderTest, SuccessReadingElogRecords) {
  EventType expected_types[] = {
      EventType::LOG_AREA_RESET,   EventType::BIOS_FILESYSTEM_LOCATION,
      EventType::OS_SHUTDOWN,      EventType::SYSTEM_BOOT,
      EventType::BIOS_END_OF_POST, EventType::OS_SHUTDOWN,
      EventType::SYSTEM_BOOT,      EventType::BIOS_END_OF_POST,
      EventType::END_OF_LOG,
  };

  ElogReader reader(std::move(system_event_log_),
                    GetTestDataDependencyPath(kElogFile));

  for (auto expected_type : expected_types) {
    EXPECT_EQ(expected_type, std::get<Elog>(reader.ReadEvent()->record)
                                 .GetElogRecordView()
                                 .id()
                                 .Read());
  }
  // No more records left to read
  EXPECT_FALSE(reader.ReadEvent());
}

}  // namespace

}  // namespace ecclesia
