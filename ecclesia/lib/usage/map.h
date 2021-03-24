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

#include <cstdint>
#include <string>
#include <tuple>
#include <utility>

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
    std::string persistent_file;
    // Set a duration where updating a key older than this will trigger an
    // automatic write out of the map. If this is infinite duration then
    // automatic writes will never trigger. If this is the zero duration (or
    // negative) then every write will trigger a write.
    //
    // Note that the "age" is calculated using the age relative to the time of
    // the use being recorded, not the current time. So if this value is 30 days
    // and you have an entry which is 40 days old, trying to record a usage from
    // 20 days ago will _not_ trigger a write. In general times should not be
    // that heavily skewed.
    absl::Duration auto_write_on_older_than = absl::InfiniteDuration();
  };

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

  // Record a new entry in the usage map. By default the timestamp of the call
  // will be presumed to be "now" but if the caller has a more accurate one it
  // can be explicitly passed in.
  //
  // This can automatically trigger writes to the persistent store, depending on
  // the auto-write policy.
  void RecordUse(std::string operation, std::string user,
                 absl::Time timestamp = absl::Now())
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Flush the current contents of the map out to the persistent store. Returns
  // a not-OK status if the store failed for some reason.
  absl::Status WriteToPersistentStore() ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  // The type used as the map key. This is just an (operation, user) pair with
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

  // Helper that will update a single record, either inserting a new entry into
  // the map or updating an existing one. You already need to have assembled the
  // key and value, it just does the insert-or-update check.
  //
  // This function will return the age of the existing entry relative to the
  // given timestamp. It will be zero or negative if the timestamp is older than
  // the existing entry; it will be infinite if this is a new entry.
  absl::Duration InsertOrUpdateMapEntry(OperationUser op_user,
                                        absl::Time timestamp)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Helper that will merge the contents of the persistent file into the
  // in-memory map. Returns a not-OK status if the load failed for some reason;
  // the in-memory map will not be modified in that case.
  absl::Status MergeFromPersistentStore() ABSL_LOCKS_EXCLUDED(mutex_);

  // Implementation of WriteToPersistentStore which expects the locks to already
  // be held. For use by internal code already holding the mutex.
  absl::Status WriteToPersistentStoreUnlocked()
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // The name of the file the usage map is persisted to.
  std::string persistent_file_;

  // Policy flags controlling the behavior of the map.
  absl::Duration auto_write_on_older_than_;

  // The underlying timestamp map, used in memory.
  mutable absl::Mutex mutex_;
  absl::flat_hash_map<OperationUser, absl::Time> in_memory_map_
      ABSL_GUARDED_BY(mutex_);

  // Store all of the stats being tracked.
  Stats stats_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_USAGE_MAP_H_
