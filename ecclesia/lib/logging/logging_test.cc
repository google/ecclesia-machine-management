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

#include "ecclesia/lib/logging/logging.h"

#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/interfaces.h"
#include "ecclesia/lib/time/clock_fake.h"

namespace ecclesia {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

// A logging interface implementation that captures all of the logs it is sent
// into vectors of (metadata, text) pairs. The test stores the metadata and log
// text separately so that most tests don't have to check the metadata which is
// much more brittle, because it depends on the time and source location.
class TestRecordingLogger : public LoggerInterface {
 public:
  struct Message {
    std::string prefix;
    std::string text;
  };

  TestRecordingLogger()
      : clock_(absl::UnixEpoch()), messages_by_level_({&fatals_, &errors_}) {}

  // The recorded messages relies on this being a unique object so do not allow
  // these loggers to be copied.
  TestRecordingLogger(const TestRecordingLogger &other) = delete;
  TestRecordingLogger &operator=(const TestRecordingLogger &other) = delete;

  // The fake clock used by the logger. Tests can manipulate this to control the
  // logging times.
  FakeClock &GetClock() { return clock_; }

  // Export all of the logged messages at various levels.
  const std::vector<Message> &GetFatalMessages() const { return fatals_; }
  const std::vector<Message> &GetErrorMessages() const { return errors_; }

 private:
  void Write(WriteParameters params) override {
    if (params.log_level < messages_by_level_.size()) {
      messages_by_level_[params.log_level]->push_back(
          {.prefix = MakeMetadataPrefix(params.log_level, clock_.Now(),
                                        params.source_location),
           .text = std::string(params.text)});
    } else {
      FAIL() << "TestRecordingLogger received unexpected log level "
             << params.log_level << " message: " << params.text;
    }
  }

  FakeClock clock_;

  // The messages vectors, at various levels.
  std::vector<Message> fatals_;
  std::vector<Message> errors_;

  // All the messages vectors, indexed by log level.
  std::vector<std::vector<Message> *> messages_by_level_;
};

// A matcher for matching against TestRecordingLogger::Message values. It can
// match against both the prefix+text (see IsLogMessage) and against just the
// text (see IsLogMessageWithText).
class TestRecordingMessageMatcher
    : public ::testing::MatcherInterface<const TestRecordingLogger::Message &> {
 public:
  explicit TestRecordingMessageMatcher(absl::string_view text) : text_(text) {}
  TestRecordingMessageMatcher(absl::string_view prefix, absl::string_view text)
      : prefix_(prefix), text_(text) {}

  bool MatchAndExplain(const TestRecordingLogger::Message &value,
                       ::testing::MatchResultListener *listener) const final {
    bool matches = true;
    if (prefix_ && value.prefix != *prefix_) {
      *listener << "\nprefix=" << value.prefix << ", expected=" << *prefix_;
      matches = false;
    }
    if (value.text != text_) {
      *listener << "\ntext=" << value.text << ", expected=" << text_;
      matches = false;
    }
    return matches;
  }

  void DescribeTo(::std::ostream *os) const override {
    *os << "has ";
    if (prefix_) {
      *os << "prefix=" << *prefix_ << ", ";
    }
    *os << "text=" << text_;
  }

  void DescribeNegationTo(::std::ostream *os) const override {
    *os << "does not have ";
    if (prefix_) {
      *os << "prefix=" << *prefix_ << ", ";
      *os << "text=" << text_;
    }
  }

 private:
  absl::optional<absl::string_view> prefix_;
  absl::string_view text_;
};
::testing::Matcher<const TestRecordingLogger::Message &> IsLogMessage(
    absl::string_view prefix, absl::string_view text) {
  return ::testing::MakeMatcher(new TestRecordingMessageMatcher(prefix, text));
}
::testing::Matcher<const TestRecordingLogger::Message &> IsLogMessageWithText(
    absl::string_view text) {
  return ::testing::MakeMatcher(new TestRecordingMessageMatcher(text));
}

// Create and register a test logger and then return a pointer to it for tests
// to use in actually verifying the logging behavior.
TestRecordingLogger *RegisterTestLogging() {
  auto owned_logger = absl::make_unique<TestRecordingLogger>();
  auto *logger = owned_logger.get();
  SetGlobalLogger(std::move(owned_logger));
  return logger;
}

TEST(LoggingTest, LogMetadata) {
  TestRecordingLogger *logger = RegisterTestLogging();

  // Note that if you rearrange the lines of code below, you'll have to update
  // the line number computations in the following EXPECT_THAT. This even
  // includes formatting-only changes: the line numbers where the ErrorLog()
  // appears are what matters.
  int base_line = __LINE__;  // Count lines from here to ErrorLog() calls.
  ErrorLog() << "m1";
  logger->GetClock().AdvanceTime(absl::Hours(3) + absl::Minutes(14) +
                                 absl::Seconds(15) +
                                 absl::Microseconds(926536));
  ErrorLog() << "m2";
  logger->GetClock().AdvanceTime(absl::Hours(1) + absl::Microseconds(25));
  ErrorLog() << "m3";
  // End of the code where we're sensitive to the line numbers. Compute the
  // line numbers of the three log lines.
  int m1_line = base_line + 1;
  int m2_line = base_line + 5;
  int m3_line = base_line + 7;

  // Expected metadata strings.
  std::string m1_meta = absl::StrCat(
      "L1 1970-01-01 00:00:00.000000 logging_test.cc:", m1_line, "] ");
  std::string m2_meta = absl::StrCat(
      "L1 1970-01-01 03:14:15.926536 logging_test.cc:", m2_line, "] ");
  std::string m3_meta = absl::StrCat(
      "L1 1970-01-01 04:14:15.926561 logging_test.cc:", m3_line, "] ");
  // Run the actual validation.
  EXPECT_THAT(
      logger->GetErrorMessages(),
      ElementsAre(IsLogMessage(m1_meta, "m1"), IsLogMessage(m2_meta, "m2"),
                  IsLogMessage(m3_meta, "m3")));
}

TEST(LoggingDeathTest, LogFatal) {
  EXPECT_DEATH(FatalLog() << "terminate program", "terminate program");
}

TEST(LoggingTest, LogErrors) {
  TestRecordingLogger *logger = RegisterTestLogging();

  ErrorLog() << "first message";
  ErrorLog() << "second message = " << 2;
  ErrorLog() << "third"
             << " "
             << "message";

  EXPECT_THAT(logger->GetFatalMessages(), IsEmpty());
  EXPECT_THAT(logger->GetErrorMessages(),
              ElementsAre(IsLogMessageWithText("first message"),
                          IsLogMessageWithText("second message = 2"),
                          IsLogMessageWithText("third message")));
}

TEST(LoggingDeathTest, CheckTerminates) {
  Check(true, "true is true");
  EXPECT_DEATH(Check(false, "false is false"),
               "Check failed \\(false is false\\)");
}

TEST(LoggingDeathTest, CheckConditionTerminates) {
  CheckCondition(1 == 1);
  EXPECT_DEATH(CheckCondition(0 == 1), "Check failed \\(0 == 1\\)");
}

TEST(LoggingTest, DieIfNullDoesNotDieOnNotNull) {
  std::unique_ptr<int> heap_int = DieIfNull(absl::make_unique<int>(278));
  int *raw_heap_int = DieIfNull(heap_int.get());
  EXPECT_EQ(heap_int.get(), raw_heap_int);
}

TEST(LoggingDeathTest, DieIfNullDiesOnNull) {
  auto make_nullptr = []() -> int * { return nullptr; };
  EXPECT_DEATH(DieIfNull(make_nullptr()), "pointer is null");
}

}  // namespace
}  // namespace ecclesia
