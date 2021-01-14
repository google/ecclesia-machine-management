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

// Filesystem utilities for testing. We provide both functions for finding
// data file paths and temporary paths, as well as a utility class for setting
// up simulated filesystem trees.

#ifndef ECCLESIA_LIB_FILE_TEST_FILESYSTEM_H_
#define ECCLESIA_LIB_FILE_TEST_FILESYSTEM_H_

#include <string>

#include "absl/strings/string_view.h"

namespace ecclesia {

// Find the path for a data dependency in a test. The given path should be
// relative to the "ecclesia" root in the source tree. For example if this
// header was used as a data dependency then the path to pass into the function
// would be "lib/file/test_filesystem.h".
std::string GetTestDataDependencyPath(absl::string_view path);

// Find a path in the test temporary directory. Also provided as a 0-arg version
// that simply returns the test temporary directory itself.
std::string GetTestTempdirPath();
std::string GetTestTempdirPath(absl::string_view path);

// Create a temporary directory for unix domain sockets. In theory you could
// just pick a path using GetTestTempdirPath() but this is often a very long
// name that exceeds the AF_UNIX limit. This function will generate a shorter
// path, but one outside of the normal test temporary directory.
std::string GetTestTempUdsDirectory();

// A utility class to make it easy to set up a filesystem layout for testing.
//
// The class is designed to be pointed at a directory which will be acting as
// the root filesystem. When used with googletest you can point it at either
// test_tmpdir or some directory within it. You can then use the utility class
// to create directory, write contents into files, and set up symlinks.
//
// This doesn't attempt to emulate a filesystem or anything like that. It just
// makes it easy to do a bunch of complex setup via a series of simple calls.
//
// This works best for setting up things like faked out /sys or /proc subtrees
// where there are lots of directories and small files and symlinks. If you need
// to use large files or binary files then bundling test data into your test
// may be a better choice.
//
// To keep the API simple and easy to use on failure the operation will log a
// fatal error. In the contexts of tests it is assumed that your operations
// should always succeed and so an error is either something catastrophic (e.g.
// your local test filesystem is broken) or a programming error in your test
// code. In both cases recovery is assumed to not be a priority.
//
// The functions are expected to be given absolute paths, with "/" corresponding
// to the root of the filesystem.
class TestFilesystem {
 public:
  // Create a test filesystem object rooted at the given directory path. This
  // directory entry does not have to exist (if it does not it will be created)
  // but the directory containing it does.
  explicit TestFilesystem(std::string root);

  // You cannot share ownership of this sub-filesystem, so don't allow copying
  // of these objects.
  TestFilesystem(const TestFilesystem &) = delete;
  TestFilesystem &operator=(const TestFilesystem &) = delete;

  // On destruction the entire contents of the filesystem will be removed.
  ~TestFilesystem();

  // Given a test filesystem path, construct the "true path" to the file on the
  // underlying real filesystem. This will fail if you give it a relative path.
  std::string GetTruePath(absl::string_view path) const;

  // Create a directory in the fake filesystem. This will also create any
  // intermediate directories needed, if they do not already exist.
  void CreateDir(absl::string_view path);

  // Populate a file in the fake filesystem and populate it with the given data.
  // The CreateFile version considers it an error for the file to already exist.
  // The WriteFile version will create OR overwrite the file.
  void CreateFile(absl::string_view path, absl::string_view data);
  void WriteFile(absl::string_view path, absl::string_view data);

  // Create a symlink pointing to target at link_path. If the target is an
  // absolute path then it will be transformed into the "real" path using the
  // filesystem root. If the target is a relative path it will be used as-is.
  // The link path must be absolute.
  void CreateSymlink(absl::string_view target, absl::string_view link_path);

  // Delete the entire contents of the test filesystem. This will not remove the
  // root itself.
  void RemoveAllContents();

 private:
  std::string root_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_FILE_TEST_FILESYSTEM_H_
