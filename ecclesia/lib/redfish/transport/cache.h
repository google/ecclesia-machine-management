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
#include "ecclesia/lib/complexity_tracker/complexity_tracker.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/time/clock.h"

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

  // Manager can be nullptr when used outside of mmanager
  RedfishCachedGetterInterface(
      std::optional<const ApiComplexityContextManager *> manager)
      : manager_(manager) {
    ecclesia::Check(!manager.has_value() || (*manager) != nullptr,
                    "Complexity manager is nullptr");
  }
  virtual ~RedfishCachedGetterInterface() {}

  // Returns a result from a GET to a path. May return a cached result depending
  // on an implementation-specific cache policy. May also cause a cache to be
  // updated.
  GetResult CachedGet(absl::string_view path) {
    if (manager_.has_value()) {
      (*manager_)->RecordDownstreamCall(
          ApiComplexityContext::CallType::kCachedRedfish);
    }
    return CachedGetInternal(path);
  }

  // Returns a result from a GET to a path. The result should be fetched
  // from a live service and GetResult.is_fresh should be set to true.
  GetResult UncachedGet(absl::string_view path) {
    if (manager_.has_value()) {
      (*manager_)->RecordDownstreamCall(
          ApiComplexityContext::CallType::kUncachedRedfish);
    }
    return UncachedGetInternal(path);
  }

 protected:
  // Methods implement the cache specific logic for CachedGet and UncachedGet
  // public methods
  virtual GetResult CachedGetInternal(absl::string_view path) = 0;
  virtual GetResult UncachedGetInternal(absl::string_view path) = 0;

 private:
  std::optional<const ApiComplexityContextManager *> manager_;
};

// No cache policy; there is no cache and CachedGet is equivalent to
// UncachedGet.
class NullCache : public RedfishCachedGetterInterface {
 public:
  explicit NullCache(RedfishTransport *transport)
      : NullCache(transport, std::nullopt) {}

  NullCache(RedfishTransport *transport,
            std::optional<const ApiComplexityContextManager *> manager)
      : RedfishCachedGetterInterface(manager), transport_(transport) {}

 protected:
  GetResult CachedGetInternal(absl::string_view path) override;
  GetResult UncachedGetInternal(absl::string_view path) override;

 private:
  RedfishTransport *transport_;
};

// Time-based cache policy. A cached entry will be returned as long as it was
// last fetched within a max_age_ window.
class TimeBasedCache : public RedfishCachedGetterInterface {
 public:
  TimeBasedCache(
      RedfishTransport *transport, Clock *clock, absl::Duration max_age,
      std::optional<const ApiComplexityContextManager *> manager = std::nullopt)
      : RedfishCachedGetterInterface(manager),
        transport_(transport),
        clock_(clock),
        max_age_(max_age) {}

 protected:
  GetResult CachedGetInternal(absl::string_view path) override;
  GetResult UncachedGetInternal(absl::string_view path) override;

 private:
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
