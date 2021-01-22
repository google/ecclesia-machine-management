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

#include "ecclesia/lib/apifs/apifs.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>   // IWYU pragma: keep
#include <sys/types.h>  // IWYU pragma: keep
#include <unistd.h>     // IWYU pragma: keep

#include <climits>
#include <cstddef>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ecclesia/lib/cleanup/cleanup.h"
#include "ecclesia/lib/file/dir.h"
#include "ecclesia/lib/file/path.h"
#include "ecclesia/lib/status/macros.h"

namespace ecclesia {
namespace {

// Returns if the given path (diretory or a file) exists.
bool PathExists(const std::string &path) {
  return access(path.c_str(), F_OK) == 0;
}

// List all the entries in a given directory and apply the given functor to get
// the return value for each entry.
absl::StatusOr<std::vector<std::string>> ListEntriesInDir(
    const std::string &dir_path,
    std::function<std::string(absl::string_view)> per_entry_func) {
  if (!PathExists(dir_path)) {
    return absl::NotFoundError(
        absl::StrFormat("Directory not found at path: %s", dir_path));
  }
  std::vector<std::string> entries;
  ECCLESIA_RETURN_IF_ERROR(
      WithEachFileInDirectory(dir_path, [&](absl::string_view entry) {
        entries.push_back(per_entry_func(entry));
      }));
  return entries;
}

}  // namespace

ApifsDirectory::ApifsDirectory() {}

ApifsDirectory::ApifsDirectory(std::string path) : dir_path_(std::move(path)) {}

ApifsDirectory::ApifsDirectory(const ApifsDirectory &directory,
                               std::string path)
    : dir_path_(JoinFilePaths(directory.dir_path_, path)) {}

bool ApifsDirectory::Exists() const { return PathExists(dir_path_); }

bool ApifsDirectory::Exists(std::string path) const {
  ApifsDirectory d(*this, std::move(path));
  return d.Exists();
}

absl::StatusOr<struct stat> ApifsDirectory::Stat(std::string path) const {
  ApifsFile f(*this, std::move(path));
  return f.Stat();
}

absl::StatusOr<std::vector<std::string>> ApifsDirectory::ListEntryNames()
    const {
  return ListEntriesInDir(
      dir_path_, [](absl::string_view entry) { return std::string(entry); });
}

absl::StatusOr<std::vector<std::string>> ApifsDirectory::ListEntryNames(
    std::string path) const {
  return ListEntriesInDir(
      JoinFilePaths(dir_path_, path),
      [](absl::string_view entry) { return std::string(entry); });
}

absl::StatusOr<std::vector<std::string>> ApifsDirectory::ListEntryPaths()
    const {
  return ListEntriesInDir(dir_path_, [&](absl::string_view entry) {
    return JoinFilePaths(this->dir_path_, entry);
  });
}

absl::StatusOr<std::vector<std::string>> ApifsDirectory::ListEntryPaths(
    std::string path) const {
  std::string full_path = JoinFilePaths(dir_path_, path);
  return ListEntriesInDir(full_path, [&full_path](absl::string_view entry) {
    return JoinFilePaths(full_path, entry);
  });
}

absl::StatusOr<std::string> ApifsDirectory::Read(std::string path) const {
  ApifsFile f(*this, std::move(path));
  return f.Read();
}

absl::Status ApifsDirectory::Write(std::string path,
                                   absl::string_view value) const {
  ApifsFile f(*this, std::move(path));
  return f.Write(value);
}

absl::StatusOr<std::string> ApifsDirectory::ReadLink(std::string path) const {
  ApifsFile f(*this, std::move(path));
  return f.ReadLink();
}

ApifsFile::ApifsFile() {}

ApifsFile::ApifsFile(std::string path) : path_(std::move(path)) {}

ApifsFile::ApifsFile(const ApifsDirectory &directory,
                     absl::string_view entry_relative_path)
    : path_(JoinFilePaths(directory.dir_path_, entry_relative_path)) {}

bool ApifsFile::Exists() const { return PathExists(path_); }

absl::StatusOr<struct stat> ApifsFile::Stat() const {
  struct stat st;
  if (stat(path_.c_str(), &st) < 0) {
    return absl::InternalError(absl::StrFormat(
        "failure while stat-ing file at path: %s, errno: %d", path_, errno));
  }
  return st;
}

absl::StatusOr<std::string> ApifsFile::Read() const {
  if (!Exists()) {
    return absl::NotFoundError(
        absl::StrFormat("File not found at path: %s", path_));
  }

  const int fd = open(path_.c_str(), O_RDONLY);
  if (fd < 0) {
    return absl::NotFoundError(absl::StrFormat(
        "unable to open the file at path: %s, errno: %d", path_, errno));
  }
  auto fd_closer = LambdaCleanup([fd]() { close(fd); });

  std::string value;
  while (true) {
    char buffer[4096];
    const ssize_t n = read(fd, buffer, sizeof(buffer));
    if (n < 0) {
      const auto read_errno = errno;
      if (read_errno == EINTR) {
        continue;  // Retry on EINTR.
      }
      return absl::InternalError(absl::StrFormat(
          "failure while reading from file at path: %s, errno: %d", path_,
          read_errno));
      break;
    } else if (n == 0) {
      break;  // Nothing left to read.
    } else {
      value.append(buffer, n);
    }
  }
  return value;
}

absl::Status ApifsFile::Write(absl::string_view value) const {
  if (!Exists()) {
    return absl::NotFoundError(
        absl::StrFormat("File not found at path: %s", path_));
  }
  const int fd = open(path_.c_str(), O_WRONLY);
  if (fd < 0) {
    return absl::NotFoundError(
        absl::StrFormat("unable to open the file at path: %s", path_));
  }
  auto fd_closer = LambdaCleanup([fd]() { close(fd); });
  const char *data = value.data();
  size_t size = value.size();
  while (size > 0) {
    ssize_t result = write(fd, data, size);
    if (result <= 0) {
      const auto write_errno = errno;
      if (write_errno == EINTR) continue;  // Retry on EINTR.
      return absl::InternalError(absl::StrFormat(
          "failure while writing to file at path: %s, errno: %d", path_,
          errno));
      break;
    }
    // We successfully wrote out 'result' bytes, advance the data pointer.
    size -= result;
    data += result;
  }
  return absl::OkStatus();
}

absl::Status ApifsFile::ReadRange(uint64_t offset,
                                  absl::Span<char> value) const {
  if (!Exists()) {
    return absl::NotFoundError(
        absl::StrFormat("File not found at path: %s", path_));
  }

  int fd = open(path_.c_str(), O_RDONLY);
  if (fd < 0) {
    return absl::NotFoundError(absl::StrFormat(
        "Unable to open the file at path: %s, errno: %d", path_, errno));
  }
  auto fd_closer = LambdaCleanup([fd]() { close(fd); });
  // Read data.
  size_t size = value.size();
  int rlen = pread(fd, value.data(), size, offset);
  if (rlen != size) {
    return absl::InternalError(absl::StrFormat(
        "Fail to read %d bytes from offset %#x. rlen: %d", size, offset, rlen));
  }
  return absl::OkStatus();
}

absl::Status ApifsFile::WriteRange(uint64_t offset,
                                   absl::Span<const char> value) const {
  if (!Exists()) {
    return absl::NotFoundError(
        absl::StrFormat("File not found at path: %s", path_));
  }
  const int fd = open(path_.c_str(), O_WRONLY);
  if (fd < 0) {
    return absl::NotFoundError(
        absl::StrFormat("Unable to open the file at path: %s", path_));
  }
  auto fd_closer = LambdaCleanup([fd]() { close(fd); });
  // Write data.
  size_t size = value.size();
  int wlen = pwrite(fd, value.data(), size, offset);
  if (wlen != size) {
    return absl::NotFoundError(
        absl::StrFormat("Failed to write %d bytes to msr %s", size, path_));
  }
  return absl::OkStatus();
}

absl::StatusOr<std::string> ApifsFile::ReadLink() const {
  // Do an lstat of the path to determine the link size and verify the file is
  // in fact a symlink.
  struct stat st;
  if (lstat(path_.c_str(), &st) < 0) {
    if (errno == ENOENT) {
      return absl::NotFoundError(
          absl::StrFormat("file not found at path: %s", path_));
    } else {
      return absl::InternalError(absl::StrFormat(
          "failure while lstat-ing file at path: %s, errno: %d", path_, errno));
    }
  }
  if (!S_ISLNK(st.st_mode)) {
    return absl::InvalidArgumentError(
        absl::StrFormat("path: %s is not a symlink", path_));
  }
  // Read the symlink using a std::string buffer size from the lstat. The +1 is
  // used for determining whether the buffer returned by readlink was truncated.
  ssize_t bufsize = st.st_size + 1;
  // Some symlinks under /proc and /sys report st.st_size as zero. In that case,
  // use PATH_MAX as a "good enough" estimate.
  if (st.st_size == 0) {
    bufsize = PATH_MAX;
  }
  std::string link(bufsize, '\0');
  ssize_t rc = readlink(path_.c_str(), &link[0], link.size());
  if (rc == -1) {
    return absl::InternalError(absl::StrFormat(
        "unable to read the link at path: %s, errno: %d", path_, errno));
  }
  if (rc >= bufsize) {
    // If this happens, it means the target symlinks filename size is larger
    // than expected. Perhaps because the target was changed between lstat() and
    // readlink(). Just consider that an error.
    return absl::InternalError(absl::StrFormat(
        "the link at: %s was changed while it was being read", path_));
  }
  // The first "rc" characters in the string were populated. Return that.
  return link.substr(0, rc);
}

}  // namespace ecclesia
