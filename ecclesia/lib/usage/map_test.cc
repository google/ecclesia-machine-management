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

#include "ecclesia/lib/usage/map.h"

#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/testing/status.h"

namespace ecclesia {
namespace {

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Ge;
using ::testing::Le;

class PersistentUsageMapTest : public ::testing::Test {
 public:
  PersistentUsageMapTest() : fs_(GetTestTempdirPath()) {}

 protected:
  TestFilesystem fs_;
};

TEST_F(PersistentUsageMapTest, CreateWithMissingFileIsEmpty) {
  PersistentUsageMap usage_map(
      {.persistent_file = fs_.GetTruePath("/empty.usage")});

  // Make sure the map has no entries.
  bool has_entries = false;
  usage_map.WithEntries(
      [&](const std::string &operation, const std::string &user,
          const absl::Time &timestamp) { has_entries = true; });
  EXPECT_FALSE(has_entries);
}

TEST_F(PersistentUsageMapTest, NewUsesReplaceOld) {
  PersistentUsageMap usage_map(
      {.persistent_file = fs_.GetTruePath("/test_a.usage")});

  // Make three writes, with the third expected to replace the first.
  usage_map.RecordUse("rpc1", "user");
  absl::Time after_first = absl::Now();
  usage_map.RecordUse("rpc2", "user");
  absl::Time after_second = absl::Now();
  usage_map.RecordUse("rpc1", "user");
  absl::Time after_third = absl::Now();

  // Verify the map has the two expected entries.
  int num_entries = 0;
  usage_map.WithEntries([&](const std::string &operation,
                            const std::string &user,
                            const absl::Time &timestamp) {
    num_entries += 1;
    EXPECT_THAT(user, Eq("user"));
    if (operation == "rpc1") {
      EXPECT_THAT(timestamp, AllOf(Ge(after_second), Le(after_third)));
    } else if (operation == "rpc2") {
      EXPECT_THAT(timestamp, AllOf(Ge(after_first), Le(after_second)));
    } else {
      ADD_FAILURE() << "unexpected operation: " << operation;
    }
  });
  EXPECT_THAT(num_entries, Eq(2));

  // Verify that we have no writes.
  auto stats = usage_map.GetStats();
  EXPECT_THAT(stats.total_writes, Eq(0));
  EXPECT_THAT(stats.automatic_writes, Eq(0));
  EXPECT_THAT(stats.failed_writes, Eq(0));
}

TEST_F(PersistentUsageMapTest, OldDoesNotReplaceNew) {
  PersistentUsageMap usage_map(
      {.persistent_file = fs_.GetTruePath("/test_b.usage")});

  // Make three writes, but backdate the third one to before the first one.
  absl::Time before_first = absl::Now();
  usage_map.RecordUse("rpc1", "user");
  absl::Time before_second = absl::Now();
  usage_map.RecordUse("rpc2", "user");
  absl::Time before_third = absl::Now();
  usage_map.RecordUse("rpc1", "user", before_first - absl::Seconds(1));

  // Verify the map has the two expected entries.
  int num_entries = 0;
  usage_map.WithEntries([&](const std::string &operation,
                            const std::string &user,
                            const absl::Time &timestamp) {
    num_entries += 1;
    EXPECT_THAT(user, Eq("user"));
    if (operation == "rpc1") {
      EXPECT_THAT(timestamp, AllOf(Ge(before_first), Le(before_second)));
    } else if (operation == "rpc2") {
      EXPECT_THAT(timestamp, AllOf(Ge(before_second), Le(before_third)));
    } else {
      ADD_FAILURE() << "unexpected operation: " << operation;
    }
  });
  EXPECT_THAT(num_entries, Eq(2));

  // Verify that we have no writes.
  auto stats = usage_map.GetStats();
  EXPECT_THAT(stats.total_writes, Eq(0));
  EXPECT_THAT(stats.automatic_writes, Eq(0));
  EXPECT_THAT(stats.failed_writes, Eq(0));
}

TEST_F(PersistentUsageMapTest, SaveRecordsAndLoadRecords) {
  PersistentUsageMap first_map(
      {.persistent_file = fs_.GetTruePath("/saved.usage")});

  // Verify that we have no writes at the start.
  auto stats = first_map.GetStats();
  EXPECT_THAT(stats.total_writes, Eq(0));
  EXPECT_THAT(stats.automatic_writes, Eq(0));
  EXPECT_THAT(stats.failed_writes, Eq(0));

  // Write out a few records.
  first_map.RecordUse("rpc1", "user");
  first_map.RecordUse("rpc2", "user");
  first_map.RecordUse("rpc3", "hacker");
  EXPECT_THAT(first_map.WriteToPersistentStore(), IsOk());

  // Verify that we have a write (manual, not automatic).
  stats = first_map.GetStats();
  EXPECT_THAT(stats.total_writes, Eq(1));
  EXPECT_THAT(stats.automatic_writes, Eq(0));
  EXPECT_THAT(stats.failed_writes, Eq(0));

  // Extract the three timestamps from the map for later comparison.
  absl::Time timestamps[3];
  first_map.WithEntries([&](const std::string &operation, const std::string &,
                            const absl::Time &timestamp) {
    if (operation == "rpc1") {
      timestamps[0] = timestamp;
    } else if (operation == "rpc2") {
      timestamps[1] = timestamp;
    } else if (operation == "rpc3") {
      timestamps[2] = timestamp;
    } else {
      ADD_FAILURE() << "unexpected operation: " << operation;
    }
  });

  // Now load up a second version of the map and check that it matches. Note
  // that although you can't actually use maps multiple maps with a single file,
  // there's no actual conflict if we don't ever flush first_map again.
  PersistentUsageMap second_map(
      {.persistent_file = fs_.GetTruePath("/saved.usage")});
  int num_entries = 0;
  second_map.WithEntries([&](const std::string &operation,
                             const std::string &user,
                             const absl::Time &timestamp) {
    num_entries += 1;
    if (operation == "rpc1") {
      EXPECT_THAT(user, Eq("user"));
      EXPECT_THAT(timestamp, Eq(timestamps[0]));
    } else if (operation == "rpc2") {
      EXPECT_THAT(user, Eq("user"));
      EXPECT_THAT(timestamp, Eq(timestamps[1]));
    } else if (operation == "rpc3") {
      EXPECT_THAT(user, Eq("hacker"));
      EXPECT_THAT(timestamp, Eq(timestamps[2]));
    } else {
      ADD_FAILURE() << "unexpected operation: " << operation;
    }
  });
  EXPECT_THAT(num_entries, Eq(3));

  // Verify that we have no writes in the second map.
  stats = second_map.GetStats();
  EXPECT_THAT(stats.total_writes, Eq(0));
  EXPECT_THAT(stats.automatic_writes, Eq(0));
  EXPECT_THAT(stats.failed_writes, Eq(0));
}

TEST_F(PersistentUsageMapTest, SaveRecordOnEveryUpdate) {
  PersistentUsageMap usage_map({
      .persistent_file = fs_.GetTruePath("/on_every_update.usage"),
      .auto_write_on_older_than = absl::ZeroDuration(),
  });

  // Verify that we have no writes at the start.
  auto stats = usage_map.GetStats();
  EXPECT_THAT(stats.total_writes, Eq(0));
  EXPECT_THAT(stats.automatic_writes, Eq(0));
  EXPECT_THAT(stats.failed_writes, Eq(0));

  // Write out a few records. All but one of these should trigger a write.
  absl::Time before_first = absl::Now();
  usage_map.RecordUse("rpc1", "user");
  usage_map.RecordUse("rpc2", "user");
  usage_map.RecordUse("rpc3", "hacker");
  usage_map.RecordUse("rpc1", "user", before_first);  // Too old, no write!
  usage_map.RecordUse("rpc2", "user");
  usage_map.RecordUse("rpc3", "hacker");

  // Verify that we have five automatic writes.
  stats = usage_map.GetStats();
  EXPECT_THAT(stats.total_writes, Eq(5));
  EXPECT_THAT(stats.automatic_writes, Eq(5));
  EXPECT_THAT(stats.failed_writes, Eq(0));
}

TEST_F(PersistentUsageMapTest, SaveRecordOnOldUpdates) {
  PersistentUsageMap usage_map({
      .persistent_file = fs_.GetTruePath("/on_old_update.usage"),
      .auto_write_on_older_than = absl::Minutes(1),
  });

  // Verify that we have no writes at the start.
  auto stats = usage_map.GetStats();
  EXPECT_THAT(stats.total_writes, Eq(0));
  EXPECT_THAT(stats.automatic_writes, Eq(0));
  EXPECT_THAT(stats.failed_writes, Eq(0));

  // Write out a few records. We manually control the timestamps so that after
  // the first three updates only one of the followups should trigger a write.
  absl::Time start_time = absl::Now();
  usage_map.RecordUse("rpc1", "user", start_time);
  usage_map.RecordUse("rpc2", "user", start_time + absl::Seconds(1));
  usage_map.RecordUse("rpc3", "hacker", start_time + absl::Seconds(2));
  // Too new, no updates.
  usage_map.RecordUse("rpc1", "user", start_time + absl::Seconds(3));
  usage_map.RecordUse("rpc2", "user", start_time + absl::Seconds(4));
  usage_map.RecordUse("rpc3", "hacker", start_time + absl::Seconds(5));
  // The first two entries are just slightly too new, so only the last writes.
  usage_map.RecordUse("rpc1", "user", start_time + absl::Seconds(63));
  usage_map.RecordUse("rpc2", "user", start_time + absl::Seconds(64));
  usage_map.RecordUse("rpc3", "hacker", start_time + absl::Seconds(66));

  // Verify that we have four automatic writes. This is the original three uses
  // and then the very final one.
  stats = usage_map.GetStats();
  EXPECT_THAT(stats.total_writes, Eq(4));
  EXPECT_THAT(stats.automatic_writes, Eq(4));
  EXPECT_THAT(stats.failed_writes, Eq(0));
}

TEST_F(PersistentUsageMapTest, LoadingRecordsDoesNotTriggerWrites) {
  PersistentUsageMap first_map(
      {.persistent_file = fs_.GetTruePath("/no_writes_on_load.usage")});

  // Write out a few records.
  first_map.RecordUse("rpc1", "user");
  first_map.RecordUse("rpc2", "user");
  first_map.RecordUse("rpc3", "hacker");
  EXPECT_THAT(first_map.WriteToPersistentStore(), IsOk());

  // Verify that we have a write (manual, not automatic).
  auto stats = first_map.GetStats();
  EXPECT_THAT(stats.total_writes, Eq(1));
  EXPECT_THAT(stats.automatic_writes, Eq(0));
  EXPECT_THAT(stats.failed_writes, Eq(0));

  // Now load up a second version of the map and check that it matches. Note
  // that although you can't actually use maps multiple maps with a single file,
  // there's no actual conflict if we don't ever flush first_map again.
  //
  // We configure the test to write on every update. Make sure that loading the
  // map doesn't cause it to write the map!
  PersistentUsageMap second_map({
      .persistent_file = fs_.GetTruePath("/no_writes_on_load.usage"),
      .auto_write_on_older_than = absl::ZeroDuration(),
  });
  // Count up the number of entries. We don't fully validate any of the values
  // here as other tests do that, this is just for sanity.
  int num_entries = 0;
  second_map.WithEntries(
      [&](const std::string &operation, const std::string &user,
          const absl::Time &timestamp) { num_entries += 1; });
  EXPECT_THAT(num_entries, Eq(3));

  // Verify that we have no writes in the second map.
  stats = second_map.GetStats();
  EXPECT_THAT(stats.total_writes, Eq(0));
  EXPECT_THAT(stats.automatic_writes, Eq(0));
  EXPECT_THAT(stats.failed_writes, Eq(0));
}

TEST_F(PersistentUsageMapTest, WriteFileFails) {
  // Unfortunately there's not a great way to make the actual writing of the
  // file fail artificially. The easiest thing we can do is to make the map file
  // path be a directory.
  fs_.CreateDir("/failed_write.usage");
  PersistentUsageMap first_map(
      {.persistent_file = fs_.GetTruePath("/failed_write.usage")});

  // Write out a few records.
  first_map.RecordUse("rpc1", "user");
  first_map.RecordUse("rpc2", "user");
  first_map.RecordUse("rpc3", "hacker");
  EXPECT_THAT(first_map.WriteToPersistentStore(), IsStatusInternal());

  // Verify that we have a failed write.
  auto stats = first_map.GetStats();
  EXPECT_THAT(stats.total_writes, Eq(1));
  EXPECT_THAT(stats.automatic_writes, Eq(0));
  EXPECT_THAT(stats.failed_writes, Eq(1));
}

}  // namespace
}  // namespace ecclesia
