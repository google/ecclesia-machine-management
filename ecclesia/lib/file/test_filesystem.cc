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
#include <string>
#include <utility>
#include <vector>

#include "absl/cleanup/cleanup.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/file/dir.h"
#include "ecclesia/lib/file/path.h"
#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/lib/logging/posix.h"

namespace ecclesia {
namespace {

// Path of the ecclesia root, relative to the build environment WORKSPACE root.
constexpr absl::string_view kEcclesiaRoot = "com_google_ecclesia/ecclesia";

// Make a directory exist. This will create the directory if it does not exist
// and do nothing if it already does.
void MakeDirectoryExist(const std::string &path) {
  int rc = mkdir(path.c_str(), 0755);
  if (rc < 0 && errno != EEXIST) {
    PosixFatalLog() << "mkdir() failed for dir " << path;
  }
}

// Opens and writes 'data' to a given file. The file will be opened using the
// open() call using the provided flags. Any error will be treated as fatal.
void OpenAndWriteFile(const std::string &path, int flags,
                      absl::string_view data) {
  // Open the file.
  int fd = open(path.c_str(), flags, S_IRWXU);
  if (fd == -1) {
    PosixFatalLog() << "open() failed for file " << path;
  }
  auto fd_closer = absl::MakeCleanup([fd]() { close(fd); });

  // Write the file. Fail if the write fails, OR if it's too short.
  int rc = write(fd, data.data(), data.size());
  if (rc == -1) {
    PosixFatalLog() << "write() failed for file " << path;
  } else if (rc < data.size()) {
    FatalLog() << "write() was unable to write the full contents to " << path;
  }
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
      PosixFatalLog() << "remove() failed for path " << path;
    }
  }).IgnoreError();
}

}  // namespace

std::string GetTestDataDependencyPath(absl::string_view path) {
  char *srcdir = std::getenv("TEST_SRCDIR");
  Check(srcdir, "TEST_SRCDIR environment variable is defined");
  return JoinFilePaths(srcdir, kEcclesiaRoot, path);
}

std::string GetTestTempdirPath() {
  char *tmpdir = std::getenv("TEST_TMPDIR");
  Check(tmpdir, "TEST_TMPDIR environment variable is defined");
  return tmpdir;
}

std::string GetTestTempdirPath(absl::string_view path) {
  return JoinFilePaths(GetTestTempdirPath(), path);
}

std::string GetTestTempUdsDirectory() {
  char tempdir_buffer[] = "/tmp/socket_dir.XXXXXX";
  if (mkdtemp(tempdir_buffer) == nullptr) {
    FatalLog() << "unable to create a temporary directory";
  }
  return tempdir_buffer;
}

TestFilesystem::TestFilesystem(std::string root) : root_(std::move(root)) {
  MakeDirectoryExist(root_);
}

TestFilesystem::~TestFilesystem() { RemoveAllContents(); }

std::string TestFilesystem::GetTruePath(absl::string_view path) const {
  Check(path[0] == '/', "path is absolute") << "path=" << path;
  while (!path.empty() && path[0] == '/') path.remove_prefix(1);
  return JoinFilePaths(root_, path);
}

void TestFilesystem::CreateDir(absl::string_view path) {
  CheckCondition(!path.empty());
  Check(path[0] == '/', "path is absolute") << "path=" << path;

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
  CheckCondition(!path.empty());

  // Open and write, treating an existing file as an open() error.
  OpenAndWriteFile(GetTruePath(path), O_CREAT | O_EXCL | O_WRONLY, data);
}

void TestFilesystem::WriteFile(absl::string_view path, absl::string_view data) {
  CheckCondition(!path.empty());

  // Open and write, truncating any existing file to empty.
  OpenAndWriteFile(GetTruePath(path), O_CREAT | O_TRUNC | O_WRONLY, data);
}

void TestFilesystem::CreateSymlink(absl::string_view target,
                                   absl::string_view link_path) {
  CheckCondition(!target.empty());
  CheckCondition(!link_path.empty());

  // Construct the true target path.
  std::string full_target =
      target[0] == '/' ? GetTruePath(target) : std::string(target);

  // Create the symlink.
  int rc = symlink(full_target.c_str(), GetTruePath(link_path).c_str());
  if (rc == -1) {
    PosixFatalLog() << "symlink() failed for " << link_path << " -> "
                    << "target";
  }
}

void TestFilesystem::RemoveAllContents() { RemoveDirectoryTree(root_); }

}  // namespace ecclesia
