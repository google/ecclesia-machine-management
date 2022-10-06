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

#include "ecclesia/lib/file/test_filesystem.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/cleanup/cleanup.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/file/dir.h"
#include "ecclesia/lib/file/path.h"

namespace ecclesia {
namespace {

// Path of the ecclesia root, relative to the build environment WORKSPACE root.
constexpr absl::string_view kEcclesiaRoot = "com_google_ecclesia/ecclesia";

// Make a directory exist. This will create the directory if it does not exist
// and do nothing if it already does.
void MakeDirectoryExist(const std::string &path) {
  int rc = mkdir(path.c_str(), 0755);
  if (rc < 0 && errno != EEXIST) {
    PLOG(FATAL) << "mkdir() failed for dir " << path;
  }
}

// Opens and writes 'data' to a given file. The file will be opened using the
// open() call using the provided flags. Any error will be treated as fatal.
void OpenAndWriteFile(const std::string &path, int flags,
                      absl::string_view data) {
  // Open the file.
  int fd = open(path.c_str(), flags, S_IRWXU);
  if (fd == -1) {
    PLOG(FATAL) << "open() failed for file " << path;
  }
  absl::Cleanup fd_closer = [fd]() { close(fd); };

  // Write the file. Fail if the write fails, OR if it's too short.
  int rc = write(fd, data.data(), data.size());
  if (rc == -1) {
    PLOG(FATAL) << "write() failed for file " << path;
  } else if (rc < data.size()) {
    LOG(FATAL) << "write() was unable to write the full contents to " << path;
  }
}

// Read in the contents of a path. Crashes and logs if the read fails.
std::string PathContents(const std::string &path) {
  // Open the file.
  int fd = open(path.c_str(), O_RDONLY);
  if (fd == -1) {
    LOG(FATAL) << "open() was unable to return file descriptor: " << path;
  }
  absl::Cleanup fd_closer = [fd]() { close(fd); };

  // Read from the file in 4k chunks until read returns 0, or fails.
  char buffer[4096];
  std::string contents;
  while (ssize_t rc = read(fd, buffer, sizeof(buffer))) {
    if (rc == -1) PLOG(FATAL) << "read() failed for file " << path;
    contents.append(buffer, rc);
  }
  return contents;
}

// Recursively remove everything in the given directory.
void RemoveDirectoryTree(const std::string &path) {
  WithEachFileInDirectory(path, [&path](absl::string_view entry) {
    std::string full_path = JoinFilePaths(path, entry);
    // Try to remove the entry.
    int rc = remove(full_path.c_str());
    // If it failed because it was a non-empty directory, do a recursive call
    // into that directory.
    if (rc == -1 && errno == ENOTEMPTY) {
      RemoveDirectoryTree(full_path);
      rc = remove(full_path.c_str());
    }
    // If the last remove failed the we can't recover.
    if (rc == -1) {
      PLOG(FATAL) << "remove() failed for path " << path;
    }
  }).IgnoreError();
}

}  // namespace

std::string GetTestDataDependencyPath(absl::string_view path) {
  char *srcdir = std::getenv("TEST_SRCDIR");
  if (srcdir) {
    return JoinFilePaths(srcdir, kEcclesiaRoot, path);
  }
#if defined(__GLIBC__)
  // Check for runfiles in the directory of the binary itself
  if (program_invocation_name != nullptr) {
    // Note that we don't want to use JoinFilePaths here: the runfiles directory
    // is the same name as the program invocation but with ".runfiles" appended.
    std::string runfiles_srcdir =
        absl::StrCat(program_invocation_name, ".runfiles");
    struct stat st;
    if (stat(runfiles_srcdir.c_str(), &st) == 0) {
      return JoinFilePaths(runfiles_srcdir, kEcclesiaRoot, path);
    }
  }
  LOG(FATAL) << "TEST_SRCDIR environment variable was not defined, and "
                ".runfiles directory was not found.";
#else
  LOG(FATAL) << "TEST_SRCDIR environment variable was not defined";
#endif
}

std::string GetTestTempdirPath() {
  char *tmpdir = std::getenv("TEST_TMPDIR");
  CHECK(tmpdir) << "TEST_TMPDIR environment variable is defined";
  return tmpdir;
}

std::string GetTestTempdirPath(absl::string_view path) {
  return JoinFilePaths(GetTestTempdirPath(), path);
}

std::string GetTestTempUdsDirectory() {
  char tempdir_buffer[] = "/tmp/socket_dir.XXXXXX";
  if (mkdtemp(tempdir_buffer) == nullptr) {
    LOG(FATAL) << "unable to create a temporary directory";
  }
  return tempdir_buffer;
}

TestFilesystem::TestFilesystem(std::string root) : root_(std::move(root)) {
  MakeDirectoryExist(root_);
}

TestFilesystem::~TestFilesystem() { RemoveAllContents(); }

std::string TestFilesystem::GetTruePath(absl::string_view path) const {
  CHECK(path[0] == '/') << "path is absolute, path=" << path;
  while (!path.empty() && path[0] == '/') path.remove_prefix(1);
  return JoinFilePaths(root_, path);
}

void TestFilesystem::CreateDir(absl::string_view path) {
  CHECK(!path.empty());
  CHECK(path[0] == '/') << "path is absolute, path=" << path;

  // Break the path up into components to be created.
  std::vector<absl::string_view> parts =
      absl::StrSplit(path, '/', absl::SkipEmpty());

  // Start from the root and create the components, piece by piece.
  std::string current_path = root_;
  for (absl::string_view part : parts) {
    absl::StrAppend(&current_path, "/", part);
    MakeDirectoryExist(current_path);
  }
}

void TestFilesystem::CreateFile(absl::string_view path,
                                absl::string_view data) {
  CHECK(!path.empty());

  // Open and write, treating an existing file as an open() error.
  OpenAndWriteFile(GetTruePath(path), O_CREAT | O_EXCL | O_WRONLY, data);
}

void TestFilesystem::WriteFile(absl::string_view path, absl::string_view data) {
  CHECK(!path.empty());

  // Open and write, truncating any existing file to empty.
  OpenAndWriteFile(GetTruePath(path), O_CREAT | O_TRUNC | O_WRONLY, data);
}

std::string TestFilesystem::ReadFile(absl::string_view path) {
  CHECK(!path.empty());

  // Open and read file.
  return PathContents(GetTruePath(path));
}

void TestFilesystem::CreateSymlink(absl::string_view target,
                                   absl::string_view link_path) {
  CHECK(!target.empty());
  CHECK(!link_path.empty());

  // Construct the true target path.
  std::string full_target =
      target[0] == '/' ? GetTruePath(target) : std::string(target);

  // Create the symlink.
  int rc = symlink(full_target.c_str(), GetTruePath(link_path).c_str());
  if (rc == -1) {
    PLOG(FATAL) << "symlink() failed for " << link_path << " -> "
                << "target";
  }
}

void TestFilesystem::RemoveAllContents() { RemoveDirectoryTree(root_); }

}  // namespace ecclesia
