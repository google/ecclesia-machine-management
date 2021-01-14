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

#include "ecclesia/magent/lib/event_logger/indus/system_event_visitors.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "ecclesia/lib/mcedecoder/cpu_topology.h"
#include "ecclesia/lib/time/clock.h"
#include "ecclesia/magent/lib/event_logger/event_logger.h"
#include "ecclesia/magent/lib/event_logger/system_event_visitors.h"
#include "ecclesia/magent/lib/event_reader/event_reader.h"

namespace ecclesia {
namespace {

using ::testing::Return;

class MockEventReader : public SystemEventReader {
 public:
  MOCK_METHOD(absl::optional<SystemEventRecord>, ReadEvent, ());
};

class MockClock : public Clock {
 public:
  MOCK_METHOD(absl::Time, Now, (), (const, override));
};

// Cpu topology as observed on an Indus machine.
class FakeCpuTopology : public CpuTopologyInterface {
 public:
  absl::StatusOr<int> GetSocketIdForLpu(int lpu) const override {
    if ((lpu >= 0 && lpu < 28) || (lpu >= 56 && lpu < 84)) return 0;
    if ((lpu >= 28 && lpu < 56) || (lpu >= 84 && lpu < 112)) return 1;
    return absl::NotFoundError(absl::StrFormat("unknown lpu %d", lpu));
  }
};

class IndusVisitorTest : public ::testing::Test {
 protected:
  IndusVisitorTest() { reader_ = new MockEventReader(); }

  MockEventReader *reader_;
  absl::Notification last_event_logged_;
};

TEST_F(IndusVisitorTest, MemoryErrorCounts) {
  MachineCheck mces[] = {
      {
          // This is a mesh-to-mem correctible error on channel 1. For
          // mesh-to-mem the decoder cannot disambiguate between the two dimms
          // on a given channel, since it does not perform
          // memory-address-decoding. It defaults to the dimm on slot 0 on that
          // channel. So instead of DIMM15, the error will be reported on
          // DIMM14.
          .mci_status = 0x9c000040010400a1,
          .mci_address = 0x35a4456040,
          .mci_misc = 0x200414a228001086,
          .mcg_status = 0,
          .cpu = 54,
          .bank = 7,
      },
      {
          // 3 correctible errors on DIMM10
          .mci_status = 0xc80000c100800090,
          .mci_address = 0,
          .mci_misc = 0xd129e00204404400,
          .mcg_status = 0,
          .cpu = 6,
          .bank = 13,
      },
  };

  EXPECT_CALL(*reader_, ReadEvent)
      .WillOnce(Return(SystemEventRecord{.record = mces[0]}))
      .WillOnce(Return(SystemEventRecord{.record = mces[1]}))
      .WillOnce(Return(SystemEventRecord{.record = mces[0]}))
      .WillOnce(Return(SystemEventRecord{.record = mces[1]}))
      .WillOnce(Return(SystemEventRecord{.record = mces[0]}))
      .WillOnce([&]() {
        last_event_logged_.Notify();
        return absl::nullopt;
      })
      .WillRepeatedly(Return(absl::nullopt));

  std::unique_ptr<MockClock> clock = absl::make_unique<MockClock>();

  EXPECT_CALL(*clock.get(), Now)
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

  {
    // Visit all of the events
    auto dimm_visitor = CreateIndusDimmErrorCountingVisitor(
        absl::UnixEpoch(), absl::make_unique<FakeCpuTopology>());

    logger.Visit(dimm_visitor.get());

    auto dimm_error_counts = dimm_visitor->GetDimmErrorCounts();

    EXPECT_EQ(dimm_error_counts.size(), 2);
    EXPECT_EQ(dimm_error_counts[14].correctable, 3);
    EXPECT_EQ(dimm_error_counts[14].uncorrectable, 0);
    EXPECT_EQ(dimm_error_counts[10].correctable, 6);
    EXPECT_EQ(dimm_error_counts[10].uncorrectable, 0);
  }
  {
    // Visit the last 3 records
    auto dimm_visitor = CreateIndusDimmErrorCountingVisitor(
        absl::UnixEpoch() + absl::Seconds(2),
        absl::make_unique<FakeCpuTopology>());

    logger.Visit(dimm_visitor.get());

    auto dimm_error_counts = dimm_visitor->GetDimmErrorCounts();

    EXPECT_EQ(dimm_error_counts.size(), 2);
    EXPECT_EQ(dimm_error_counts[14].correctable, 2);
    EXPECT_EQ(dimm_error_counts[14].uncorrectable, 0);
    EXPECT_EQ(dimm_error_counts[10].correctable, 3);
    EXPECT_EQ(dimm_error_counts[10].uncorrectable, 0);
  }
}

TEST_F(IndusVisitorTest, CpuErrorCounts) {
  MachineCheck mces[] = {
      {
          // Uncorrectible cpu cache error on socket 0
          .mci_status = 0xbd80000000100134,
          .mci_address = 0x166fab040,
          .mci_misc = 0x86,
          .mcg_status = 7,
          .cpu = 59,
          .bank = 1,
      },
      {
          // Uncorrectible instruction fetch error on socket 1
          .mci_status = 0xbd800000000c0150,
          .mci_address = 0x16a616040,
          .mci_misc = 0x86,
          .mcg_status = 7,
          .cpu = 28,
          .bank = 0,
      }};

  EXPECT_CALL(*reader_, ReadEvent)
      .WillOnce(Return(SystemEventRecord{.record = mces[0]}))
      .WillOnce(Return(SystemEventRecord{.record = mces[1]}))
      .WillOnce(Return(SystemEventRecord{.record = mces[0]}))
      .WillOnce(Return(SystemEventRecord{.record = mces[1]}))
      .WillOnce(Return(SystemEventRecord{.record = mces[0]}))
      .WillOnce([&]() {
        last_event_logged_.Notify();
        return absl::nullopt;
      })
      .WillRepeatedly(Return(absl::nullopt));

  std::unique_ptr<MockClock> clock = absl::make_unique<MockClock>();

  EXPECT_CALL(*clock.get(), Now)
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

  {
    // Visit all of the 5 events
    auto cpu_visitor = CreateIndusCpuErrorCountingVisitor(
        absl::UnixEpoch(), absl::make_unique<FakeCpuTopology>());
    logger.Visit(cpu_visitor.get());
    auto cpu_error_counts = cpu_visitor->GetCpuErrorCounts();
    EXPECT_EQ(cpu_error_counts.size(), 2);
    EXPECT_EQ(cpu_error_counts[0].correctable, 0);
    EXPECT_EQ(cpu_error_counts[0].uncorrectable, 3);
    EXPECT_EQ(cpu_error_counts[1].correctable, 0);
    EXPECT_EQ(cpu_error_counts[1].uncorrectable, 2);
  }

  {
    // Visit the last 2 records
    auto cpu_visitor = CreateIndusCpuErrorCountingVisitor(
        absl::UnixEpoch() + absl::Seconds(3),
        absl::make_unique<FakeCpuTopology>());
    logger.Visit(cpu_visitor.get());
    auto cpu_error_counts = cpu_visitor->GetCpuErrorCounts();
    EXPECT_EQ(cpu_error_counts.size(), 2);
    EXPECT_EQ(cpu_error_counts[0].correctable, 0);
    EXPECT_EQ(cpu_error_counts[0].uncorrectable, 1);
    EXPECT_EQ(cpu_error_counts[1].correctable, 0);
    EXPECT_EQ(cpu_error_counts[1].uncorrectable, 1);
  }
}

}  // namespace

}  // namespace ecclesia
