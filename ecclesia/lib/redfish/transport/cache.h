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

#ifndef ECCLESIA_LIB_REDFISH_TRANSPORT_CACHE_H_
#define ECCLESIA_LIB_REDFISH_TRANSPORT_CACHE_H_

#include <optional>
#include <string>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "ecclesia/lib/http/codes.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/time/clock.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {

// RedfishCachedGetterInterface provides the CachedGet and UncachedGet interface
// methods to transparently allow subclasses to implement cache implementations.
class RedfishCachedGetterInterface {
 public:
  struct GetResult {
    // Result of the Redfish GET request.
    absl::StatusOr<RedfishTransport::Result> result;
    // True if the result was fetched from a live service. False if the result
    // was fetched from a cache.
    bool is_fresh;
  };
  RedfishCachedGetterInterface() {}
  virtual ~RedfishCachedGetterInterface() {}
  // Returns a result from a GET to a path. May return a cached result depending
  // on an implementation-specific cache policy. May also cause a cache to be
  // updated.
  virtual GetResult CachedGet(absl::string_view path) = 0;
  // Returns a result from a GET to a path. The result should be fetched
  // from a live service and GetResult.is_fresh should be set to true.
  virtual GetResult UncachedGet(absl::string_view path) = 0;
};

// No cache policy; there is no cache and CachedGet is equivalent to
// UncachedGet.
class NullCache : public RedfishCachedGetterInterface {
 public:
  NullCache(RedfishTransport *transport) : transport_(transport) {}
  GetResult CachedGet(absl::string_view path) override;
  GetResult UncachedGet(absl::string_view path) override;

 private:
  RedfishTransport *transport_;
};

// Time-based cache policy. A cached entry will be returned as long as it was
// last fetched within a max_age_ window.
class TimeBasedCache : public RedfishCachedGetterInterface {
 public:
  TimeBasedCache(RedfishTransport *transport, Clock *clock,
                 absl::Duration max_age)
      : transport_(transport), clock_(clock), max_age_(max_age) {}

  GetResult CachedGet(absl::string_view path) override;
  GetResult UncachedGet(absl::string_view path) override;

 private:
  GetResult DoUncachedGet(absl::string_view path)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(cache_lock_);

  struct CacheEntry {
    absl::Time insert_time;
    absl::StatusOr<RedfishTransport::Result> data;
  };
  RedfishTransport *transport_;
  const Clock *clock_;
  const absl::Duration max_age_;
  absl::Mutex cache_lock_;
  absl::flat_hash_map<std::string, CacheEntry> cache_
      ABSL_GUARDED_BY(cache_lock_);
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_TRANSPORT_CACHE_H_
