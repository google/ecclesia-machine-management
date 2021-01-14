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

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstddef>
#include <memory>
#include <queue>
#include <string>

#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/lib/smbios/structures.emb.h"
#include "ecclesia/lib/smbios/system_event_log.h"
#include "ecclesia/magent/lib/event_reader/elog.emb.h"
#include "ecclesia/magent/lib/event_reader/event_reader.h"
#include "runtime/cpp/emboss_enum_view.h"
#include "runtime/cpp/emboss_prelude.h"

namespace ecclesia {

namespace {
// (Google) OEM Header format
constexpr uint8_t kElogHeaderFormat = 0x81;

// RAII wrapper to perform mmap. The destructor calls munmap to unmap the
// mapped memory
class ScopedMmap {
 public:
  // length: length of the mapping starting with offset within the file
  // prot: passed on to mmap
  // flags: passed onto mmap
  // offset: offset within the file to map from
  // filename: file to be mmaped
  ScopedMmap(size_t length, int prot, int flags, const std::string &filename,
             off_t offset)
      : addr_(nullptr), mapping_length_(0) {
    int file_flags = 0;
    // derive file access permissions from mmap's flags
    if ((prot & PROT_READ) && (prot & PROT_WRITE)) {
      file_flags = O_RDWR;
    } else if (prot & PROT_WRITE) {
      file_flags = O_WRONLY;
    } else {
      // default to read only
      file_flags = O_RDONLY;
    }
    int fd = open(filename.c_str(), file_flags);
    if (fd < 0) {
      ErrorLog() << "Unable to open " << filename;
      return;
    }
    auto page_size = sysconf(_SC_PAGE_SIZE);
    uint32_t page_base = (offset / page_size) * page_size;
    uint32_t page_offset = offset - page_base;
    mapping_length_ = length + page_offset;
    addr_ = reinterpret_cast<uint8_t *>(
        mmap(NULL, mapping_length_, prot, flags, fd, page_base));
    // Can safely close the file descriptor once mmaped
    close(fd);
    if (addr_) addr_ += page_offset;
  }

  // Returns nullptr if there was a failure in memory mapping the file
  uint8_t *GetMappedAddress() const { return addr_; }

  ~ScopedMmap() {
    if (addr_) munmap(addr_, mapping_length_);
  }

 private:
  uint8_t *addr_;
  size_t mapping_length_;
};

// Parses a series of elog records starting at start_addr and returns them in
// a queue. Parsing stops either when END_OF_LOG event is reached or "size"
// number of bytes have been consumed.
std::queue<SystemEventRecord> ParseElogRecords(const uint8_t *start_addr,
                                               size_t size) {
  std::queue<SystemEventRecord> elogs;
  const uint8_t *const end_addr = start_addr + size;
  while (start_addr < end_addr) {
    auto elog_view = MakeElogRecordView(start_addr, end_addr - start_addr);
    if (!elog_view.Ok()) break;
    // LOG_AREA_RESET implies the previous records have been invalidated
    if (elog_view.id().Read() == EventType::LOG_AREA_RESET) {
      elogs = std::queue<SystemEventRecord>();
    }
    elogs.push({.record = Elog(elog_view)});
    // stop if the last record is found
    if (elog_view.id().Read() == EventType::END_OF_LOG) break;
    start_addr += elog_view.size().Read();
  }
  return elogs;
}

// Validates whether the elog format / access mechamism specified in the
// SystemEventLog structure is supported by the ElogReader
bool ValidateSmbiosSystemEventLog(SystemEventLog *system_event_log) {
  if (!system_event_log) return false;
  auto view = system_event_log->GetMessageView();
  // This elog reader only supports pasring Google defined Elog header.
  // Only memory mapped io access is supported as yet
  return view.Ok() && view.log_header_format().Read() == kElogHeaderFormat &&
         view.access_method().Read() == AccessMethod::MEMORY_MAPPED_IO &&
         system_event_log->GetLogHeaderLength() > 0;
}

}  // namespace

ElogReader::ElogReader(std::unique_ptr<SystemEventLog> system_event_log,
                       const std::string &mem_file) {
  if (!ValidateSmbiosSystemEventLog(system_event_log.get())) {
    ErrorLog() << "Error validating smbios system event log structure";
    return;
  }
  auto view = system_event_log->GetMessageView();
  auto header_length = system_event_log->GetLogHeaderLength();
  uint32_t elog_address = view.access_method_address().Read() +
                          view.log_header_start_offset().Read();

  ScopedMmap memory_map(view.log_area_length().Read(), PROT_READ, MAP_PRIVATE,
                        mem_file, elog_address);

  if (!memory_map.GetMappedAddress()) {
    ErrorLog() << "Failure memory mapping " << mem_file;
    return;
  }
  // The log area starts with the header
  auto header_view =
      MakeElogHeaderView(memory_map.GetMappedAddress(), header_length);
  if (!header_view.Ok()) {
    ErrorLog() << "Error parsing Elog Header";
    return;
  }
  // Following the header are the log records
  elogs_ = ParseElogRecords(memory_map.GetMappedAddress() + header_length,
                            view.log_area_length().Read() - header_length);
}

}  // namespace ecclesia
