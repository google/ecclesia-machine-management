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

#include "ecclesia/lib/redfish/transport/cache.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "ecclesia/lib/redfish/timing/query_timeout_manager.h"

namespace ecclesia {

RedfishCachedGetterInterface::OperationResult NullCache::CachedGetInternal(
    absl::string_view path,
    std::optional<ecclesia::QueryTimeoutManager *> timeout_mgr) {
  // Report uncached call as this is null cache
  return {.result =
              timeout_mgr.has_value()
                  ? transport_->Get(path, (*timeout_mgr)->GetRemainingTimeout())
                  : transport_->Get(path),
          .is_fresh = true};
}

RedfishCachedGetterInterface::OperationResult NullCache::UncachedGetInternal(
    absl::string_view path,
    RedfishCachedGetterInterface::Relevance /* relevance */,
    std::optional<ecclesia::QueryTimeoutManager *> timeout_mgr) {
  return {
      .result =
          timeout_mgr.has_value()
              ? transport_->Get(path, (*timeout_mgr)->GetRemainingTimeout())
              : transport_->Get(path),
      .is_fresh = true,
  };
}

RedfishCachedGetterInterface::OperationResult NullCache::CachedPostInternal(
    absl::string_view path, absl::string_view post_payload,
    absl::Duration duration) {
  return {.result = transport_->Post(path, post_payload), .is_fresh = true};
}

TimeBasedCache::CacheNode &TimeBasedCache::RetrieveCacheNode(
    absl::string_view path) {
  absl::MutexLock mu(&get_cache_lock_);
  auto val = get_cache_.find(path);
  if (val != get_cache_.end()) {
    return *val->second;
  }
  auto map_return = get_cache_.insert(
      std::make_pair(std::string(path),
                     std::make_unique<CacheNode>(std::string(path), transport_,
                                                 *clock_, get_max_age_)));
  return *map_return.first->second;
}

TimeBasedCache::CacheNode &TimeBasedCache::RetrieveCacheNode(
    absl::string_view path, absl::string_view post_payload,
    absl::Duration duration) {
  absl::MutexLock mu(&post_cache_lock_);
  auto key = std::make_pair(std::string(path), std::string(post_payload));
  auto val = post_cache_.find(key);
  if (val != post_cache_.end()) {
    return *val->second;
  }
  auto map_return = post_cache_.insert(std::make_pair(
      std::move(key),
      std::make_unique<CacheNode>(std::string(path), std::string(post_payload),
                                  transport_, *clock_, duration)));
  return *map_return.first->second;
}

RedfishCachedGetterInterface::OperationResult TimeBasedCache::CachedGetInternal(
    absl::string_view path,
    std::optional<ecclesia::QueryTimeoutManager *> timeout_mgr) {
  TimeBasedCache::CacheNode &store = RetrieveCacheNode(path);
  auto result = store.CachedRead(timeout_mgr);
  return {.result = std::move(result.result), .is_fresh = result.is_fresh};
}

RedfishCachedGetterInterface::OperationResult
TimeBasedCache::UncachedGetInternal(
    absl::string_view path, RedfishCachedGetterInterface::Relevance relevance,
    std::optional<ecclesia::QueryTimeoutManager *> timeout_mgr) {
  // Bypass cache node retrieval if caller has indicated cache irrelevance.
  if (relevance == RedfishCachedGetterInterface::Relevance::kNotRelevant) {
    return {.result = timeout_mgr.has_value()
                          ? transport_->Get(
                                path, (*timeout_mgr)->GetRemainingTimeout())
                          : transport_->Get(path),
            .is_fresh = true};
  }
  TimeBasedCache::CacheNode &store = RetrieveCacheNode(path);
  auto result = store.UncachedRead(timeout_mgr);
  return {.result = std::move(result.result), .is_fresh = result.is_fresh};
}

RedfishCachedGetterInterface::OperationResult
TimeBasedCache::CachedPostInternal(absl::string_view path,
                                   absl::string_view post_payload,
                                   absl::Duration duration) {
  TimeBasedCache::CacheNode &store =
      RetrieveCacheNode(path, post_payload, duration);
  auto result = store.CachedRead();
  return {.result = std::move(result.result), .is_fresh = result.is_fresh};
}

}  // namespace ecclesia
