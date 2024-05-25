/*
 * Copyright 2024 Google LLC
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

#include "ecclesia/lib/redfish/timing/query_timeout_manager.h"

#include <memory>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/base/thread_annotations.h"
#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "ecclesia/lib/testing/status.h"
#include "ecclesia/lib/thread/thread.h"
#include "ecclesia/lib/time/clock_fake.h"

namespace ecclesia {
namespace {
using ::ecclesia::IsOk;
using ::testing::Eq;

TEST(QueryTimeoutManagerTest, QueryTimeoutManagerTestWithNoTimeout) {
  ecclesia::FakeClock clock;
  absl::Duration timeout = absl::Seconds(10);
  QueryTimeoutManager query_timeout_manager(clock, timeout);
  ASSERT_THAT(query_timeout_manager.StartTiming(), IsOk());
  clock.AdvanceTime(absl::Seconds(5));
  // timeout should have 5 seconds left
  ASSERT_THAT(query_timeout_manager.ProbeTimeout(), IsOk());
  ASSERT_THAT(query_timeout_manager.GetRemainingTimeout(),
              Eq(absl::Seconds(5)));
  clock.AdvanceTime(absl::Seconds(3));
  // timeout should have 2 seconds left
  ASSERT_THAT(query_timeout_manager.ProbeTimeout(), IsOk());
  ASSERT_THAT(query_timeout_manager.GetRemainingTimeout(),
              Eq(absl::Seconds(2)));
  ASSERT_THAT(query_timeout_manager.EndTiming(), IsOk());
  // Start a new timing session, it should start again.
  ASSERT_THAT(query_timeout_manager.StartTiming(), IsOk());
  clock.AdvanceTime(absl::Seconds(1));
  ASSERT_THAT(query_timeout_manager.ProbeTimeout(), IsOk());
  ASSERT_THAT(query_timeout_manager.GetRemainingTimeout(),
              Eq(absl::Seconds(9)));
  ASSERT_THAT(query_timeout_manager.EndTiming(), IsOk());
}

TEST(QueryTimeoutManagerTest, QueryTimeoutManagerTestWithTimeout) {
  ecclesia::FakeClock clock;
  absl::Duration timeout = absl::Seconds(10);
  QueryTimeoutManager query_timeout_manager(clock, timeout);
  ASSERT_THAT(query_timeout_manager.StartTiming(), IsOk());
  clock.AdvanceTime(absl::Seconds(5));
  // timeout should have 5 seconds left
  ASSERT_THAT(query_timeout_manager.ProbeTimeout(), IsOk());
  clock.AdvanceTime(absl::Seconds(6));
  // timeout is exceeded.
  ASSERT_THAT(query_timeout_manager.ProbeTimeout().code(),
              Eq(absl::StatusCode::kDeadlineExceeded));
  ASSERT_THAT(query_timeout_manager.EndTiming().code(),
              Eq(absl::StatusCode::kDeadlineExceeded));
}

TEST(QueryTimeoutManagerTest, QueryTimeoutManagerHandlesMisuse) {
  ecclesia::FakeClock clock;
  absl::Duration timeout = absl::Seconds(10);
  QueryTimeoutManager query_timeout_manager(clock, timeout);
  ASSERT_THAT(query_timeout_manager.ProbeTimeout().code(),
              Eq(absl::StatusCode::kFailedPrecondition));
  ASSERT_THAT(query_timeout_manager.EndTiming().code(),
              Eq(absl::StatusCode::kFailedPrecondition));
  ASSERT_THAT(query_timeout_manager.StartTiming(), IsOk());
  clock.AdvanceTime(absl::Seconds(5));
  // timeout should have 5 seconds left
  ASSERT_THAT(query_timeout_manager.ProbeTimeout(), IsOk());
  ASSERT_THAT(query_timeout_manager.EndTiming(), IsOk());
  // Trying to end timing again should result in error.
  ASSERT_THAT(query_timeout_manager.EndTiming().code(),
              Eq(absl::StatusCode::kFailedPrecondition));
}

TEST(QueryTimeoutManagerTest, QueryTimeoutManagerThreadSafetyTest) {
  static constexpr int kNumThreads = 5;
  absl::Mutex clock_mutex;
  ecclesia::FakeClock ABSL_GUARDED_BY(clock_mutex) clock;
  absl::Duration timeout = absl::Seconds(10);
  QueryTimeoutManager query_timeout_manager(clock, timeout);
  ASSERT_THAT(query_timeout_manager.StartTiming(), IsOk());
  auto request_simulating_func = [&]() {
    absl::MutexLock lock(&clock_mutex);
    clock.AdvanceTime(absl::Seconds(1));
    ASSERT_THAT(query_timeout_manager.ProbeTimeout(), IsOk());
  };
  std::vector<std::unique_ptr<ThreadInterface>> threads;
  auto *thread_factory = GetDefaultThreadFactory();
  threads.reserve(kNumThreads);
  for (int i = 0; i < kNumThreads; i++) {
    threads.push_back(thread_factory->New(request_simulating_func));
  }
  for (auto &thread : threads) {
    thread->Join();
  }
  ASSERT_THAT(query_timeout_manager.EndTiming(), IsOk());
  EXPECT_THAT(query_timeout_manager.GetRemainingTimeout(),
              Eq(absl::Seconds(5)));
}

}  // namespace
}  // namespace ecclesia
