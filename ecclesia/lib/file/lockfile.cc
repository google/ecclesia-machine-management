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

#include "ecclesia/lib/file/lockfile.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>

#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "ecclesia/lib/status/macros.h"
#include "ecclesia/lib/status/posix.h"

namespace ecclesia {

LockedLockFile::LockedLockFile(LockFile *lock_file) : lock_file_(lock_file) {}

LockedLockFile::LockedLockFile(LockedLockFile &&other)
    : lock_file_(other.lock_file_) {
  other.lock_file_ = nullptr;
}

LockedLockFile &LockedLockFile::operator=(LockedLockFile &&other) {
  if (this != &other) {
    lock_file_ = other.lock_file_;
    other.lock_file_ = nullptr;
  }
  return *this;
}

LockedLockFile::~LockedLockFile() {
  if (lock_file_) {
    lock_file_->Unlock();
  }
}

absl::StatusOr<std::string> LockedLockFile::Read() {
  return lock_file_->Read();
}

absl::Status LockedLockFile::Write(absl::string_view value) {
  return lock_file_->Write(value);
}

absl::Status LockedLockFile::Clear() { return lock_file_->Clear(); }

absl::StatusOr<std::unique_ptr<LockFile>> LockFile::Create(std::string path) {
  auto lock_file = absl::WrapUnique(new LockFile(std::move(path)));
  ECCLESIA_RETURN_IF_ERROR(lock_file->Open());
  return lock_file;
}

LockFile::LockFile(std::string path) : fd_(-1), path_(std::move(path)) {}

LockFile::~LockFile() { Close(); }

absl::StatusOr<LockedLockFile> LockFile::TryLock() {
  if (flock(fd_, LOCK_EX | LOCK_NB) != 0) {
    return PosixErrorToStatus(
        absl::StrFormat("unable to lock file %s ", path_));
  }

  return LockedLockFile(this);
}

absl::StatusOr<absl::Time> LockFile::GetModTime() {
  struct stat st;
  if (stat(path_.c_str(), &st) != 0) {
    return PosixErrorToStatus(
        absl::StrFormat("unable to get stat from file: %s", path_));
  }
  return absl::FromUnixSeconds(st.st_mtime);
}

absl::Status LockFile::Open() {
  fd_ = open(path_.c_str(), O_CREAT | O_RDWR, 0600);
  if (fd_ < 0) {
    return PosixErrorToStatus(
        absl::StrFormat("unable to open lockfile %s", path_));
  }

  return absl::OkStatus();
}

void LockFile::Close() {
  if (fd_ >= 0) {
    close(fd_);
  }
}

absl::StatusOr<std::string> LockFile::Read() {
  if (lseek(fd_, 0, SEEK_SET) != 0) {
    return PosixErrorToStatus(
        absl::StrFormat("unable to read lockfile %s", path_));
  }

  std::string value;
  while (true) {
    char buffer[4096];
    const ssize_t n = read(fd_, buffer, sizeof(buffer));
    if (n < 0) {
      if (errno == EINTR) continue;
      return PosixErrorToStatus(
          absl::StrFormat("failed to read data from: %s", path_));
    } else if (n == 0) {
      break;  // Nothing left to read.
    } else {
      value.append(buffer, n);
    }
  }
  return value;
}

absl::Status LockFile::Write(absl::string_view value) {
  if (ftruncate(fd_, 0) == 0 && lseek(fd_, 0, SEEK_SET) == 0) {
    const char *data = value.data();
    size_t size = value.size();
    while (size > 0) {
      ssize_t write_size = write(fd_, data, size);
      if (write_size < 0) {
        if (errno == EINTR) continue;
        return PosixErrorToStatus(
            absl::StrFormat("failed to write data to %s", path_));
      }
      size -= write_size;
      data += write_size;
    }
  }
  if (fsync(fd_) != 0) {
    return PosixErrorToStatus(
        absl::StrFormat("unable to write to lockfile %s", path_));
  }
  return absl::OkStatus();
}

absl::Status LockFile::Clear() {
  if (ftruncate(fd_, 0) != 0) {
    return PosixErrorToStatus(
        absl::StrFormat("unable to clear contents of lockfile %s", path_));
  }
  if (fsync(fd_) != 0) {
    return PosixErrorToStatus(
        absl::StrFormat("unable to clear lockfile %s", path_));
  }
  return absl::OkStatus();
}

void LockFile::Unlock() { flock(fd_, LOCK_UN); }

}  // namespace ecclesia
