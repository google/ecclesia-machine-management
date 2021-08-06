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

#include "ecclesia/magent/lib/event_logger/system_event_visitors.h"

#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "ecclesia/lib/mcedecoder/mce_decode_mock.h"
#include "ecclesia/lib/mcedecoder/mce_messages.h"
#include "ecclesia/lib/time/clock.h"
#include "ecclesia/magent/lib/event_logger/event_logger.h"
#include "ecclesia/magent/lib/event_reader/event_reader.h"

namespace ecclesia {
namespace {

using ::testing::_;
using ::testing::Return;

class MockEventReader : public SystemEventReader {
 public:
  MOCK_METHOD(absl::optional<SystemEventRecord>, ReadEvent, ());
};

class MockClock : public Clock {
 public:
  MOCK_METHOD(absl::Time, Now, (), (const, override));
  MOCK_METHOD(void, Sleep, (absl::Duration), (override));
};

class SystemEventVisitorTest : public ::testing::Test {
 protected:
  SystemEventVisitorTest() { reader_ = new MockEventReader(); }

  MockEventReader *reader_;
  absl::Notification last_event_logged_;
};

TEST_F(SystemEventVisitorTest, MemoryErrorCounts) {
  // return an empty machine check, since the MceDecoder's response is mocked
  // out
  EXPECT_CALL(*reader_, ReadEvent)
      .WillOnce(Return(SystemEventRecord{.record = MachineCheck{}}))
      .WillOnce(Return(SystemEventRecord{.record = MachineCheck{}}))
      .WillOnce(Return(SystemEventRecord{.record = MachineCheck{}}))
      .WillOnce(Return(SystemEventRecord{.record = MachineCheck{}}))
      .WillOnce(Return(SystemEventRecord{.record = MachineCheck{}}))
      .WillOnce([&]() {
        last_event_logged_.Notify();
        return absl::nullopt;
      })
      .WillRepeatedly(Return(absl::nullopt));

  std::unique_ptr<MockClock> clock = absl::make_unique<MockClock>();

  EXPECT_CALL(*clock, Now)
      .WillOnce(Return(absl::UnixEpoch() + absl::Seconds(1)))
      .WillOnce(Return(absl::UnixEpoch() + absl::Seconds(2)))
      .WillOnce(Return(absl::UnixEpoch() + absl::Seconds(3)))
      .WillOnce(Return(absl::UnixEpoch() + absl::Seconds(4)))
      .WillOnce(Return(absl::UnixEpoch() + absl::Seconds(5)));

  std::vector<std::unique_ptr<SystemEventReader>> readers;
  readers.push_back(absl::WrapUnique<SystemEventReader>(reader_));
  SystemEventLogger logger(std::move(readers), clock.get());
  // Wait for the last event to be logged before visiting the records
  last_event_logged_.WaitForNotification();

  // Mock the Mce decoding by just incrementing the dimm number and invetring
  // the correctible bit on successive mces.
  auto decode_message =
      [](testing::Unused) -> absl::StatusOr<MceDecodedMessage> {
    static int gldn = 5;
    static int correctable = false;
    MceDecodedMessage output;
    output.mem_errors.push_back(MemoryError{});
    output.mem_errors[0].mem_error_bucket.gldn = gldn++;
    output.mem_errors[0].mem_error_bucket.correctable = correctable;
    output.mem_errors[0].error_count = 1;
    correctable = !correctable;
    return output;
  };

  {
    // Visit all of the events.
    auto mce_decoder = absl::make_unique<MockMceDecoder>();

    EXPECT_CALL(*mce_decoder, DecodeMceMessage(_))
        .WillRepeatedly(testing::Invoke(decode_message));

    auto dimm_visitor = DimmErrorCountingVisitor(
        absl::UnixEpoch(),
        absl::make_unique<MceDecoderAdapter>(std::move(mce_decoder)));

    logger.Visit(&dimm_visitor);

    auto dimm_error_counts = dimm_visitor.GetDimmErrorCounts();

    absl::flat_hash_map<int, DimmErrorCount> expected_counts{
        {5, {0, 1}}, {6, {1, 0}}, {7, {0, 1}}, {8, {1, 0}}, {9, {0, 1}},
    };

    EXPECT_THAT(dimm_error_counts, testing::ContainerEq(expected_counts));
  }
}

TEST_F(SystemEventVisitorTest, CpuErrorCounts) {
  // return an empty machine check, since the MceDecoder's response is mocked
  // out
  EXPECT_CALL(*reader_, ReadEvent)
      .WillOnce(Return(SystemEventRecord{.record = MachineCheck{}}))
      .WillOnce(Return(SystemEventRecord{.record = MachineCheck{}}))
      .WillOnce(Return(SystemEventRecord{.record = MachineCheck{}}))
      .WillOnce(Return(SystemEventRecord{.record = MachineCheck{}}))
      .WillOnce(Return(SystemEventRecord{.record = MachineCheck{}}))
      .WillOnce([&]() {
        last_event_logged_.Notify();
        return absl::nullopt;
      })
      .WillRepeatedly(Return(absl::nullopt));

  std::unique_ptr<MockClock> clock = absl::make_unique<MockClock>();

  EXPECT_CALL(*clock, Now)
      .WillOnce(Return(absl::UnixEpoch() + absl::Seconds(1)))
      .WillOnce(Return(absl::UnixEpoch() + absl::Seconds(2)))
      .WillOnce(Return(absl::UnixEpoch() + absl::Seconds(3)))
      .WillOnce(Return(absl::UnixEpoch() + absl::Seconds(4)))
      .WillOnce(Return(absl::UnixEpoch() + absl::Seconds(5)));

  std::vector<std::unique_ptr<SystemEventReader>> readers;
  readers.push_back(absl::WrapUnique<SystemEventReader>(reader_));
  SystemEventLogger logger(std::move(readers), clock.get());
  // Wait for the last event to be logged before visiting the records
  last_event_logged_.WaitForNotification();

  // Mock the Mce decoding by just incrementing the dimm number and invetring
  // the correctible bit on successive mces.
  auto decode_message =
      [](testing::Unused) -> absl::StatusOr<MceDecodedMessage> {
    static int correctable = false;
    static int socket = 0;
    MceDecodedMessage output;
    output.cpu_errors.push_back(CpuError{});
    output.cpu_errors[0].cpu_error_bucket.socket = socket++;
    output.cpu_errors[0].cpu_error_bucket.correctable = correctable;
    output.cpu_errors[0].error_count = 1;
    correctable = !correctable;
    return output;
  };

  {
    // Visit all of the events
    auto mce_decoder = absl::make_unique<MockMceDecoder>();

    EXPECT_CALL(*mce_decoder, DecodeMceMessage(_))
        .WillRepeatedly(testing::Invoke(decode_message));

    auto cpu_visitor = CpuErrorCountingVisitor(
        absl::UnixEpoch(),
        absl::make_unique<MceDecoderAdapter>(std::move(mce_decoder)));

    logger.Visit(&cpu_visitor);

    auto cpu_error_counts = cpu_visitor.GetCpuErrorCounts();

    absl::flat_hash_map<int, CpuErrorCount> expected_counts{
        {0, {0, 1}}, {1, {1, 0}}, {2, {0, 1}}, {3, {1, 0}}, {4, {0, 1}},
    };

    EXPECT_THAT(cpu_error_counts, testing::ContainerEq(expected_counts));
  }
}

TEST_F(SystemEventVisitorTest, CpuErrorExcludeWhitelisted) {
  // return an empty machine check, since the MceDecoder's response is mocked
  // out
  EXPECT_CALL(*reader_, ReadEvent)
      .WillOnce(Return(SystemEventRecord{.record = MachineCheck{}}))
      .WillOnce(Return(SystemEventRecord{.record = MachineCheck{}}))
      .WillOnce(Return(SystemEventRecord{.record = MachineCheck{}}))
      .WillOnce(Return(SystemEventRecord{.record = MachineCheck{}}))
      .WillOnce(Return(SystemEventRecord{.record = MachineCheck{}}))
      .WillOnce([&]() {
        last_event_logged_.Notify();
        return absl::nullopt;
      })
      .WillRepeatedly(Return(absl::nullopt));

  std::unique_ptr<MockClock> clock = absl::make_unique<MockClock>();

  EXPECT_CALL(*clock, Now)
      .WillOnce(Return(absl::UnixEpoch() + absl::Seconds(1)))
      .WillOnce(Return(absl::UnixEpoch() + absl::Seconds(2)))
      .WillOnce(Return(absl::UnixEpoch() + absl::Seconds(3)))
      .WillOnce(Return(absl::UnixEpoch() + absl::Seconds(4)))
      .WillOnce(Return(absl::UnixEpoch() + absl::Seconds(5)));

  std::vector<std::unique_ptr<SystemEventReader>> readers;
  readers.push_back(absl::WrapUnique<SystemEventReader>(reader_));
  SystemEventLogger logger(std::move(readers), clock.get());
  // Wait for the last event to be logged before visiting the records
  last_event_logged_.WaitForNotification();

  // Mock the Mce decoding by just incrementing the dimm number and invetring
  // the correctible bit on successive mces.
  auto decode_message =
      [](testing::Unused) -> absl::StatusOr<MceDecodedMessage> {
    static bool correctable = false;
    static int socket = 0;
    MceDecodedMessage output;
    output.cpu_errors.push_back(CpuError{});
    // To emulate that all CPU errors with socket <= 2 are whitelisted.
    if (socket <= 2) {
      output.cpu_errors[0].cpu_error_bucket.whitelisted = true;
    } else {
      output.cpu_errors[0].cpu_error_bucket.whitelisted = false;
    }
    output.cpu_errors[0].cpu_error_bucket.socket = socket++;
    output.cpu_errors[0].cpu_error_bucket.correctable = correctable;
    output.cpu_errors[0].error_count = 1;
    correctable = !correctable;
    return output;
  };

  {
    // Visit all of the events
    auto mce_decoder = absl::make_unique<MockMceDecoder>();

    EXPECT_CALL(*mce_decoder, DecodeMceMessage(_))
        .WillRepeatedly(testing::Invoke(decode_message));

    auto cpu_visitor = CpuErrorCountingVisitor(
        absl::UnixEpoch(),
        absl::make_unique<MceDecoderAdapter>(std::move(mce_decoder)));

    logger.Visit(&cpu_visitor);

    auto cpu_error_counts = cpu_visitor.GetCpuErrorCounts();

    // The cpu errors on socket 0, 1, 2 are all whitelisted. Thus, socket 0, 1,
    // 2 has 0 CE & UE counters.
    absl::flat_hash_map<int, CpuErrorCount> expected_counts{
        {3, {1, 0}},
        {4, {0, 1}},
    };

    EXPECT_THAT(cpu_error_counts, testing::ContainerEq(expected_counts));
  }
}

}  // namespace

}  // namespace ecclesia
