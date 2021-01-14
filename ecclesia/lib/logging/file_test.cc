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

#include "ecclesia/lib/logging/file.h"

#include <unistd.h>

#include <istream>
#include <memory>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/flags/flag.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/logging/interfaces.h"
#include "ecclesia/lib/testing/status.h"
#include "ecclesia/lib/time/clock_fake.h"

namespace ecclesia {
namespace {

using ::testing::Not;
using ::testing::NotNull;

class FileLoggerTest : public ::testing::Test {
 protected:
  FileLoggerTest() : fs_(GetTestTempdirPath()) {}

  // Check if a given path (in the test filesystem) exists.
  bool FileExists(absl::string_view path) {
    return access(fs_.GetTruePath(path).c_str(), F_OK) == 0;
  }

  // Given a path (in the test filesystem) read in its contents and return it.
  std::string ReadFile(absl::string_view path) {
    std::ifstream file(fs_.GetTruePath(path));
    EXPECT_TRUE(file.good()) << "unable to open " << path;
    std::string contents;
    for (std::string line; std::getline(file, line);) {
      contents.append(line);
      contents.push_back('\n');
    }
    return contents;
  }

  TestFilesystem fs_;
};

class FileLoggerDeathTest : public FileLoggerTest {};

TEST_F(FileLoggerTest, WriteEachNonFatalLogLevel) {
  fs_.CreateDir("/logdir1");
  absl::SetFlag(&FLAGS_ecclesia_file_logging_dirs,
                {fs_.GetTruePath("/logdir1")});

  FakeClock clock(absl::UnixEpoch());
  clock.AdvanceTime(absl::Hours(1) + absl::Minutes(23));
  auto maybe_logger = FileLogger::Create("testbin", &clock);

  ASSERT_THAT(maybe_logger, IsOkAndHolds(NotNull()));
  LoggerInterface *logger = maybe_logger->get();

  // Fixed source location to use in tests.
  SourceLocation src_loc = SourceLocation::current();

  // Log lines at every level except 0 (fatal).
  logger->Write(
      {.log_level = 4, .source_location = src_loc, .text = "debug log"});
  clock.AdvanceTime(absl::Minutes(7));
  logger->Write(
      {.log_level = 3, .source_location = src_loc, .text = "info log"});
  clock.AdvanceTime(absl::Minutes(7));
  logger->Write(
      {.log_level = 2, .source_location = src_loc, .text = "warning log"});
  clock.AdvanceTime(absl::Minutes(7));
  logger->Write(
      {.log_level = 1, .source_location = src_loc, .text = "error log"});

  // Expected log lines from above. Different logs will have different mixes of
  // logging lines.
  std::string info_line = absl::StrCat(
      "L3 1970-01-01 01:30:00.000000 file_test.cc:", src_loc.line(),
      "] info log\n");
  std::string warning_line = absl::StrCat(
      "L2 1970-01-01 01:37:00.000000 file_test.cc:", src_loc.line(),
      "] warning log\n");
  std::string error_line = absl::StrCat(
      "L1 1970-01-01 01:44:00.000000 file_test.cc:", src_loc.line(),
      "] error log\n");

  // Check the log files.
  EXPECT_EQ(ReadFile("/logdir1/testbin.log.L0.19700101-012300.000000"), "");
  EXPECT_EQ(ReadFile("/logdir1/testbin.log.L1.19700101-012300.000000"),
            error_line);
  EXPECT_EQ(ReadFile("/logdir1/testbin.log.L2.19700101-012300.000000"),
            warning_line + error_line);
  EXPECT_EQ(ReadFile("/logdir1/testbin.log.L3.19700101-012300.000000"),
            info_line + warning_line + error_line);

  // Check the log symlinks.
  EXPECT_EQ(ReadFile("/logdir1/testbin.log.L0"), "");
  EXPECT_EQ(ReadFile("/logdir1/testbin.log.L1"), error_line);
  EXPECT_EQ(ReadFile("/logdir1/testbin.log.L2"), warning_line + error_line);
  EXPECT_EQ(ReadFile("/logdir1/testbin.log.L3"),
            info_line + warning_line + error_line);
}

TEST_F(FileLoggerDeathTest, WriteFatalLog) {
  fs_.CreateDir("/logdir2");
  absl::SetFlag(&FLAGS_ecclesia_file_logging_dirs,
                {fs_.GetTruePath("/logdir2")});

  FakeClock clock(absl::UnixEpoch());
  clock.AdvanceTime(absl::Hours(2) + absl::Minutes(34));
  auto maybe_logger = FileLogger::Create("testbin", &clock);

  ASSERT_THAT(maybe_logger, IsOkAndHolds(NotNull()));
  LoggerInterface *logger = maybe_logger->get();

  // Fixed source location to use in tests.
  SourceLocation src_loc = SourceLocation::current();

  // Log a fatal log. This should terminate.
  ASSERT_DEATH(
      logger->Write(
          {.log_level = 0, .source_location = src_loc, .text = "fatal log"}),
      "fatal log");

  // Expected log line. Every log should get this line.
  std::string fatal_line = absl::StrCat(
      "L0 1970-01-01 02:34:00.000000 file_test.cc:", src_loc.line(),
      "] fatal log\n");

  // Check the log files.
  EXPECT_EQ(ReadFile("/logdir2/testbin.log.L0.19700101-023400.000000"),
            fatal_line);
  EXPECT_EQ(ReadFile("/logdir2/testbin.log.L1.19700101-023400.000000"),
            fatal_line);
  EXPECT_EQ(ReadFile("/logdir2/testbin.log.L2.19700101-023400.000000"),
            fatal_line);
  EXPECT_EQ(ReadFile("/logdir2/testbin.log.L3.19700101-023400.000000"),
            fatal_line);

  // Check the log symlinks.
  EXPECT_EQ(ReadFile("/logdir2/testbin.log.L0"), fatal_line);
  EXPECT_EQ(ReadFile("/logdir2/testbin.log.L1"), fatal_line);
  EXPECT_EQ(ReadFile("/logdir2/testbin.log.L2"), fatal_line);
  EXPECT_EQ(ReadFile("/logdir2/testbin.log.L3"), fatal_line);
}

TEST_F(FileLoggerTest, LoggerUsesFirstExistingDir) {
  // Create logdir4, but not logdir3.
  fs_.CreateDir("/logdir4");
  absl::SetFlag(&FLAGS_ecclesia_file_logging_dirs,
                {fs_.GetTruePath("/logdir3"), fs_.GetTruePath("/logdir4")});

  FakeClock clock(absl::UnixEpoch());
  clock.AdvanceTime(absl::Hours(3) + absl::Minutes(45));
  auto maybe_logger = FileLogger::Create("testbin", &clock);

  ASSERT_THAT(maybe_logger, IsOkAndHolds(NotNull()));
  LoggerInterface *logger = maybe_logger->get();

  // Fixed source location to use in tests.
  SourceLocation src_loc = SourceLocation::current();

  // Log an error log to check.
  clock.AdvanceTime(absl::Minutes(7));
  logger->Write(
      {.log_level = 1, .source_location = src_loc, .text = "error log"});
  std::string error_line = absl::StrCat(
      "L1 1970-01-01 03:52:00.000000 file_test.cc:", src_loc.line(),
      "] error log\n");

  // Check the log files and symlinks.
  EXPECT_EQ(ReadFile("/logdir4/testbin.log.L1.19700101-034500.000000"),
            error_line);
  EXPECT_EQ(ReadFile("/logdir4/testbin.log.L1"), error_line);

  // Check that the logdir3 equivalents don't exist.
  EXPECT_FALSE(FileExists("/logdir3/testbin.log.L1.19700101-034500.000000"));
  EXPECT_FALSE(FileExists("/logdir3/testbin.log.L1"));
}

TEST_F(FileLoggerTest, LoggerFailsIfNoDirectoryExists) {
  absl::SetFlag(&FLAGS_ecclesia_file_logging_dirs,
                {fs_.GetTruePath("/logdir5")});

  FakeClock clock(absl::UnixEpoch());
  clock.AdvanceTime(absl::Hours(4) + absl::Minutes(56));
  auto maybe_logger = FileLogger::Create("testbin", &clock);

  EXPECT_THAT(maybe_logger, Not(IsOk()));
}

TEST_F(FileLoggerTest, LaterLoggersOverwriteSymlink) {
  fs_.CreateDir("/logdir5");
  absl::SetFlag(&FLAGS_ecclesia_file_logging_dirs,
                {fs_.GetTruePath("/logdir5")});

  // Create a first logger and log something.
  FakeClock clock(absl::UnixEpoch());
  clock.AdvanceTime(absl::Hours(5) + absl::Minutes(7));
  auto maybe_logger1 = FileLogger::Create("testbin", &clock);
  ASSERT_THAT(maybe_logger1, IsOkAndHolds(NotNull()));
  LoggerInterface *logger1 = maybe_logger1->get();

  // Fixed source location to use in tests.
  SourceLocation src_loc = SourceLocation::current();

  // Log an error log to check.
  clock.AdvanceTime(absl::Minutes(7));
  logger1->Write(
      {.log_level = 1, .source_location = src_loc, .text = "first log"});
  std::string error_line1 = absl::StrCat(
      "L1 1970-01-01 05:14:00.000000 file_test.cc:", src_loc.line(),
      "] first log\n");

  // Check the log files and symlinks.
  EXPECT_EQ(ReadFile("/logdir5/testbin.log.L1.19700101-050700.000000"),
            error_line1);
  EXPECT_EQ(ReadFile("/logdir5/testbin.log.L1"), error_line1);

  // Create a second logger and log something else.
  clock.AdvanceTime(absl::Hours(1));
  auto maybe_logger2 = FileLogger::Create("testbin", &clock);
  ASSERT_THAT(maybe_logger2, IsOkAndHolds(NotNull()));
  LoggerInterface *logger2 = maybe_logger2->get();

  // Log to the new logger.
  clock.AdvanceTime(absl::Minutes(7));
  logger2->Write(
      {.log_level = 1, .source_location = src_loc, .text = "second log"});
  std::string error_line2 = absl::StrCat(
      "L1 1970-01-01 06:21:00.000000 file_test.cc:", src_loc.line(),
      "] second log\n");

  // Check the log files and symlinks.
  EXPECT_EQ(ReadFile("/logdir5/testbin.log.L1.19700101-050700.000000"),
            error_line1);
  EXPECT_EQ(ReadFile("/logdir5/testbin.log.L1.19700101-061400.000000"),
            error_line2);
  EXPECT_EQ(ReadFile("/logdir5/testbin.log.L1"), error_line2);
}

}  // namespace
}  // namespace ecclesia
