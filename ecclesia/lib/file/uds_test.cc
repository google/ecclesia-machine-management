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

#include "ecclesia/lib/file/uds.h"

#include <filesystem>
#include <fstream>
#include <functional>
#include <string>

#include "gtest/gtest.h"
#include "ecclesia/lib/file/test_filesystem.h"

namespace ecclesia {
namespace {

namespace fs = std::filesystem;

TEST(IsSafeTest, VarRunIsSafe) {
  // Absolute path yes, relative path no.
  EXPECT_TRUE(IsSafeUnixDomainSocketRoot("/var/run"));
  EXPECT_FALSE(IsSafeUnixDomainSocketRoot("var/run"));
}

TEST(IsSafeTest, OtherVarsAreNotSafe) {
  // Try /var itself, and some subdirs that people sometimes use.
  EXPECT_FALSE(IsSafeUnixDomainSocketRoot("/var"));
  EXPECT_FALSE(IsSafeUnixDomainSocketRoot("/var/lock"));
  EXPECT_FALSE(IsSafeUnixDomainSocketRoot("/var/tmp"));
}

class SetUpUnixDomainSocketTest : public ::testing::Test {
 public:
  SetUpUnixDomainSocketTest()
      : test_var_run_(GetTestTempdirPath("varrun")),
        socket_dir_(test_var_run_ / "testd"),
        socket_path_(socket_dir_ / "test.socket") {
    fs::create_directory(test_var_run_);
  }

  ~SetUpUnixDomainSocketTest() { fs::remove_all(test_var_run_); }

 protected:
  static void CreateFile(const fs::path &path) {
    std::ofstream touch(path);
    EXPECT_TRUE(fs::exists(path));
  }

  // Makes a "is_root_safe" function that compares against test_var_run_.
  std::function<bool(const std::string &)> MakeIsRootSafe() {
    return [this](const std::string &path) {
      return test_var_run_.string() == path;
    };
  }

  // Three paths that are useful for testing:
  //   - a "/var/run" equivalent for use in testing
  //   - a standard socket directory under it
  //   - a standard socket path in the socket directory
  // The test_var_run_ directory will be created at startup and should be empty.
  // The entire tree will be torn down at shutdown time.
  const fs::path test_var_run_;
  const fs::path socket_dir_;
  const fs::path socket_path_;
};

TEST_F(SetUpUnixDomainSocketTest, FailsOnUnsafeDirectory) {
  // Try to use a file directly in our "/var/run" equivalent. It should fail
  // because the root is then the parent of test_var_run, not test_var_run_.
  EXPECT_FALSE(
      SetUpUnixDomainSocket(test_var_run_ / "test.socket", MakeIsRootSafe()));
}

TEST_F(SetUpUnixDomainSocketTest, PassesWhenDirectoryEmpty) {
  // The directory starts out empty and so this should always work.
  ASSERT_TRUE(SetUpUnixDomainSocket(socket_path_.string(), MakeIsRootSafe()));

  EXPECT_TRUE(fs::exists(socket_dir_));
  EXPECT_EQ(fs::status(socket_dir_).permissions(), fs::perms::owner_all);
  EXPECT_FALSE(fs::exists(socket_path_));
}

TEST_F(SetUpUnixDomainSocketTest, PassesWhenDirectoryExists) {
  fs::create_directory(socket_dir_);
  fs::permissions(socket_dir_, fs::perms::owner_all);

  ASSERT_TRUE(SetUpUnixDomainSocket(socket_path_.string(), MakeIsRootSafe()));

  EXPECT_TRUE(fs::exists(socket_dir_));
  EXPECT_EQ(fs::status(socket_dir_).permissions(), fs::perms::owner_all);
  EXPECT_FALSE(fs::exists(socket_path_));
}

TEST_F(SetUpUnixDomainSocketTest, PassesWhenDirectoryAndFileExists) {
  fs::create_directory(socket_dir_);
  fs::permissions(socket_dir_, fs::perms::owner_all);
  CreateFile(socket_path_);

  ASSERT_TRUE(SetUpUnixDomainSocket(socket_path_.string(), MakeIsRootSafe()));

  EXPECT_TRUE(fs::exists(socket_dir_));
  EXPECT_EQ(fs::status(socket_dir_).permissions(), fs::perms::owner_all);
  EXPECT_FALSE(fs::exists(socket_path_));
}

TEST_F(SetUpUnixDomainSocketTest, FailsWhenDirectoryExistsButIsFile) {
  CreateFile(socket_dir_);

  EXPECT_FALSE(SetUpUnixDomainSocket(socket_path_.string(), MakeIsRootSafe()));
}

TEST_F(SetUpUnixDomainSocketTest, FailsWhenDirectoryExistsWithBadPerms) {
  fs::create_directory(socket_dir_);
  fs::permissions(socket_dir_, fs::perms::owner_read | fs::perms::group_all);

  EXPECT_FALSE(SetUpUnixDomainSocket(socket_path_.string(), MakeIsRootSafe()));
}

TEST_F(SetUpUnixDomainSocketTest, FailsWhenDirectoryCreationFails) {
  // We can make creation of socket_dir_ fail by removing the "/var/run"
  // equivalent it's trying to get created inside of.
  fs::remove(test_var_run_);

  EXPECT_FALSE(SetUpUnixDomainSocket(socket_path_.string(), MakeIsRootSafe()));
}

}  // namespace
}  // namespace ecclesia
