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

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/cleanup/cleanup.h"
#include "absl/types/optional.h"
#include "ecclesia/lib/file/path.h"

namespace ecclesia {
namespace {

using ::testing::Eq;
using ::testing::Optional;

// Simple check if a path exists using access().
bool PathExists(const std::string &path) {
  return access(path.c_str(), F_OK) == 0;
}

// Read in the contents of a path. Returns nullopt if the read fails.
absl::optional<std::string> PathContents(const std::string &path) {
  // Open the file.
  int fd = open(path.c_str(), O_RDONLY);
  if (fd == -1) {
    return absl::nullopt;
  }
  auto fd_closer = absl::MakeCleanup([fd]() { close(fd); });

  // Read from the file in 4k chunks until read returns 0, or fails.
  char buffer[4096];
  std::string contents;
  while (ssize_t rc = read(fd, buffer, sizeof(buffer))) {
    if (rc == -1) return absl::nullopt;
    contents.append(buffer, rc);
  }
  return std::move(contents);
}

// Returns the link pointed to by a given path. Returns nullopt if reading the
// link target fails.
absl::optional<std::string> PathLinkTarget(const std::string &path) {
  // Buffer for holding the link. In theory links could be larger, in practice
  // in all these tests you shouldn't have link paths that take >4KiB.
  char buffer[4096];
  ssize_t rc = readlink(path.c_str(), buffer, sizeof(buffer));
  if (rc == -1) {
    return absl::nullopt;
  }
  return std::string(buffer, rc);
}

TEST(TestFilesystem, RootDoesNotExist) {
  std::string root_path = GetTestTempdirPath("rdne");
  ASSERT_FALSE(PathExists(root_path));

  // Create the filesystem, it should work and create the root directory.
  {
    TestFilesystem fs(root_path);
    EXPECT_TRUE(PathExists(root_path));
  }

  // Now that the filesystem is gone, the root directory should still exist.
  EXPECT_TRUE(PathExists(root_path));
}

TEST(TestFilesystem, RootExists) {
  std::string root_path = GetTestTempdirPath("re");
  ASSERT_FALSE(PathExists(root_path));
  ASSERT_EQ(mkdir(root_path.c_str(), 0755), 0);
  ASSERT_TRUE(PathExists(root_path));

  // Create the filesystem, it should work with the existing directory.
  {
    TestFilesystem fs(root_path);
    EXPECT_TRUE(PathExists(root_path));
  }

  // Now that the filesystem is gone, the root directory should still exist.
  EXPECT_TRUE(PathExists(root_path));
}

TEST(TestFilesystem, CreateDirs) {
  std::string root_path = GetTestTempdirPath("cd");
  TestFilesystem fs(root_path);

  fs.CreateDir("/");  // No-op.

  fs.CreateDir("/d1");
  EXPECT_TRUE(PathExists(JoinFilePaths(root_path, "d1")));

  fs.CreateDir("/d2/sub1/subsub1");
  EXPECT_TRUE(PathExists(JoinFilePaths(root_path, "d2")));
  EXPECT_TRUE(PathExists(JoinFilePaths(root_path, "d2", "sub1")));
  EXPECT_TRUE(PathExists(JoinFilePaths(root_path, "d2", "sub1", "subsub1")));
}

TEST(TestFilesystem, CreateFile) {
  std::string root_path = GetTestTempdirPath("cf");
  TestFilesystem fs(root_path);

  fs.CreateFile("/abcd", "1234567890");
  EXPECT_THAT(PathContents(JoinFilePaths(root_path, "abcd")),
              Optional(Eq("1234567890")));

  fs.CreateDir("/sub");
  fs.CreateFile("/sub/ghi", "hello world");
  EXPECT_THAT(PathContents(JoinFilePaths(root_path, "sub", "ghi")),
              Optional(Eq("hello world")));
}

TEST(TestFilesystem, WriteFileCreatesFiles) {
  std::string root_path = GetTestTempdirPath("wfcf");
  TestFilesystem fs(root_path);

  fs.WriteFile("/abcd", "1234567890");
  EXPECT_THAT(PathContents(JoinFilePaths(root_path, "abcd")),
              Optional(Eq("1234567890")));

  fs.CreateDir("/sub");
  fs.WriteFile("/sub/ghi", "hello world");
  EXPECT_THAT(PathContents(JoinFilePaths(root_path, "sub", "ghi")),
              Optional(Eq("hello world")));
}

TEST(TestFilesystem, CreateFileFailsOnExistingFiles) {
  std::string root_path = GetTestTempdirPath("cffoef");
  TestFilesystem fs(root_path);

  fs.CreateFile("/abcd", "1234567890");
  EXPECT_THAT(PathContents(JoinFilePaths(root_path, "abcd")),
              Optional(Eq("1234567890")));

  EXPECT_DEATH(fs.CreateFile("/abcd", "31415"), "open\\(\\) failed for file");
}

TEST(TestFilesystem, WriteFileWorksOnExistingFiles) {
  std::string root_path = GetTestTempdirPath("wfwoef");
  TestFilesystem fs(root_path);

  fs.CreateFile("/abcd", "1234567890");
  EXPECT_THAT(PathContents(JoinFilePaths(root_path, "abcd")),
              Optional(Eq("1234567890")));

  fs.WriteFile("/abcd", "31415");
  EXPECT_THAT(PathContents(JoinFilePaths(root_path, "abcd")),
              Optional(Eq("31415")));
}

TEST(TestFilesystem, CreateSymlinkAbsolute) {
  std::string root_path = GetTestTempdirPath("csa");
  TestFilesystem fs(root_path);

  fs.CreateFile("/abcd", "1234567890");
  EXPECT_THAT(PathContents(JoinFilePaths(root_path, "abcd")),
              Optional(Eq("1234567890")));

  fs.CreateSymlink("/abcd", "/xyz");
  EXPECT_THAT(PathLinkTarget(JoinFilePaths(root_path, "xyz")),
              Optional(Eq(JoinFilePaths(root_path, "abcd"))));
}

TEST(TestFilesystem, CreateSymlinkRelative) {
  std::string root_path = GetTestTempdirPath("csr");
  TestFilesystem fs(root_path);

  fs.CreateFile("/abcd", "1234567890");
  EXPECT_THAT(PathContents(JoinFilePaths(root_path, "abcd")),
              Optional(Eq("1234567890")));

  fs.CreateSymlink("abcd", "/xyz");
  EXPECT_THAT(PathLinkTarget(JoinFilePaths(root_path, "xyz")),
              Optional(Eq("abcd")));
}

}  // namespace
}  // namespace ecclesia
