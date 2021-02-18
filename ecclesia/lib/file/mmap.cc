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

#include "ecclesia/lib/file/mmap.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cstddef>
#include <string>

#include "absl/cleanup/cleanup.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/posix.h"

namespace ecclesia {

absl::StatusOr<MappedMemory> MappedMemory::Create(const std::string &path,
                                                  size_t offset, size_t size,
                                                  Type type) {
  // Parameters for the open() and mmap() calls.
  int open_flags, mmap_prot, mmap_flags;
  bool writable;
  switch (type) {
    case Type::kReadOnly:
      open_flags = O_RDONLY;
      mmap_prot = PROT_READ;
      mmap_flags = MAP_PRIVATE;
      writable = false;
      break;
    case Type::kReadWrite:
      open_flags = O_RDWR;
      mmap_prot = PROT_READ | PROT_WRITE;
      mmap_flags = MAP_SHARED;
      writable = true;
      break;
  }
  // Open the file for reading.
  const int fd = open(path.c_str(), open_flags);
  if (fd == -1) {
    return absl::InternalError(
        absl::StrFormat("unable to open the file: %s", path));
  }
  auto fd_closer = absl::MakeCleanup([fd]() { close(fd); });

  // Determine the offset and length to use for the memory mapping, rounding
  // to an appropriate page size.
  size_t true_offset = offset - (offset % sysconf(_SC_PAGE_SIZE));
  size_t user_offset = offset - true_offset;
  size_t true_size = size + user_offset;
  // Create the memory mapping.
  void *addr = mmap(nullptr, true_size, mmap_prot, mmap_flags, fd, true_offset);
  if (addr == MAP_FAILED) {
    return absl::InternalError(
        absl::StrFormat("unable to mmap the file: %s", path));
  }
  // We have a good mapping. Note that we no longer need to keep the fd open.
  return MappedMemory({addr, true_size, user_offset, size, writable});
}

MappedMemory::~MappedMemory() {
  if (mapping_.addr) {
    if (munmap(mapping_.addr, mapping_.size) == -1) {
      PosixErrorLog() << "munmap() of " << mapping_.addr << ", "
                      << mapping_.size << " failed";
    }
  }
}

absl::string_view MappedMemory::MemoryAsStringView() const {
  return absl::string_view(
      static_cast<char *>(mapping_.addr) + mapping_.user_offset,
      mapping_.user_size);
}

MappedMemory::MappedMemory(MmapInfo mapping) : mapping_(mapping) {}

}  // namespace ecclesia
