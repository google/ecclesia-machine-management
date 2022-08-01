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

// This library provides a generic map structure for tracking "usage", in the
// "when was this operation most recently used?" sense. It defines a map of the
// form (operation, user) -> (timestamp) where "operation" and "user" are simple
// strings that it us up to the client to define. The library was designed for
// the purpose of tracking RPC usage, but this is not actually mandated or
// depended on by the library itself.
//
//
// The major benefit this library provides is support for persistenting the map
// out to persistent storage, as well as of course loading from said persisted
// maps. This makes the maps useful for services/daemons which need to track
// this information in a way that survives process restarts.
//
// Note that this library is intended to be used by daemons which are senstive
// to the amount of persistent I/O which is generated. So rather than
// implementing the persistence by transparently writing all updates
// automatically, it instead provides several knobs to allow the client code to
// control when writes happen.

#ifndef ECCLESIA_LIB_USAGE_MAP_H_
#define ECCLESIA_LIB_USAGE_MAP_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

namespace ecclesia {

class PersistentUsageMap {
 public:
  struct Options {
    // Full path to the file where usage should be persisted. The initial data
    // will also be loaded from this file, if it exists and is valid.
    //
    // The usage map will also use an intermediate temporary file for writes,
    // with the path of the temporary file being this path plus ".tmp". For
    // persistence to work, both of these paths must be writable.
    std::string persistent_file;
    // Set a duration where updating a key older than this will trigger an
    // automatic write out of the map. If this is infinite duration then
    // automatic writes will never trigger. If this is the zero duration (or
    // negative) then every write will trigger a write.
    //
    // Note that the "age" is calculated using the age relative to the time of
    // most recent entry in the map, not the absolute current time.
    absl::Duration auto_write_on_older_than = absl::InfiniteDuration();
    // Set a duration where entries in the map that are older than this will be
    // expired out. If this is infinite duration than writes will never expire.
    //
    // Like with the auto-write parameter, the age of the entries is determined
    // relative to the newest entry in the map, not the absolute time. This
    // trimming is also only done when the map is being written out.
    absl::Duration trim_entries_older_than = absl::InfiniteDuration();
    // Define a maximum size of the persistent map when it is serialized out to
    // a protocol buffer. If set, then the map will be trimmed to under this
    // size by evicting entries starting with the oldest first.
    //
    // This trimming only happens when the map is being written out. This means
    // that if you never write the usage map out to disk then the internal map
    // can still grow without bound.
    //
    // Be aware that this value does not correspond to a precise maximum size
    // limit for the underlying file. The file format imposes some fixed
    // additional overhead on top of the serialized proto, and so the total
    // persisted bytes can exceed this by some small, fixed amount.
    std::optional<size_t> maximum_proto_size;
  };

  // Returns `Options` instance with default settings.
  static Options DefaultOptions();

  // The type usage key map type. This is just an (operation, user) pair with
  // a comparison and hashing function so that we can use it as the map key.
  struct OperationUser {
    std::string operation;
    std::string user;
  };
  friend bool operator==(const OperationUser &lhs, const OperationUser &rhs) {
    return std::tie(lhs.operation, lhs.user) ==
           std::tie(rhs.operation, rhs.user);
  }
  friend bool operator!=(const OperationUser &lhs, const OperationUser &rhs) {
    return !(lhs == rhs);
  }
  template <typename H>
  friend H AbslHashValue(H h, const OperationUser &op_user) {
    return H::combine(std::move(h), op_user.operation, op_user.user);
  }

  // Construct a new persistent usage map using the given options. This will
  // initialize the map from persistent file(s) specified by the options.
  explicit PersistentUsageMap(Options options);

  // Because the map is associated with a persistent store which is not
  // something that can be copied, the map itself cannot be copyable either.
  PersistentUsageMap(const PersistentUsageMap &) = delete;
  PersistentUsageMap &operator=(const PersistentUsageMap &) = delete;

  // Fetch statistics on the usage map. These statistics do not concern the
  // contents of the map itself but instead track the behavior of this class.
  struct Stats {
    // Counters tracking how many times the map has been written out to the
    // persistent store.
    int32_t total_writes = 0;      // All writes, manual and automatic.
    int32_t automatic_writes = 0;  // Automatic writes only.
    int32_t failed_writes = 0;     // All writes which failed.
    // The size of the most recent persisted proto. This will be set to the size
    // of the loaded proto on construction, and updated after writes. Note that
    // this does not correspond to the precise size of the underlying file.
    size_t proto_size = 0;
  };
  Stats GetStats() const ABSL_LOCKS_EXCLUDED(mutex_);

  // Call a given function with an (operation, user, timestamp) triple for every
  // entry in the usage map. The caller should not expect the calls to happen in
  // in particular order.
  //
  // NOTE: for thread safety, while WithEntries is executing access to the map
  // (and thus all record writes) will be blocked. Therefore you should avoid
  // doing any blocking or very expensive operations in the given function, and
  // you must absolutely not try to write to the usage map. If you must do a
  // blocking or long-running operation you should use WithEntries to save a
  // snapshot of the map and use that snapshot for your operation instead.
  template <typename F>
  void WithEntries(F callback) const ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock ml(&mutex_);
    for (const auto &[key, value] : in_memory_map_) {
      callback(key.operation, key.user, value);
    }
  }

  // Report the most recent timestamp stored in the map. Every entry in the map
  // will have a value <= this value, and at least one entry (if any exist) will
  // have a value equal to it. If the map is empty this will be InfinitePast.
  absl::Time GetMostRecentTimestamp() const ABSL_LOCKS_EXCLUDED(mutex_);

  // Record a new entry in the usage map. By default the timestamp of the call
  // will be presumed to be "now" but if the caller has a more accurate one it
  // can be explicitly passed in.
  //
  // This can automatically trigger writes to the persistent store, depending on
  // the auto-write policy.
  void RecordUse(std::string operation, std::string user,
                 absl::Time timestamp = absl::Now())
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Record multiple new entries in the usae map. This works the same as if you
  // were to call RecordUse in a loop, but it will only acquire the map lock a
  // single time and do at most a single write to the persistent store.
  void RecordUses(std::vector<OperationUser> uses,
                  absl::Time timestamp = absl::Now())
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Flush the current contents of the map out to the persistent store. Returns
  // a not-OK status if the store failed for some reason.
  absl::Status WriteToPersistentStore() ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  // Helper that will update a single record, either inserting a new entry into
  // the map or updating an existing one. You already need to have assembled the
  // key and value, it just does the insert-or-update check.
  //
  // This function will return the age of the existing entry relative to the
  // newest entry in the map. It will be zero if the timestamp is older than the
  // existing entry; it will be infinite if this is a new entry.
  absl::Duration InsertOrUpdateMapEntry(OperationUser op_user,
                                        absl::Time timestamp)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Helper that will merge the contents of the persistent file into the
  // in-memory map. Returns a not-OK status if the load failed for some reason;
  // the in-memory map will not be modified in that case.
  absl::Status MergeFromPersistentStore() ABSL_LOCKS_EXCLUDED(mutex_);

  // Helper that will serialize the in memory map, along with doing any trimming
  // necessary to reduce the map size to be under the maximum proto size.
  std::string SerializeAndTrimMap() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Implementation of WriteToPersistentStore which expects the locks to already
  // be held. For use by internal code already holding the mutex.
  absl::Status WriteToPersistentStoreUnlocked()
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // The name of the files the usage map is persisted to.
  std::string persistent_file_;
  std::string temporary_file_;

  // Policy flags controlling the behavior of the map.
  absl::Duration auto_write_on_older_than_;
  absl::Duration trim_entries_older_than_;
  std::optional<size_t> maximum_proto_size_;

  // The underlying timestamp map, used in memory.
  mutable absl::Mutex mutex_;
  absl::flat_hash_map<OperationUser, absl::Time> in_memory_map_
      ABSL_GUARDED_BY(mutex_);

  // The newest timestamp currently in the map. This will be infinite past if
  // the map is empty. For logic which is calculating how "old" an entry is this
  // is the value which age is calculated relative to.
  absl::Time newest_timestamp_in_map_ ABSL_GUARDED_BY(mutex_);

  // Store all of the stats being tracked.
  Stats stats_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_USAGE_MAP_H_
