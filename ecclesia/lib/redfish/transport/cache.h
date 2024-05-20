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
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "ecclesia/lib/complexity_tracker/complexity_tracker.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/time/clock.h"

namespace ecclesia {

// RedfishCachedGetterInterface provides the CachedGet and UncachedGet interface
// methods to transparently allow subclasses to implement cache implementations.
class RedfishCachedGetterInterface {
 public:
  enum class Relevance : uint8_t { kRelevant, kNotRelevant};

  struct OperationResult {
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
  OperationResult CachedGet(
      absl::string_view path,
      std::optional<absl::Duration> timeout = std::nullopt) {
    if (manager_.has_value()) {
      (*manager_)->RecordDownstreamCall(
          ApiComplexityContext::CallType::kCachedRedfish);
    }
    return CachedGetInternal(path, timeout);
  }

  // Returns a result from a GET to a path. The result should be fetched
  // from a live service and GetResult.is_fresh should be set to true.
  OperationResult UncachedGet(
      absl::string_view path,
      RedfishCachedGetterInterface::Relevance relevance =
          RedfishCachedGetterInterface::Relevance::kRelevant,
      std::optional<absl::Duration> timeout = std::nullopt) {
    if (manager_.has_value()) {
      (*manager_)->RecordDownstreamCall(
          ApiComplexityContext::CallType::kUncachedRedfish);
    }
    return UncachedGetInternal(path, relevance, timeout);
  }

  // Returns a result from a POST to a path and a payload. May return a cached
  // result depending on an implementation-specific cache policy. May also cause
  // a cache to be updated.
  OperationResult CachedPost(absl::string_view path, absl::string_view payload,
                       absl::Duration duration) {
    if (manager_.has_value()) {
      (*manager_)->RecordDownstreamCall(
          ApiComplexityContext::CallType::kCachedRedfish);
    }
    return CachedPostInternal(path, payload, duration);
  }

 protected:
  // Methods implement the cache specific logic for CachedGet and UncachedGet
  // public methods
  virtual OperationResult CachedGetInternal(
      absl::string_view path,
      std::optional<absl::Duration> timeout = std::nullopt) = 0;
  virtual OperationResult UncachedGetInternal(
      absl::string_view path,
      RedfishCachedGetterInterface::Relevance relevance =
          RedfishCachedGetterInterface::Relevance::kRelevant,
      std::optional<absl::Duration> timeout = std::nullopt) = 0;
  virtual OperationResult CachedPostInternal(absl::string_view path,
                                       absl::string_view payload,
                                       absl::Duration duration) = 0;

 private:
  std::optional<const ApiComplexityContextManager *> manager_;
};

// No cache policy; there is no cache and CachedGet is equivalent to
// UncachedGet.
class NullCache : public RedfishCachedGetterInterface {
 public:
  static std::unique_ptr<RedfishCachedGetterInterface> Create(
      RedfishTransport *transport) {
    return std::make_unique<NullCache>(transport, std::nullopt);
  }

  explicit NullCache(RedfishTransport *transport)
      : NullCache(transport, std::nullopt) {}

  NullCache(RedfishTransport *transport,
            std::optional<const ApiComplexityContextManager *> manager)
      : RedfishCachedGetterInterface(manager), transport_(transport) {}

 protected:
  OperationResult CachedGetInternal(
      absl::string_view path,
      std::optional<absl::Duration> timeout = std::nullopt) override;
  OperationResult UncachedGetInternal(
      absl::string_view path,
      RedfishCachedGetterInterface::Relevance relevance =
          RedfishCachedGetterInterface::Relevance::kNotRelevant,
      std::optional<absl::Duration> timeout = std::nullopt) override;
  OperationResult CachedPostInternal(absl::string_view path,
                               absl::string_view payload,
                               absl::Duration duration) override;

 private:
  RedfishTransport *transport_;
};

// Time-based cache policy. A cached entry will be returned as long as it was
// last fetched within a max_age_ window.
class TimeBasedCache : public RedfishCachedGetterInterface {
 public:
  static std::unique_ptr<RedfishCachedGetterInterface> Create(
      RedfishTransport *transport, absl::Duration max_age,
      const Clock *clock = Clock::RealClock()) {
    return std::make_unique<TimeBasedCache>(transport, clock, max_age);
  }

  TimeBasedCache(
      RedfishTransport *transport, const Clock *clock, absl::Duration max_age,
      std::optional<const ApiComplexityContextManager *> manager = std::nullopt)
      : RedfishCachedGetterInterface(manager),
        transport_(transport),
        clock_(clock),
        get_max_age_(max_age) {}

 protected:
  OperationResult CachedGetInternal(
      absl::string_view path,
      std::optional<absl::Duration> timeout = std::nullopt) override;
  OperationResult UncachedGetInternal(
      absl::string_view path,
      RedfishCachedGetterInterface::Relevance relevance =
          RedfishCachedGetterInterface::Relevance::kRelevant,
      std::optional<absl::Duration> timeout = std::nullopt) override;
  OperationResult CachedPostInternal(absl::string_view path,
                               absl::string_view payload,
                               absl::Duration duration) override;

 private:
  // Each CacheNode owns its own result cache and previous update timestamp.
  class CacheNode {
   public:
    CacheNode(std::string path, RedfishTransport *transport, const Clock &clock,
              const absl::Duration duration)
        : CacheNode(std::move(path), std::nullopt, transport, clock, duration) {
    }
    CacheNode(std::string path, std::optional<std::string> post_payload,
              RedfishTransport *transport, const Clock &clock,
              const absl::Duration duration)
        : path_(std::move(path)),
          post_payload_(std::move(post_payload)),
          transport_(transport),
          clock_(&clock),
          duration_(duration) {}

    struct ResultAndFreshness {
      absl::StatusOr<RedfishTransport::Result> result;
      bool is_fresh;
    };

    ResultAndFreshness CachedRead(
        std::optional<absl::Duration> timeout = std::nullopt) {
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
      if (!local_notification) return DoUpdateAndNotifyOthers(timeout);
      // Otherwise another thread is responsible for doing the update.
      return WaitForNotificationAndUseCachedResult(*local_notification);
    }

    ResultAndFreshness UncachedRead(
        std::optional<absl::Duration> timeout = std::nullopt) {
      std::unique_ptr<absl::Notification> local_notification = nullptr;
      {
        absl::MutexLock mu(&mutex_);
        local_notification = RegisterNotificationIfUpdateInProgress();
      }
      // No notification:
      // This thread is the one responsible for doing the update.
      if (!local_notification) return DoUpdateAndNotifyOthers(timeout);
      // Otherwise another thread is responsible for doing the update.
      return WaitForNotificationAndUseCachedResult(*local_notification);
    }

   private:
    std::unique_ptr<absl::Notification> RegisterNotificationIfUpdateInProgress()
        ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
      if (!operation_in_progress_) {
        operation_in_progress_ = true;
        return nullptr;
      }
      // Someone is in the middle of doing an update.
      // Add ourselves to the notification vector.
      auto notification = std::make_unique<absl::Notification>();
      notifications_.push_back(notification.get());
      return notification;
    }

    ResultAndFreshness DoUpdateAndNotifyOthers(
        std::optional<absl::Duration> timeout = std::nullopt)
        ABSL_LOCKS_EXCLUDED(mutex_) {
      // transport_->Get() or Post() might be slow so do not hold any locks
      // here.
      absl::StatusOr<RedfishTransport::Result> result;
      if (post_payload_.has_value()) {
        result = transport_->Post(path_, *post_payload_);
      } else {
        result = timeout.has_value() ? transport_->Get(path_, *timeout)
                                     : transport_->Get(path_);
      }

      // For successful return, if this is Post operation, we cache the result
      // no matter what format the body is, otherwise we only cache it if it's
      // JSON format.
      // However, we still update result_ so that we can batch colliding
      // uncached Gets in case the payload is polled frequently and has a long
      // latency.
      absl::Time update_time = absl::InfinitePast();
      if (result.ok() &&
          (post_payload_.has_value() ||
           std::holds_alternative<nlohmann::json>(result->body))) {
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
        operation_in_progress_ = false;
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
    // The optional POST payload associated with this cache. If this has some
    // value, it means this is cache from POST.
    std::optional<std::string> post_payload_;
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
    bool operation_in_progress_ ABSL_GUARDED_BY(mutex_) = false;
  };

  CacheNode &RetrieveCacheNode(absl::string_view path)
      ABSL_LOCKS_EXCLUDED(get_cache_lock_);
  CacheNode &RetrieveCacheNode(absl::string_view path,
                               absl::string_view post_payload,
                               absl::Duration duration)
      ABSL_LOCKS_EXCLUDED(post_cache_lock_);

  RedfishTransport *transport_;
  const Clock *clock_;
  const absl::Duration get_max_age_;
  absl::Mutex get_cache_lock_;
  absl::flat_hash_map<std::string, std::unique_ptr<CacheNode>> get_cache_
      ABSL_GUARDED_BY(get_cache_lock_);
  absl::Mutex post_cache_lock_;
  absl::flat_hash_map<std::pair<std::string, std::string>,
                      std::unique_ptr<CacheNode>>
      post_cache_ ABSL_GUARDED_BY(post_cache_lock_);
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_TRANSPORT_CACHE_H_
