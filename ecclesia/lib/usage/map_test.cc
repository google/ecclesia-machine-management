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

#include <cstddef>
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/testing/status.h"

namespace ecclesia {
namespace {

using ::testing::AllOf;
using ::testing::AnyOf;
using ::testing::Eq;
using ::testing::Ge;
using ::testing::Gt;
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
  EXPECT_THAT(usage_map.GetMostRecentTimestamp(), Eq(absl::InfinitePast()));
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
  EXPECT_THAT(usage_map.GetMostRecentTimestamp(), Gt(absl::InfinitePast()));

  // Verify that we have no writes.
  auto stats = usage_map.GetStats();
  EXPECT_THAT(stats.total_writes, Eq(0));
  EXPECT_THAT(stats.automatic_writes, Eq(0));
  EXPECT_THAT(stats.failed_writes, Eq(0));
  EXPECT_THAT(stats.proto_size, Eq(0));
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
  EXPECT_THAT(usage_map.GetMostRecentTimestamp(), Gt(absl::InfinitePast()));

  // Verify that we have no writes.
  auto stats = usage_map.GetStats();
  EXPECT_THAT(stats.total_writes, Eq(0));
  EXPECT_THAT(stats.automatic_writes, Eq(0));
  EXPECT_THAT(stats.failed_writes, Eq(0));
  EXPECT_THAT(stats.proto_size, Eq(0));
}

TEST_F(PersistentUsageMapTest, SaveRecordsAndLoadRecords) {
  PersistentUsageMap first_map(
      {.persistent_file = fs_.GetTruePath("/saved.usage")});

  // Verify that we have no writes at the start.
  auto stats = first_map.GetStats();
  EXPECT_THAT(stats.total_writes, Eq(0));
  EXPECT_THAT(stats.automatic_writes, Eq(0));
  EXPECT_THAT(stats.failed_writes, Eq(0));
  EXPECT_THAT(stats.proto_size, Eq(0));

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
  EXPECT_THAT(stats.proto_size, Gt(0));
  size_t first_size = stats.proto_size;

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
  EXPECT_THAT(first_map.GetMostRecentTimestamp(), Eq(timestamps[2]));

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
  EXPECT_THAT(second_map.GetMostRecentTimestamp(), Eq(timestamps[2]));

  // Verify that we have no writes in the second map.
  stats = second_map.GetStats();
  EXPECT_THAT(stats.total_writes, Eq(0));
  EXPECT_THAT(stats.automatic_writes, Eq(0));
  EXPECT_THAT(stats.failed_writes, Eq(0));
  EXPECT_THAT(stats.proto_size, Eq(first_size));
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
  EXPECT_THAT(stats.proto_size, Eq(0));

  // Write out a few records. All but one of these should trigger a write.
  absl::Time before_first = absl::Now();
  usage_map.RecordUse("rpc1", "user");
  usage_map.RecordUse("rpc2", "user");
  usage_map.RecordUse("rpc3", "hacker");
  usage_map.RecordUse("rpc1", "user", before_first);  // Too old, no write!
  usage_map.RecordUse("rpc2", "user");
  usage_map.RecordUse("rpc3", "hacker");

  // Verify that we have five automatic writes.
  EXPECT_THAT(usage_map.GetMostRecentTimestamp(), Gt(absl::InfinitePast()));
  stats = usage_map.GetStats();
  EXPECT_THAT(stats.total_writes, Eq(5));
  EXPECT_THAT(stats.automatic_writes, Eq(5));
  EXPECT_THAT(stats.failed_writes, Eq(0));
  EXPECT_THAT(stats.proto_size, Gt(0));
}

TEST_F(PersistentUsageMapTest, SaveRecordOnCombinedUpdate) {
  PersistentUsageMap usage_map({
      .persistent_file = fs_.GetTruePath("/on_every_combined_update.usage"),
      .auto_write_on_older_than = absl::ZeroDuration(),
  });

  // Verify that we have no writes at the start.
  auto stats = usage_map.GetStats();
  EXPECT_THAT(stats.total_writes, Eq(0));
  EXPECT_THAT(stats.automatic_writes, Eq(0));
  EXPECT_THAT(stats.failed_writes, Eq(0));
  EXPECT_THAT(stats.proto_size, Eq(0));

  // Write out a few records. Then write them again, but they are too old.
  absl::Time before_first = absl::Now();
  usage_map.RecordUses(
      {{"rpc1", "user"}, {"rpc2", "user"}, {"rpc3", "hacker"}});
  usage_map.RecordUses({{"rpc1", "user"}, {"rpc2", "user"}, {"rpc3", "hacker"}},
                       before_first);  // Too old, no write!

  // Verify that we have five automatic writes.
  EXPECT_THAT(usage_map.GetMostRecentTimestamp(), Gt(absl::InfinitePast()));
  stats = usage_map.GetStats();
  EXPECT_THAT(stats.total_writes, Eq(1));
  EXPECT_THAT(stats.automatic_writes, Eq(1));
  EXPECT_THAT(stats.failed_writes, Eq(0));
  EXPECT_THAT(stats.proto_size, Gt(0));
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
  EXPECT_THAT(stats.proto_size, Eq(0));

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
  EXPECT_THAT(usage_map.GetMostRecentTimestamp(),
              Eq(start_time + absl::Seconds(66)));
  stats = usage_map.GetStats();
  EXPECT_THAT(stats.total_writes, Eq(4));
  EXPECT_THAT(stats.automatic_writes, Eq(4));
  EXPECT_THAT(stats.failed_writes, Eq(0));
  EXPECT_THAT(stats.proto_size, Gt(0));
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
  EXPECT_THAT(first_map.GetMostRecentTimestamp(), Gt(absl::InfinitePast()));
  auto stats = first_map.GetStats();
  EXPECT_THAT(stats.total_writes, Eq(1));
  EXPECT_THAT(stats.automatic_writes, Eq(0));
  EXPECT_THAT(stats.failed_writes, Eq(0));
  EXPECT_THAT(stats.proto_size, Gt(0));
  size_t first_size = stats.proto_size;

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
  EXPECT_THAT(second_map.GetMostRecentTimestamp(),
              Eq(first_map.GetMostRecentTimestamp()));

  // Verify that we have no writes in the second map.
  stats = second_map.GetStats();
  EXPECT_THAT(stats.total_writes, Eq(0));
  EXPECT_THAT(stats.automatic_writes, Eq(0));
  EXPECT_THAT(stats.failed_writes, Eq(0));
  EXPECT_THAT(stats.proto_size, Eq(first_size));
}

TEST_F(PersistentUsageMapTest, WriteFileFails) {
  // Unfortunately there's not a great way to make the actual writing of the
  // file fail artificially. The easiest thing we can do is to make the map file
  // path be a directory.
  fs_.CreateDir("/failed_write.usage");
  PersistentUsageMap usage_map(
      {.persistent_file = fs_.GetTruePath("/failed_write.usage")});

  // Write out a few records.
  usage_map.RecordUse("rpc1", "user");
  usage_map.RecordUse("rpc2", "user");
  usage_map.RecordUse("rpc3", "hacker");
  EXPECT_THAT(usage_map.WriteToPersistentStore(), IsStatusInternal());

  // Verify that we have a failed write.
  EXPECT_THAT(usage_map.GetMostRecentTimestamp(), Gt(absl::InfinitePast()));
  auto stats = usage_map.GetStats();
  EXPECT_THAT(stats.total_writes, Eq(1));
  EXPECT_THAT(stats.automatic_writes, Eq(0));
  EXPECT_THAT(stats.failed_writes, Eq(1));
  EXPECT_THAT(stats.proto_size, Eq(0));
}

TEST_F(PersistentUsageMapTest, WrittenFileIsEmptyWithZeroLimit) {
  PersistentUsageMap usage_map({
      .persistent_file = fs_.GetTruePath("/zero_limit.usage"),
      .maximum_proto_size = 0,
  });

  // Make three writes.
  usage_map.RecordUse("rpc1", "user");
  usage_map.RecordUse("rpc2", "user");
  usage_map.RecordUse("rpc1", "user");

  // Verify the map has the two expected entries.
  int num_entries = 0;
  usage_map.WithEntries([&](const std::string &operation,
                            const std::string &user,
                            const absl::Time &timestamp) {
    num_entries += 1;
    EXPECT_THAT(user, Eq("user"));
    EXPECT_THAT(operation, AnyOf("rpc1", "rpc2"));
  });
  EXPECT_THAT(num_entries, Eq(2));
  EXPECT_THAT(usage_map.GetMostRecentTimestamp(), Gt(absl::InfinitePast()));

  // Now write out the file. This should wipe all of the contents.
  EXPECT_THAT(usage_map.WriteToPersistentStore(), IsOk());

  // Verify that we have a write.
  auto stats = usage_map.GetStats();
  EXPECT_THAT(stats.total_writes, Eq(1));
  EXPECT_THAT(stats.automatic_writes, Eq(0));
  EXPECT_THAT(stats.failed_writes, Eq(0));
  EXPECT_THAT(stats.proto_size, Eq(0));  // Proto should be empty.

  // Verify that the usage map is now empty.
  usage_map.WithEntries(
      [](const std::string &, const std::string &, const absl::Time &) {
        ADD_FAILURE() << "usage map still contains entries";
      });
  EXPECT_THAT(usage_map.GetMostRecentTimestamp(), Eq(absl::InfinitePast()));
}

TEST_F(PersistentUsageMapTest, WrittenFileUnderPreciseSizeLimits) {
  // Make maps with several different limits at the boundary of one or more
  // of the messages.
  PersistentUsageMap map_74({
      .persistent_file = fs_.GetTruePath("/size_74_map.usage"),
      .maximum_proto_size = 74,
  });
  PersistentUsageMap map_73({
      .persistent_file = fs_.GetTruePath("/size_73_map.usage"),
      .maximum_proto_size = 73,
  });
  PersistentUsageMap map_54({
      .persistent_file = fs_.GetTruePath("/size_54_map.usage"),
      .maximum_proto_size = 54,
  });
  PersistentUsageMap map_53({
      .persistent_file = fs_.GetTruePath("/size_53_map.usage"),
      .maximum_proto_size = 53,
  });
  PersistentUsageMap map_28({
      .persistent_file = fs_.GetTruePath("/size_28_map.usage"),
      .maximum_proto_size = 28,
  });
  PersistentUsageMap map_27({
      .persistent_file = fs_.GetTruePath("/size_27_map.usage"),
      .maximum_proto_size = 27,
  });
  PersistentUsageMap *all_maps[] = {&map_74, &map_73, &map_54,
                                    &map_53, &map_28, &map_27};
  absl::flat_hash_map<PersistentUsageMap *, size_t> all_map_sizes = {
      {&map_74, 74}, {&map_73, 54}, {&map_54, 54},
      {&map_53, 28}, {&map_28, 28}, {&map_27, 0}};

  // Populate the same set of entries into every single map. We use specific
  // timestamps in all of the calls so that we can precisely know the resulting
  // output proto size.
  absl::Time most_recent_time;
  for (PersistentUsageMap *usage_map : all_maps) {
    absl::Time timestamp = absl::UnixEpoch();
    timestamp += absl::Seconds(2000000);
    usage_map->RecordUse("rpc1", "user", timestamp);
    timestamp += absl::Seconds(200000000);
    usage_map->RecordUse("rpc2", "otheruser", timestamp);
    timestamp += absl::Seconds(30000000000) + absl::Nanoseconds(123456789);
    usage_map->RecordUse("rpc3", "third", timestamp);
    most_recent_time = timestamp;
  }

  // Verify that every map has three entries.
  for (PersistentUsageMap *usage_map : all_maps) {
    int num_entries = 0;
    usage_map->WithEntries([&](const std::string &operation,
                               const std::string &user,
                               const absl::Time &timestamp) {
      num_entries += 1;
      EXPECT_THAT(operation, AnyOf("rpc1", "rpc2", "rpc3"));
    });
    EXPECT_THAT(num_entries, Eq(3));
    EXPECT_THAT(usage_map->GetMostRecentTimestamp(), Eq(most_recent_time));
  }

  // Now write out all the maps. This should trim them all.
  for (PersistentUsageMap *usage_map : all_maps) {
    EXPECT_THAT(usage_map->WriteToPersistentStore(), IsOk());

    // Verify that we have a write.
    auto stats = usage_map->GetStats();
    EXPECT_THAT(stats.total_writes, Eq(1));
    EXPECT_THAT(stats.automatic_writes, Eq(0));
    EXPECT_THAT(stats.failed_writes, Eq(0));
    EXPECT_THAT(stats.proto_size, Eq(all_map_sizes.at(usage_map)));
  }

  // Verify that only the largest map has three entries.
  for (PersistentUsageMap *usage_map : {&map_74}) {
    int num_entries = 0;
    usage_map->WithEntries([&](const std::string &operation,
                               const std::string &user,
                               const absl::Time &timestamp) {
      num_entries += 1;
      EXPECT_THAT(operation, AnyOf("rpc1", "rpc2", "rpc3"));
    });
    EXPECT_THAT(num_entries, Eq(3));
  }
  // Verify that the next two largest maps have two entries.
  for (PersistentUsageMap *usage_map : {&map_73, &map_54}) {
    int num_entries = 0;
    usage_map->WithEntries([&](const std::string &operation,
                               const std::string &user,
                               const absl::Time &timestamp) {
      num_entries += 1;
      EXPECT_THAT(operation, AnyOf("rpc2", "rpc3"));
    });
    EXPECT_THAT(num_entries, Eq(2));
  }
  // Verify that the next two largest maps have one entry.
  for (PersistentUsageMap *usage_map : {&map_53, &map_28}) {
    int num_entries = 0;
    usage_map->WithEntries([&](const std::string &operation,
                               const std::string &user,
                               const absl::Time &timestamp) {
      num_entries += 1;
      EXPECT_THAT(operation, Eq("rpc3"));
    });
    EXPECT_THAT(num_entries, Eq(1));
  }
  // Verify that the smallest map has no entries.
  for (PersistentUsageMap *usage_map : {&map_27}) {
    usage_map->WithEntries([&](const std::string &operation,
                               const std::string &user,
                               const absl::Time &timestamp) {
      ADD_FAILURE() << "the smallest map should contain nothing";
    });
  }

  // Verify that every but the smallest still has the same most recent time.
  for (PersistentUsageMap *usage_map : all_maps) {
    if (usage_map == &map_27) {
      EXPECT_THAT(usage_map->GetMostRecentTimestamp(),
                  Eq(absl::InfinitePast()));
    } else {
      EXPECT_THAT(usage_map->GetMostRecentTimestamp(), Eq(most_recent_time));
    }
  }
}

TEST_F(PersistentUsageMapTest, TrimOlderEntries) {
  PersistentUsageMap usage_map({
      .persistent_file = fs_.GetTruePath("/last_3s.usage"),
      .trim_entries_older_than = absl::Seconds(3),
  });

  // Write out a few records. Space them one second apart.
  absl::Time start_time = absl::Now();
  usage_map.RecordUse("rpc1", "user", start_time);
  usage_map.RecordUse("rpc2", "user", start_time + absl::Seconds(1));
  usage_map.RecordUse("rpc3", "hacker", start_time + absl::Seconds(2));
  usage_map.RecordUse("rpc4", "friend", start_time + absl::Seconds(3));
  usage_map.RecordUse("rpc2", "user", start_time + absl::Seconds(4));
  usage_map.RecordUse("rpc5", "parent", start_time + absl::Seconds(5));

  // Verify the map has five expected entries and six writes.
  int num_entries = 0;
  usage_map.WithEntries([&](const std::string &operation, const std::string &,
                            const absl::Time &) {
    num_entries += 1;
    EXPECT_THAT(operation, AnyOf("rpc1", "rpc2", "rpc3", "rpc4", "rpc5"));
  });
  EXPECT_THAT(num_entries, Eq(5));
  EXPECT_THAT(usage_map.GetMostRecentTimestamp(),
              Eq(start_time + absl::Seconds(5)));

  // Now write out the file. This should trim off all of the entries but the
  // last three, i.e. everything three seconds or older relative to the last
  // entry at start_time+5s.
  EXPECT_THAT(usage_map.WriteToPersistentStore(), IsOk());
  num_entries = 0;
  usage_map.WithEntries([&](const std::string &operation, const std::string &,
                            const absl::Time &) {
    num_entries += 1;
    EXPECT_THAT(operation, AnyOf("rpc2", "rpc4", "rpc5"));
  });
  EXPECT_THAT(num_entries, Eq(3));
  EXPECT_THAT(usage_map.GetMostRecentTimestamp(),
              Eq(start_time + absl::Seconds(5)));
}

TEST_F(PersistentUsageMapTest, TrimAllEntries) {
  PersistentUsageMap usage_map({
      .persistent_file = fs_.GetTruePath("/last_0s.usage"),
      .trim_entries_older_than = absl::ZeroDuration(),
  });

  // Write out a few records. Space them one second apart.
  absl::Time start_time = absl::Now();
  usage_map.RecordUse("rpc1", "user", start_time);
  usage_map.RecordUse("rpc2", "user", start_time + absl::Seconds(1));
  usage_map.RecordUse("rpc3", "hacker", start_time + absl::Seconds(2));
  usage_map.RecordUse("rpc4", "friend", start_time + absl::Seconds(3));
  usage_map.RecordUse("rpc2", "user", start_time + absl::Seconds(4));
  usage_map.RecordUse("rpc5", "parent", start_time + absl::Seconds(5));

  // Verify the map has five expected entries and six writes.
  int num_entries = 0;
  usage_map.WithEntries([&](const std::string &operation, const std::string &,
                            const absl::Time &) {
    num_entries += 1;
    EXPECT_THAT(operation, AnyOf("rpc1", "rpc2", "rpc3", "rpc4", "rpc5"));
  });
  EXPECT_THAT(num_entries, Eq(5));
  EXPECT_THAT(usage_map.GetMostRecentTimestamp(),
              Eq(start_time + absl::Seconds(5)));

  // Now write out the file. This should trim off everything.
  EXPECT_THAT(usage_map.WriteToPersistentStore(), IsOk());
  usage_map.WithEntries(
      [&](const std::string &, const std::string &, const absl::Time &) {
        ADD_FAILURE() << "the written map should contain nothing";
      });
  EXPECT_THAT(usage_map.GetMostRecentTimestamp(), Eq(absl::InfinitePast()));
}

}  // namespace
}  // namespace ecclesia
