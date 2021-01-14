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

#include "ecclesia/magent/lib/event_logger/event_logger.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "ecclesia/lib/time/clock.h"
#include "ecclesia/magent/lib/event_reader/elog.emb.h"
#include "ecclesia/magent/lib/event_reader/event_reader.h"
#include "runtime/cpp/emboss_cpp_util.h"
#include "runtime/cpp/emboss_prelude.h"

namespace ecclesia {
namespace {

using ::testing::ContainerEq;
using ::testing::Return;

class MockEventReader : public SystemEventReader {
 public:
  MOCK_METHOD(absl::optional<SystemEventRecord>, ReadEvent, ());
};

class EventLoggerTest : public ::testing::Test {
 protected:
  EventLoggerTest() {
    reader_ = new MockEventReader();
    auto elog_f = [](EventType type) {
      static const size_t record_size = ElogRecord::MaxSizeInBytes();
      std::vector<uint8_t> record_data = std::vector<uint8_t>(record_size, 0);
      auto record_view = MakeElogRecordView(record_data.data(), record_size);
      record_view.size().Write(record_size);
      record_view.id().Write(type);
      SystemEventRecord record = {absl::Now(), Elog(record_view)};
      return record;
    };

    // This is the sequence in which event records will be logged
    EXPECT_CALL(*reader_, ReadEvent)
        .WillOnce(Return(elog_f(EventType::LOG_AREA_RESET)))
        .WillOnce(Return(elog_f(EventType::SINGLE_BIT_ECC_ERROR)))
        .WillOnce(Return(elog_f(EventType::MULTI_BIT_ECC_ERROR)))
        .WillOnce(Return(elog_f(EventType::MEMORY_PARITY_ERROR)))
        .WillOnce(Return(elog_f(EventType::CPU_FAILURE)))
        .WillOnce(Return(elog_f(EventType::UNCORRECTABLE_CPU_COMPLEX_ERROR)))
        .WillOnce(Return(elog_f(EventType::ERROR_DIMMS)))
        .WillOnce(Return(elog_f(EventType::END_OF_LOG)))
        .WillOnce([&]() {
          last_event_logged_.Notify();
          return absl::nullopt;
        })
        .WillRepeatedly(Return(absl::nullopt));
  }

  MockEventReader *reader_;
  absl::Notification last_event_logged_;
};

class TypeExtractingVisitor : public SystemEventVisitor {
 public:
  TypeExtractingVisitor(VisitDirection direction)
      : SystemEventVisitor(direction) {}

  bool Visit(const SystemEventRecord &record) {
    const auto &elog_record = absl::get<Elog>(record.record);
    record_types_.push_back(elog_record.GetElogRecordView().id().Read());
    return true;
  }

  std::vector<EventType> GetRecordTypes() { return record_types_; }

 private:
  std::vector<EventType> record_types_;
};

// Test the visit direction  FROM_START
TEST_F(EventLoggerTest, VisitFromStart) {
  std::vector<EventType> expected_types = {
      EventType::LOG_AREA_RESET,
      EventType::SINGLE_BIT_ECC_ERROR,
      EventType::MULTI_BIT_ECC_ERROR,
      EventType::MEMORY_PARITY_ERROR,
      EventType::CPU_FAILURE,
      EventType::UNCORRECTABLE_CPU_COMPLEX_ERROR,
      EventType::ERROR_DIMMS,
      EventType::END_OF_LOG};

  std::vector<std::unique_ptr<SystemEventReader>> readers;
  readers.push_back(absl::WrapUnique<SystemEventReader>(reader_));
  SystemEventLogger logger(std::move(readers), Clock::RealClock());
  // Wait for the last event to be logged before visiting the records
  last_event_logged_.WaitForNotification();

  TypeExtractingVisitor forward_visitor(
      SystemEventVisitor::VisitDirection::FROM_START);
  logger.Visit(&forward_visitor);

  EXPECT_THAT(expected_types, ContainerEq(forward_visitor.GetRecordTypes()));
}

// Test the visit direction  FROM_END
TEST_F(EventLoggerTest, VisitFromEnd) {
  std::vector<EventType> expected_types = {
      EventType::END_OF_LOG,
      EventType::ERROR_DIMMS,
      EventType::UNCORRECTABLE_CPU_COMPLEX_ERROR,
      EventType::CPU_FAILURE,
      EventType::MEMORY_PARITY_ERROR,
      EventType::MULTI_BIT_ECC_ERROR,
      EventType::SINGLE_BIT_ECC_ERROR,
      EventType::LOG_AREA_RESET};
  std::vector<std::unique_ptr<SystemEventReader>> readers;
  readers.push_back(absl::WrapUnique<SystemEventReader>(reader_));
  SystemEventLogger logger(std::move(readers), Clock::RealClock());
  // Wait for the last event to be logged before visiting the records
  last_event_logged_.WaitForNotification();
  TypeExtractingVisitor reverse_visitor(
      SystemEventVisitor::VisitDirection::FROM_END);
  logger.Visit(&reverse_visitor);

  EXPECT_THAT(expected_types, ContainerEq(reverse_visitor.GetRecordTypes()));
}

}  // namespace

}  // namespace ecclesia
