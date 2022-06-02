/*
 * Copyright 2021 Google LLC
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

#include "ecclesia/lib/cache/timed_store.h"

#include "gtest/gtest.h"
#include "absl/time/time.h"
#include "ecclesia/lib/cache/rcu_snapshot.h"
#include "ecclesia/lib/time/clock_fake.h"

namespace ecclesia {
namespace {

// Simple store which increments a value on every read.
class SimpleTimedStore final : public TimedRcuStore<int> {
 public:
  using TimedRcuStore::TimedRcuStore;

  int ReadFresh() const override { return ++read_count; }

  mutable int read_count = 0;
};

TEST(TimedRcuStore, ReadUntilCacheExpires) {
  FakeClock clock;
  SimpleTimedStore store(clock, absl::Seconds(5));
  EXPECT_EQ(0, store.read_count);
  // Initial read.
  auto snapshot1 = store.Read();
  EXPECT_EQ(1, *snapshot1);
  EXPECT_EQ(1, store.read_count);
  // A re-read a the same time.
  auto snapshot2 = store.Read();
  EXPECT_EQ(1, *snapshot2);
  EXPECT_EQ(1, store.read_count);
  // Advacing to +3 or +5 seconds should also not re-read.
  clock.AdvanceTime(absl::Seconds(3));
  auto snapshot3 = store.Read();
  EXPECT_EQ(1, *snapshot3);
  EXPECT_EQ(1, store.read_count);
  clock.AdvanceTime(absl::Seconds(2));
  auto snapshot4 = store.Read();
  EXPECT_EQ(1, *snapshot4);
  EXPECT_EQ(1, store.read_count);
  // At +6 we should actually get a re-read, but not again at +10.
  clock.AdvanceTime(absl::Seconds(1));
  auto snapshot5 = store.Read();
  EXPECT_EQ(2, *snapshot5);
  EXPECT_EQ(2, store.read_count);
  clock.AdvanceTime(absl::Seconds(4));
  auto snapshot6 = store.Read();
  EXPECT_EQ(2, *snapshot6);
  EXPECT_EQ(2, store.read_count);
}

// Another simple store which also advances the clock during ReadFresh to
// replicate the effect of a Read taking a non-trivial amount of time.
class DelayedTimedStore final : public TimedRcuStore<int> {
 public:
  DelayedTimedStore(FakeClock &clock, absl::Duration delay,
                    absl::Duration duration)
      : TimedRcuStore<int>(clock, duration),
        read_count(0),
        clock_(clock),
        delay_(delay) {}

  int ReadFresh() const override {
    clock_.AdvanceTime(delay_);
    return ++read_count;
  }

  mutable int read_count;

 private:
  FakeClock &clock_;
  absl::Duration delay_;
};

TEST(TimedRcuStore, CacheTimeDoesNotIncludeReadTime) {
  FakeClock clock;
  DelayedTimedStore store(clock, absl::Seconds(2), absl::Seconds(7));
  EXPECT_EQ(0, store.read_count);
  // Initial read.
  auto snapshot1 = store.Read();
  EXPECT_EQ(1, *snapshot1);
  EXPECT_EQ(1, store.read_count);
  // Advancing to +7 should not read, but +8 should.
  clock.AdvanceTime(absl::Seconds(7));
  auto snapshot2 = store.Read();
  EXPECT_EQ(1, *snapshot2);
  EXPECT_EQ(1, store.read_count);
  // Advacing to +3 or +5 seconds should also not re-read.
  clock.AdvanceTime(absl::Seconds(1));
  auto snapshot3 = store.Read();
  EXPECT_EQ(2, *snapshot3);
  EXPECT_EQ(2, store.read_count);
  // Advancing another +7 should again not re-read.
  clock.AdvanceTime(absl::Seconds(7));
  auto snapshot4 = store.Read();
  EXPECT_EQ(2, *snapshot4);
  EXPECT_EQ(2, store.read_count);
}

}  // namespace
}  // namespace ecclesia
