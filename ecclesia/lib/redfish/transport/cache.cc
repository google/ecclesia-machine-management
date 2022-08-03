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

#include <variant>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/time/clock.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {

RedfishCachedGetterInterface::GetResult NullCache::CachedGetInternal(
    absl::string_view path) {
  // Report uncached call as this is nullcache
  return {.result = transport_->Get(path), .is_fresh = true};
}

RedfishCachedGetterInterface::GetResult NullCache::UncachedGetInternal(
    absl::string_view path) {
  return {.result = transport_->Get(path), .is_fresh = true};
}

RedfishCachedGetterInterface::GetResult TimeBasedCache::CachedGetInternal(
    absl::string_view path) {
  {
    absl::MutexLock mu(&cache_lock_);
    auto val = cache_.find(path);
    if (val != cache_.end() &&
        (clock_->Now() - val->second.insert_time) < max_age_) {
      // Report cached result
      return {.result = val->second.data, .is_fresh = false};
    }
  }
  auto result = transport_->Get(path);
  absl::MutexLock mu(&cache_lock_);
  if (result.ok() && std::holds_alternative<nlohmann::json>(result->body)) {
    cache_[path] = CacheEntry{.insert_time = clock_->Now(), .data = result};
  }
  return {.result = result, .is_fresh = true};
}

RedfishCachedGetterInterface::GetResult TimeBasedCache::UncachedGetInternal(
    absl::string_view path) {
  auto result = transport_->Get(path);
  absl::MutexLock mu(&cache_lock_);
  if (result.ok() && std::holds_alternative<nlohmann::json>(result->body)) {
    cache_[path] = CacheEntry{.insert_time = clock_->Now(), .data = result};
  }
  return {.result = result, .is_fresh = true};
}

}  // namespace ecclesia
