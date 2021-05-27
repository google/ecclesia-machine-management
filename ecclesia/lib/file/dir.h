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

#ifndef ECCLESIA_LIB_FILE_DIR_H_
#define ECCLESIA_LIB_FILE_DIR_H_

#include <dirent.h>

#include <cstddef>
#include <cstdlib>
#include <string>
#include <tuple>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"

namespace ecclesia {

// Get a system temporary directory path. Note that this doesn't create a
// temporary directory for you, it simply gets the path to a usable system
// temporary directory. This will make use of TEST_TMPDIR if it is set in order
// to be "bazel test"-friendly. It should never fail because as a last resort it
// will just return /tmp.
std::string GetSystemTempdirPath();

// Given a path, create a directory at it, including creating any intervening
// directories above it if they do not already exist.
//
// Returns a non-ok status if it is unable to create the directories for any
// reason. Returns OK if the directory already exists.
absl::Status MakeDirectories(absl::string_view dirname);

// Iterates over a list of all filenames in directory, invoking output_func with
// each filename. The filename will be passed as a string_view whose underlying
// buffer will be released at the end of the WithEachFileInDirectory call.
//
// The paths passed to output_func will be directory entries, not full paths
// (e.g. a file "/tmp/myfile.txt" in dirname "/tmp" will be passed as
// "myfile.txt"). If no files were found or there are any errors, output_func
// will not be called.
template <typename F>
absl::Status WithEachFileInDirectory(absl::string_view dirname, F output_func) {
  // Constants for the names of the pseudo directory entries.
  static constexpr absl::string_view kCurrentDir = ".";
  static constexpr absl::string_view kParentDir = "..";

  class ScandirCloser {
   public:
    ScandirCloser(struct dirent **namelist, int n)
        : namelist_(namelist), n_(n) {}
    ~ScandirCloser() {
      int i = n_;
      while (i--) free(namelist_[i]);
      free(namelist_);
    }

   private:
    struct dirent **namelist_;
    int n_;
  };

  struct dirent **namelist;
  std::string c_dirname(dirname);  // Needed to get a NUL terminator.
  if (int n = scandir(c_dirname.c_str(), &namelist, nullptr, nullptr); n >= 0) {
    ScandirCloser closer(namelist, n);
    while (n--) {
      absl::string_view directory_entry = namelist[n]->d_name;
      // Skip the entries which don't correspond to real entries.
      if (directory_entry == kCurrentDir || directory_entry == kParentDir) {
        continue;
      }
      // Call the provided output function with the name.
      output_func(directory_entry);
    }
    return absl::OkStatus();
  } else {
    return absl::InternalError(
        absl::StrFormat("scandir() failed on directory %s", dirname));
  }
}

// Helper class that tracks the usage of a directory-based data store. This
// mostly just represents a way to pass around a standard directory for storing
// data files, but in a way that also provides some tracking of what files are
// getting created and used.
//
// The use case for this is primarily for in daemons/services where you have
// some local storage available and you want to make that storage available to
// subcomponents that have some fixed set of files they want to use.
//
// Note that "using" a file in the directory is not intended to correspond to
// "creating" a file. When used in daemons this object is often going to be used
// to track files which where already created by prior runs of the daemon, in
// fact that's usually the point of using a filesystem directory at all.
// Declaring that a file is being used is instead just a signal to the object
// that it should provide stats on that filename, and that it should return a
// failure if other code tries to use the same file.
class DataStoreDirectory {
 public:
  // Construct a instance referencing the directory specified by path. The
  // directory is expected to already exist in a useable state, it will not
  // attempt to create it or initialize it in any way.
  explicit DataStoreDirectory(std::string path);

  DataStoreDirectory(const DataStoreDirectory &other) = delete;
  DataStoreDirectory &operator=(const DataStoreDirectory &other) = delete;

  // Allocate a filename for use. By default this does not do anything with the
  // actual file (e.g. create it, or delete it if it already exists). If the
  // file is already in use from a prior call to UseFile, this will fail. On
  // success it returns the full path to the file.
  struct UseFileOptions {};
  absl::StatusOr<std::string> UseFile(absl::string_view filename,
                                      const UseFileOptions &options);

  // Collect usage stats on either a single file, or all files.
  //
  // Note that both of these lookups key off of the in use files, not the actual
  // contents of the directory. You can't use this to look up stats on a file
  // that hasn't be registered with UseFile().
  struct Stats {
    // Does the file exist?
    // If this is false, the other stats will be left at their default values.
    bool exists = false;
    // The size of the file in bytes.
    size_t size = 0;

    // Equality comparisons to make it easy to compare stats.
    bool operator==(const Stats &other) const {
      return std::tie(exists, size) == std::tie(other.exists, other.size);
    }
    bool operator!=(const Stats &other) const { return !(*this == other); }
  };
  absl::StatusOr<Stats> GetFileStats(absl::string_view filename) const;
  absl::flat_hash_map<std::string, Stats> GetAllFileStats() const;

 private:
  // The path to the directory.
  std::string path_;
  // Track the files that are in use. Stores filenames, not full paths.
  absl::flat_hash_set<std::string> used_files_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_FILE_DIR_H_
