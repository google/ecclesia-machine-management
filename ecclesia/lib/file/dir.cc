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

#include "ecclesia/lib/file/dir.h"

#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdlib>
#include <stack>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/file/path.h"

namespace ecclesia {
namespace {

// Implement DataStoreDirectory::GetFileStats for a single file, but unlike the
// actual method this operates on a full path and it does not do any kind of "is
// this a registered filename" check.
DataStoreDirectory::Stats GetSingleFileStats(const std::string &path) {
  DataStoreDirectory::Stats stats;
  struct stat st;
  if (stat(path.c_str(), &st) == 0) {
    // We basically treat any kind of stat failure as "it doesn't exist". While
    // there are technically other ways that this could fail there's not much
    // that we could meaningfully distinguish so "exists=false" is good enough.
    stats.exists = true;
    stats.size = st.st_size;
  }
  return stats;
}

}  // namespace

std::string GetSystemTempdirPath() {
  // If we're in a test environment, use the test temporary directory.
  char *test_tmpdir = std::getenv("TEST_TMPDIR");
  if (test_tmpdir) {
    return test_tmpdir;
  }
  // Fall back to /tmp if we don't have a more specific directory to return.
  return "/tmp";
}

absl::Status MakeDirectories(absl::string_view dirname) {
  std::stack<std::string> missing_dirs;

  // Keep walking up the tree until we hit the root.
  while (dirname != "/" && !dirname.empty()) {
    std::string path(dirname);
    if (access(path.c_str(), F_OK) == 0) {
      break;
    } else {
      missing_dirs.push(std::move(path));
      dirname = GetDirname(dirname);
    }
  }

  // Try and create all of the components that are missing.
  while (!missing_dirs.empty()) {
    std::string path = std::move(missing_dirs.top());
    missing_dirs.pop();
    if (mkdir(path.c_str(), 0700) < 0) {
      return absl::UnknownError(absl::StrFormat(
          "unable to create directory %s, mkdir returned errno=%d", path,
          errno));
    }
  }

  // If we get here then every directory was created.
  return absl::OkStatus();
}

DataStoreDirectory::DataStoreDirectory(std::string path)
    : path_(std::move(path)) {}

absl::StatusOr<std::string> DataStoreDirectory::UseFile(
    absl::string_view filename, const UseFileOptions &options) {
  // Make sure the filename is actually a filename, not empty or a directory.
  if (filename.empty()) {
    return absl::InvalidArgumentError("filename must not be empty");
  }
  if (filename.find('/') != filename.npos) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "filename '%s' appears to be a path and not a filename", filename));
  }
  // If the file is already in use then return an error.
  if (used_files_.contains(filename)) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "file '%s' in directory '%s' is already in use", filename, path_));
  }
  // The file is not in use, do any setup required by the options and register
  // it for use.
  used_files_.emplace(filename);
  return JoinFilePaths(path_, filename);
}

absl::StatusOr<DataStoreDirectory::Stats> DataStoreDirectory::GetFileStats(
    absl::string_view filename) const {
  if (!used_files_.contains(filename)) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "'%s' is not a known filename to '%s'", filename, path_));
  }
  return GetSingleFileStats(JoinFilePaths(path_, filename));
}

absl::flat_hash_map<std::string, DataStoreDirectory::Stats>
DataStoreDirectory::GetAllFileStats() const {
  absl::flat_hash_map<std::string, Stats> stats_map;
  for (const std::string &filename : used_files_) {
    stats_map[filename] = GetSingleFileStats(JoinFilePaths(path_, filename));
  }
  return stats_map;
}

}  // namespace ecclesia
