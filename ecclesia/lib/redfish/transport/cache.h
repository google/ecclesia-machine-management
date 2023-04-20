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

#include <memory>
#include <optional>
#include <string>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "ecclesia/lib/complexity_tracker/complexity_tracker.h"
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
    CHECK(!manager.has_value() || (*manager) != nullptr)
        << "Complexity manager is nullptr";
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
      RedfishTransport *transport, const Clock *clock, absl::Duration max_age,
      std::optional<const ApiComplexityContextManager *> manager = std::nullopt)
      : RedfishCachedGetterInterface(manager),
        transport_(transport),
        clock_(clock),
        max_age_(max_age) {}

 protected:
  GetResult CachedGetInternal(absl::string_view path) override;
  GetResult UncachedGetInternal(absl::string_view path) override;

 private:
  // Each CacheNode owns its own result cache and previous update timestamp.
  class CacheNode {
   public:
    CacheNode(std::string path, RedfishTransport *transport, const Clock &clock,
              const absl::Duration duration)
        : path_(std::move(path)),
          transport_(transport),
          clock_(&clock),
          duration_(duration) {}

    struct ResultAndFreshness {
      absl::StatusOr<RedfishTransport::Result> result;
      bool is_fresh;
    };

    ResultAndFreshness CachedRead() {
      std::unique_ptr<absl::Notification> local_notification = nullptr;
      {
        absl::MutexLock mu(&mutex_);
        // If the cache has a sufficiently recent value, return it.
        if (clock_->Now() < last_update_time_ + duration_) {
          return {result_, false};
        }
        local_notification = RegisterNotificationIfUpdateInProgress();
      }
      // No notification:
      // This thread is the one responsible for doing the update.
      if (!local_notification) return DoUpdateAndNotifyOthers();
      // Otherwise another thread is responsible for doing the update.
      return WaitForNotificationAndUseCachedResult(*local_notification);
    }

    ResultAndFreshness UncachedRead() {
      std::unique_ptr<absl::Notification> local_notification = nullptr;
      {
        absl::MutexLock mu(&mutex_);
        local_notification = RegisterNotificationIfUpdateInProgress();
      }
      // No notification:
      // This thread is the one responsible for doing the update.
      if (!local_notification) return DoUpdateAndNotifyOthers();
      // Otherwise another thread is responsible for doing the update.
      return WaitForNotificationAndUseCachedResult(*local_notification);
    }

   private:
    std::unique_ptr<absl::Notification> RegisterNotificationIfUpdateInProgress()
        ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
      if (!get_in_progress_) {
        get_in_progress_ = true;
        return nullptr;
      }
      // Someone is in the middle of doing an update.
      // Add ourselves to the notification vector.
      auto notification = std::make_unique<absl::Notification>();
      notifications_.push_back(notification.get());
      return notification;
    }

    ResultAndFreshness DoUpdateAndNotifyOthers()
        ABSL_LOCKS_EXCLUDED(mutex_) {
      // transport_->Get() might be slow so do not hold any locks here.
      auto result = transport_->Get(path_);
      // If the result was not JSON, set the update time to InfinitePast() to
      // never cache. However, still update result_ so that we can batch
      // colliding uncached Gets in case the payload is polled frequently and
      // has a long latency.
      absl::Time update_time = absl::InfinitePast();
      if (result.ok() && std::holds_alternative<nlohmann::json>(result->body)) {
        update_time = clock_->Now();
      }

      {
        // Update the result and notify all waiters.
        absl::MutexLock mu(&mutex_);
        result_ = result;
        last_update_time_ = update_time;
        for (auto *notification : notifications_) {
          notification->Notify();
        }
        notifications_.clear();
        get_in_progress_ = false;
        return {std::move(result), true};
      }
    }

    ResultAndFreshness WaitForNotificationAndUseCachedResult(
        absl::Notification &local_notification)
        ABSL_LOCKS_EXCLUDED(mutex_) {
      local_notification.WaitForNotification();
      {
        absl::MutexLock mu(&mutex_);
        return {result_, true};
      }
    }

    // The URI associated with this cache.
    const std::string path_;
    // The RedfishTransport used to update this cache.
    RedfishTransport *transport_;

    // The clock used for timekeeping and the duration before new reads will
    // be made.
    const Clock *clock_;
    absl::Duration duration_;

    // The cached result and read timestamp.
    absl::Mutex mutex_;
    absl::Time last_update_time_ ABSL_GUARDED_BY(mutex_) =
        absl::InfinitePast();
    absl::StatusOr<RedfishTransport::Result> ABSL_GUARDED_BY(
        mutex_) result_;
    // The list of notification objects, each instance representing a thread
    // waiting for an uncached update result.
    std::vector<absl::Notification *> notifications_
        ABSL_GUARDED_BY(mutex_);
    // Set to true if there is an update in progress.
    bool get_in_progress_ ABSL_GUARDED_BY(mutex_) = false;
  };

  CacheNode &GetCacheNode(absl::string_view path);

  RedfishTransport *transport_;
  const Clock *clock_;
  const absl::Duration max_age_;
  absl::Mutex cache_lock_;
  absl::flat_hash_map<std::string, std::unique_ptr<CacheNode>> cache_
      ABSL_GUARDED_BY(cache_lock_);
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_TRANSPORT_CACHE_H_
