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

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/testing/status.h"

namespace ecclesia {
namespace {

namespace fs = std::filesystem;

using ::testing::Not;
using ::testing::UnorderedElementsAre;

// Sets up a temporary directory in the test directory in order to ensure no
// filesystem state is passed on from test to test.
class DirTest : public ::testing::Test {
 public:
  DirTest() : testdir_(GetTestTempdirPath("testdir")) {
    fs::create_directory(fs::path(testdir_));
  }

  std::vector<std::string> WithEachFileInDirectoryVector(
      absl::string_view dirname) {
    std::vector<std::string> files;
    absl::Status status = WithEachFileInDirectory(
        dirname,
        [&files](absl::string_view file) { files.emplace_back(file); });
    EXPECT_THAT(status, IsOk());
    return files;
  }

  ~DirTest() { fs::remove_all(fs::path(testdir_)); }

 protected:
  absl::string_view TestDirName() { return testdir_; }

 private:
  std::string testdir_;
};

TEST_F(DirTest, CreateDirectoriesNothingExists) {
  std::string created_dir = absl::StrCat(TestDirName(), "/aaa/bbb/ccc");
  ASSERT_FALSE(fs::exists(created_dir));
  EXPECT_TRUE(MakeDirectories(created_dir).ok());
  EXPECT_TRUE(fs::exists(created_dir));
}

TEST_F(DirTest, CreateDirectoriesEverythingButLeafExists) {
  fs::create_directory(absl::StrCat(TestDirName(), "/aaa"));
  fs::create_directory(absl::StrCat(TestDirName(), "/aaa/bbb"));
  std::string created_dir = absl::StrCat(TestDirName(), "/aaa/bbb/ccc");
  ASSERT_FALSE(fs::exists(created_dir));
  EXPECT_TRUE(MakeDirectories(created_dir).ok());
  EXPECT_TRUE(fs::exists(created_dir));
}

TEST_F(DirTest, CreateDirectoriesEverythingExists) {
  fs::create_directory(absl::StrCat(TestDirName(), "/aaa"));
  fs::create_directory(absl::StrCat(TestDirName(), "/aaa/bbb"));
  fs::create_directory(absl::StrCat(TestDirName(), "/aaa/bbb/ccc"));
  std::string created_dir = absl::StrCat(TestDirName(), "/aaa/bbb/ccc");
  ASSERT_TRUE(fs::exists(created_dir));
  EXPECT_TRUE(MakeDirectories(created_dir).ok());
  EXPECT_TRUE(fs::exists(created_dir));
}

TEST_F(DirTest, CreateDirectoriesFailsWithFileInTheWay) {
  fs::create_directory(absl::StrCat(TestDirName(), "/aaa"));
  std::ofstream touch(absl::StrCat(TestDirName(), "/aaa/bbb"));
  std::string created_dir = absl::StrCat(TestDirName(), "/aaa/bbb/ccc");
  ASSERT_FALSE(fs::exists(created_dir));
  EXPECT_FALSE(MakeDirectories(created_dir).ok());
  EXPECT_FALSE(fs::exists(created_dir));
}

TEST_F(DirTest, WithEachFileEmptyDirectory) {
  EXPECT_THAT(WithEachFileInDirectoryVector(TestDirName()),
              UnorderedElementsAre());
}

TEST_F(DirTest, WithEachFileDirDoesntExist) {
  std::string bad_dir = absl::StrCat(TestDirName(), "/baddir");

  EXPECT_THAT(WithEachFileInDirectory(bad_dir, [](absl::string_view) {}),
              Not(IsOk()));
}

TEST_F(DirTest, WithEachFileDirIsAFile) {
  fs::path filepath = fs::path(TestDirName()) / "file1";
  std::ofstream touch(filepath);
  EXPECT_TRUE(fs::exists(filepath));

  EXPECT_THAT(
      WithEachFileInDirectory(filepath.c_str(), [](absl::string_view) {}),
      Not(IsOk()));
}

TEST_F(DirTest, WithEachFileOneFile) {
  fs::path filepath = fs::path(TestDirName()) / "file1";
  std::ofstream touch(filepath);
  ASSERT_TRUE(fs::exists(filepath));

  EXPECT_THAT(WithEachFileInDirectoryVector(TestDirName()),
              UnorderedElementsAre("file1"));
}

TEST_F(DirTest, WithEachFileTwoFiles) {
  fs::path f1 = fs::path(TestDirName()) / "file1";
  std::ofstream touch_f1(f1);
  EXPECT_TRUE(fs::exists(f1));

  fs::path f2 = fs::path(TestDirName()) / "file2";
  std::ofstream touch_f2(f2);
  EXPECT_TRUE(fs::exists(f2));

  EXPECT_THAT(WithEachFileInDirectoryVector(TestDirName()),
              UnorderedElementsAre("file1", "file2"));
}

TEST_F(DirTest, WithEachFileSubdirectoriesListed) {
  fs::path subdir = fs::path(TestDirName()) / "subdir";
  fs::create_directories(subdir);
  EXPECT_TRUE(fs::exists(subdir));

  EXPECT_THAT(WithEachFileInDirectoryVector(TestDirName()),
              UnorderedElementsAre("subdir"));
}

}  // namespace
}  // namespace ecclesia
