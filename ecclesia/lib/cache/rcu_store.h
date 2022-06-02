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

// This header provides the store aspect of a generic shared data store that can
// be updated with Read-Copy-Update (RCU) style semantics.
//
// The class template provided by this header is the RcuStore class. This
// provides a standard facility for storing an object of a generic type. It does
// just two things:
//   * a Read operation to get snapshots of the current data
//   * an Update operation to update the stored data
//
// Callers that use Read() will get a copy of the snapshot. This data will be
// considered fresh until the Update function is called; this update will
// invalidate the existing snapshot and then replace the internal store with a
// new one. Subsequent calls to Read() will then get copies of the new snapshot;
// but existing in-use copies of the old snapshot will remain valid.
//
// Classes and APIs that want to expose an RCU interface to their clients should
// usually not expose the RcuStore class directly, as this would allow their
// users to both read AND write the data. When only a read interface is desired,
// the RcuView classes should be export instead.

#ifndef ECCLESIA_LIB_CACHE_RCU_STORE_H_
#define ECCLESIA_LIB_CACHE_RCU_STORE_H_

#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "ecclesia/lib/cache/rcu_snapshot.h"

namespace ecclesia {

template <typename T>
class RcuStore {
 public:
  // Construct a new instance, forwarding the arguments to the T constructor.
  template <typename... Args>
  explicit RcuStore(Args &&...args)
      : data_(RcuSnapshot<T>::Create(std::forward<Args>(args)...)) {}

  // Get a copy of the data for reading from.
  RcuSnapshot<T> Read() const {
    absl::MutexLock ml(&mutex_);
    return data_.snapshot;
  }

  // Update the copy of the data. Note that if there are multiple updaters then
  // this does not guarantee that a Read-Copy-Update cycle is atomic.
  template <typename... Args>
  void Update(Args &&...args) {
    // Create the new snapshot.
    typename RcuSnapshot<T>::WithInvalidator new_data =
        RcuSnapshot<T>::Create(std::forward<Args>(args)...);

    // After locking the underlying store, swap in the new value and then
    // invalidate the old snapshot.
    absl::MutexLock ml(&mutex_);
    data_.invalidator.InvalidateSnapshot();
    std::swap(data_, new_data);
  }

 private:
  mutable absl::Mutex mutex_;
  typename RcuSnapshot<T>::WithInvalidator data_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_CACHE_RCU_STORE_H_
