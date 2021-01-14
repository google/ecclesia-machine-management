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

#include <sys/stat.h>

#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "ecclesia/lib/file/path.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/testing/status.h"

namespace ecclesia {
namespace {

using ::testing::Not;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

class ApifsTest : public ::testing::Test {
 protected:
  ApifsTest() : fs_(GetTestTempdirPath()), apifs_(GetTestTempdirPath("sys")) {
    // Create a couple of directories.
    fs_.CreateDir("/sys/ab/cd/ef");
    fs_.CreateDir("/sys/ab/gh");

    // Drop some files into one of the directories.
    fs_.CreateFile("/sys/ab/file1", "contents**\n");
    fs_.CreateFile("/sys/ab/file2", "x\ny\nz\n");
    fs_.CreateFile("/sys/ab/file3", "");

    // Create a file with lots of contents, to verify reading from a file that
    // requires multiple read() calls.
    std::string large_contents(10000, 'J');
    fs_.WriteFile("/sys/ab/largefile", large_contents);

    // Create a symlink to one of the files.
    fs_.CreateSymlink("file1", "/sys/ab/file4");
  }

  TestFilesystem fs_;
  ApifsDirectory apifs_;
};

TEST_F(ApifsTest, TestExists) {
  EXPECT_TRUE(apifs_.Exists());
  EXPECT_FALSE(apifs_.Exists("a"));
  EXPECT_TRUE(apifs_.Exists("ab"));
  EXPECT_TRUE(apifs_.Exists("ab/cd"));
  EXPECT_TRUE(apifs_.Exists("ab/cd/ef"));
  EXPECT_FALSE(apifs_.Exists("ab/cd/ef/gh"));
  EXPECT_TRUE(apifs_.Exists("ab/gh"));
  EXPECT_TRUE(apifs_.Exists("ab/file1"));
  EXPECT_TRUE(apifs_.Exists("ab/file2"));
  EXPECT_TRUE(apifs_.Exists("ab/file3"));
  EXPECT_TRUE(apifs_.Exists("ab/file4"));
  EXPECT_FALSE(apifs_.Exists("ab/file5"));

  ApifsFile f4(apifs_, "ab/file4");
  EXPECT_TRUE(f4.Exists());
  ApifsFile f5(apifs_, "ab/file5");
  EXPECT_FALSE(f5.Exists());
}

TEST_F(ApifsTest, TestStat) {
  auto stat_empty = apifs_.Stat("");
  EXPECT_TRUE(stat_empty.ok());
  EXPECT_TRUE(S_ISDIR(stat_empty->st_mode));

  auto stat_a = apifs_.Stat("a");
  EXPECT_FALSE(stat_a.ok());

  auto stat_ab = apifs_.Stat("ab");
  EXPECT_TRUE(stat_ab.ok());
  EXPECT_TRUE(S_ISDIR(stat_ab->st_mode));

  auto stat_ab_file1 = apifs_.Stat("ab/file1");
  EXPECT_TRUE(stat_ab_file1.ok());
  EXPECT_TRUE(S_ISREG(stat_ab_file1->st_mode));

  ApifsFile f1(apifs_, "ab/file1");
  stat_ab_file1 = f1.Stat();
  EXPECT_TRUE(stat_ab_file1.ok());
  EXPECT_TRUE(S_ISREG(stat_ab_file1->st_mode));
}

TEST_F(ApifsTest, TestListEntries) {
  absl::StatusOr<std::vector<std::string>> maybe_entries;

  // Look at the root contents.
  maybe_entries = apifs_.ListEntryNames();
  ASSERT_TRUE(maybe_entries.ok());
  EXPECT_THAT(maybe_entries.value(), UnorderedElementsAre("ab"));
  maybe_entries = apifs_.ListEntryPaths();
  ASSERT_TRUE(maybe_entries.ok());
  EXPECT_THAT(maybe_entries.value(), UnorderedElementsAre(JoinFilePaths(
                                         GetTestTempdirPath("sys"), "ab")));

  // Look at the contents of "ab".
  std::vector<std::string> entries_names = {
      "cd", "file1", "file2", "file3", "file4", "gh", "largefile"};
  maybe_entries = apifs_.ListEntryNames("ab");
  ASSERT_TRUE(maybe_entries.ok());
  EXPECT_THAT(*maybe_entries, UnorderedElementsAreArray(entries_names));
  std::vector<std::string> entries_fullpaths;
  for (const std::string& entry : entries_names) {
    entries_fullpaths.push_back(
        JoinFilePaths(GetTestTempdirPath("sys"), "ab", entry));
  }
  maybe_entries = apifs_.ListEntryPaths("ab");
  ASSERT_TRUE(maybe_entries.ok());
  EXPECT_THAT(maybe_entries.value(),
              UnorderedElementsAreArray(entries_fullpaths));

  // Look at the contents of "ab/cd".
  maybe_entries = apifs_.ListEntryNames("ab/cd");
  ASSERT_TRUE(maybe_entries.ok());
  EXPECT_THAT(maybe_entries.value(), UnorderedElementsAre("ef"));
  maybe_entries = apifs_.ListEntryPaths("ab/cd");
  ASSERT_TRUE(maybe_entries.ok());
  EXPECT_THAT(maybe_entries.value(),
              UnorderedElementsAre(
                  JoinFilePaths(GetTestTempdirPath("sys"), "ab/cd", "ef")));

  // Look at the contents of "ab/cd/ef".
  maybe_entries = apifs_.ListEntryNames("ab/cd/ef");
  ASSERT_TRUE(maybe_entries.ok());
  EXPECT_THAT(maybe_entries.value(), UnorderedElementsAre());
  maybe_entries = apifs_.ListEntryPaths("ab/cd/ef");
  ASSERT_TRUE(maybe_entries.ok());
  EXPECT_THAT(maybe_entries.value(), UnorderedElementsAre());

  // Look at a non-existant "ab/cd/ef/gh".
  EXPECT_EQ(apifs_.ListEntryNames("ab/cd/ef/gh").status().code(),
            absl::StatusCode::kNotFound);
  EXPECT_EQ(apifs_.ListEntryPaths("ab/cd/ef/gh").status().code(),
            absl::StatusCode::kNotFound);
}

TEST_F(ApifsTest, TestRead) {
  // Look at the contents of each file in ab.
  EXPECT_THAT(apifs_.Read("ab/file1"), IsOkAndHolds("contents**\n"));
  EXPECT_THAT(apifs_.Read("ab/file2"), IsOkAndHolds("x\ny\nz\n"));
  EXPECT_THAT(apifs_.Read("ab/file3"), IsOkAndHolds(""));
  EXPECT_THAT(apifs_.Read("ab/file4"), IsOkAndHolds("contents**\n"));
  EXPECT_THAT(apifs_.Read("ab/largefile"),
              IsOkAndHolds(std::string(10000, 'J')));

  // We should not be able to read directories or non-existant files.
  EXPECT_THAT(apifs_.Read("ab"), Not(IsOk()));
  EXPECT_THAT(apifs_.Read("ab/file5"), Not(IsOk()));

  // Use an ApifsFile to do the read.
  ApifsFile f2(apifs_, "ab/file2");
  EXPECT_THAT(f2.Read(), IsOkAndHolds("x\ny\nz\n"));
}

TEST_F(ApifsTest, TestWrite) {
  // Verify that we can write a file and read it back. Depends on Read also
  // working correctly.
  EXPECT_THAT(apifs_.Read("ab/file3"), IsOkAndHolds(""));
  EXPECT_THAT(apifs_.Write("ab/file3", "hello, world!\n"), IsOk());
  EXPECT_THAT(apifs_.Read("ab/file3"), IsOkAndHolds("hello, world!\n"));

  // We should not be able to write to directories or non-existant files.
  EXPECT_THAT(apifs_.Write("ab", "testing"), Not(IsOk()));
  EXPECT_THAT(apifs_.Write("ab/file5", "testing"), Not(IsOk()));

  // Use an ApifsFile to do the same operations.
  ApifsFile f3(apifs_, "ab/file3");
  EXPECT_THAT(f3.Read(), IsOkAndHolds("hello, world!\n"));
  EXPECT_THAT(f3.Write("goodbye, file!\n"), IsOk());
  EXPECT_THAT(f3.Read(), IsOkAndHolds("goodbye, file!\n"));
}

TEST_F(ApifsTest, TestReadLink) {
  // Test reading the symlink from file4 -> file1.
  EXPECT_THAT(apifs_.ReadLink("ab/file4"), IsOkAndHolds("file1"));

  // Reading a non-symlink should fail, or a non-existant file.
  EXPECT_THAT(apifs_.ReadLink("ab/file1"), Not(IsOk()));
  EXPECT_THAT(apifs_.ReadLink("ab/file5"), Not(IsOk()));

  // Use an ApifsFile to try the read.
  ApifsFile f4(apifs_, "ab/file4");
  EXPECT_THAT(f4.ReadLink(), IsOkAndHolds("file1"));
}

class MsrTest : public ::testing::Test {
 protected:
  MsrTest()
      : fs_(GetTestTempdirPath()), msr_(GetTestTempdirPath("dev/cpu/0/msr")) {
    fs_.CreateDir("/dev/cpu/0");

    // content: 0a 0a 0a 0a 0a 0a 0a 0a 0a 0a
    fs_.CreateFile("/dev/cpu/0/msr", "\n\n\n\n\n\n\n\n\n\n");
  }

  TestFilesystem fs_;
  ApifsFile msr_;
};

TEST_F(MsrTest, TestSeekAndRead) {
  std::vector<char> out_msr_data(8);
  std::vector<char> expected(8, 0xa);

  EXPECT_TRUE(msr_.SeekAndRead(2, absl::MakeSpan(out_msr_data)).ok());
  EXPECT_EQ(expected, out_msr_data);
}

TEST_F(MsrTest, TestSeekAndWrite) {
  std::vector<char> out_msr_data(8);
  std::vector<char> expected(8, 0xa);

  EXPECT_TRUE(msr_.SeekAndRead(2, absl::MakeSpan(out_msr_data)).ok());
  EXPECT_EQ(expected, out_msr_data);

  std::vector<char> expected_msr_data{0xD, 0xE, 0xA, 0xD, 0xB, 0xE, 0xE, 0xF};
  EXPECT_TRUE(
      msr_.SeekAndWrite(1, absl::MakeConstSpan(expected_msr_data)).ok());
  EXPECT_TRUE(msr_.SeekAndRead(1, absl::MakeSpan(out_msr_data)).ok());
  EXPECT_EQ(expected_msr_data, out_msr_data);
}

TEST(MsrTestConstruct, TestMsrNotExist) {
  ApifsFile msr_default;
  std::vector<char> out_msr_data(8);
  EXPECT_FALSE(msr_default.SeekAndRead(2, absl::MakeSpan(out_msr_data)).ok());

  ApifsFile msr_non_exist(GetTestTempdirPath("dev/cpu/400/msr"));
  EXPECT_FALSE(msr_non_exist.SeekAndRead(2, absl::MakeSpan(out_msr_data)).ok());

  std::vector<char> msr_data{0xD, 0xE, 0xA, 0xD, 0xB, 0xE, 0xE, 0xF};
  EXPECT_FALSE(
      msr_non_exist.SeekAndWrite(1, absl::MakeConstSpan(msr_data)).ok());
}

TEST_F(MsrTest, TestSeekFail) {
  std::vector<char> out_msr_data(8);
  EXPECT_FALSE(msr_.SeekAndRead(20, absl::MakeSpan(out_msr_data)).ok());
}

TEST_F(MsrTest, TestSeekAndReadFail) {
  std::vector<char> out_msr_data(8);
  EXPECT_FALSE(msr_.SeekAndRead(5, absl::MakeSpan(out_msr_data)).ok());
}

}  // namespace
}  // namespace ecclesia
