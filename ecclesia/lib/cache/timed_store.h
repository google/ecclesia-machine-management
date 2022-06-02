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

// Provides an RcuStore that does automatic time-based invalidation. This is
// useful in cases where don't have a fixed invalidation mechanism and instead
// just want to cache a value for a time interval instead.
//
// You generally only want to use this for relatively short time intervals (i.e.
// times measured in seconds, not minutes). If you need to cache a value for a
// long time you usually want a more robust invalidation mechanism to avoid
// having long periods of time where you have stale data.
//
// Techncially, this library is actually providing an RcuView and not an
// RcuStore. However, conceptually it makes sense to think of this as a RcuStore
// with timed invalidation, where the RcuView is just the mechanism by which you
// access the store.
//
//
// Note that even after the cache duration passes, the underlying snapshot will
// only be invalidated on the next Read() of the store. This means that if you
// grab a snapshot you cannot use an IsFresh() check to determine if the time
// interval has passed.
//
// This also means that in general you should not use this store as a backing
// view for translated views, as the translated view will only be invalidated
// when the timed view is Read(), not when the translated view is Read(). This
// type is not well-suited to chaining RcuViews anyway as chaining on top of a
// view with a very short cache would lead to very little caching. The design is
// optimized for small, internal caches.

#ifndef ECCLESIA_LIB_CACHE_TIMED_STORE_H_
#define ECCLESIA_LIB_CACHE_TIMED_STORE_H_

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "ecclesia/lib/cache/rcu_snapshot.h"
#include "ecclesia/lib/cache/rcu_store.h"
#include "ecclesia/lib/cache/rcu_view.h"
#include "ecclesia/lib/time/clock.h"

namespace ecclesia {

template <typename T>
class TimedRcuStore : public RcuView<T> {
 public:
  // Construct the store. The type T is expected to be default-constructable.
  // Note that the initially constructed T value will never be returned from
  // Read() and so objects with complex initialization are not well suited to
  // this kind of store.
  TimedRcuStore(const Clock &clock, absl::Duration duration)
      : clock_(&clock), duration_(duration) {}

  // Read will check if the store has been updated within the cache interval and
  // return its contents if it has been. Otherwise it will use ReadFresh() to
  // popuate the snapshot with fresh data.
  RcuSnapshot<T> Read() const final {
    absl::MutexLock ml(&cache_.mutex);
    if (clock_->Now() > cache_.last_update_time + duration_) {
      cache_.store.Update(ReadFresh());
      cache_.last_update_time = clock_->Now();
    }
    return cache_.store.Read();
  }

 private:
  // Read a fresh copy of the value T.
  virtual T ReadFresh() const = 0;

  // The clock used for timekeeping and the duration before new reads will
  const Clock *clock_;
  absl::Duration duration_;

  // Internal cache that includes the underlying store as well as the timestamp
  // of when it was last updated.
  mutable struct Cache {
    absl::Mutex mutex;
    absl::Time last_update_time ABSL_GUARDED_BY(mutex) = absl::InfinitePast();
    RcuStore<T> store ABSL_GUARDED_BY(mutex);
  } cache_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_CACHE_TIMED_STORE_H_
