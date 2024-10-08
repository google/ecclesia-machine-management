/*
 * Copyright 2024 Google LLC
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

#include "ecclesia/lib/redfish/event/server/subscription_store_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "ecclesia/lib/redfish/event/server/subscription.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {

namespace {

// Concrete implementation of SubscriptionStore.
// Thread safe
class SubscriptionStoreImpl : public SubscriptionStore {
 public:
  // Adds a new subscription with the given subscription ID and event source IDs
  absl::Status AddNewSubscription(
      std::unique_ptr<SubscriptionContext> subscription_context) override {
    if (subscription_context == nullptr) {
      return absl::InvalidArgumentError("Subscription context is null.");
    }
    if (subscription_context->subscription_id.Id() <= 0) {
      return absl::InvalidArgumentError("Invalid Id, must be >0");
    }
    if (subscription_context->event_source_to_uri.empty()) {
      return absl::InvalidArgumentError("Event source to uri is empty.");
    }

    SubscriptionId subscription_id = subscription_context->subscription_id;
    const SubscriptionContext* subscription_context_raw_ptr =
        subscription_context.get();

    // Insert a subscription id if an equivalent key does not
    // already exist within the map.
    {
      absl::MutexLock lock(&subscriptions_mutex_);
      auto [it, inserted] = subscriptions_.insert(
          {subscription_id, std::move(subscription_context)});

      // subscription_context cannot be used past this point.
      if (!inserted) {
        return absl::AlreadyExistsError(absl::StrCat(
            "Subscription Id ", subscription_id.Id(), " already exists."));
      }
    }

    std::vector<EventSourceId> event_source_ids_to_add;
    for (const auto& [event_source_id, value] :
         subscription_context_raw_ptr->event_source_to_uri) {
      event_source_ids_to_add.push_back(event_source_id);
      absl::MutexLock lock(&subscriptions_by_event_sources_mutex_);
      auto iter = subscriptions_by_event_sources_.find(event_source_id);
      if (iter == subscriptions_by_event_sources_.end()) {
        subscriptions_by_event_sources_.insert(
            {event_source_id, {subscription_context_raw_ptr}});
      } else {
        iter->second.push_back(subscription_context_raw_ptr);
      }
    }

    {
      absl::MutexLock lock(&event_sources_by_subscription_mutex_);
      event_sources_by_subscription_.insert(
          {subscription_context_raw_ptr->subscription_id,
           std::move(event_source_ids_to_add)});
    }
    return absl::OkStatus();
  }

  // Deletes the subscription with the given subscription ID
  void DeleteSubscription(const SubscriptionId& subscription_id) override {
    {
      absl::MutexLock lock(&event_sources_by_subscription_mutex_);
      auto it = event_sources_by_subscription_.find(subscription_id);
      if (it != event_sources_by_subscription_.end()) {
        std::vector<EventSourceId>& event_source_ids = it->second;
        for (auto& event_source_id : event_source_ids) {
          DeleteSubscriptionFromEventSources(event_source_id, subscription_id);
        }
        event_sources_by_subscription_.erase(subscription_id);
      }
    }
    // Must be the last one to cleanup. The Other container has raw pointers to
    // the uniqe_ptrs in this container.
    absl::MutexLock lock(&subscriptions_mutex_);
    subscriptions_.erase(subscription_id);
  }

  absl::StatusOr<const SubscriptionContext*> GetSubscription(
      const SubscriptionId& subscription_id) override {
    absl::MutexLock lock(&subscriptions_mutex_);
    auto it = subscriptions_.find(subscription_id);
    if (it == subscriptions_.end()) {
      return absl::NotFoundError(absl::StrCat(
          "Subscription with ID ", subscription_id.Id(), " not found."));
    }
    return it->second.get();
  }

  // Retrieves the subscriptions associated with the given event source ID
  absl::StatusOr<absl::Span<const ecclesia::SubscriptionContext* const>>
  GetSubscriptionsByEventSourceId(const EventSourceId& source_id) override {
    absl::MutexLock lock(&subscriptions_by_event_sources_mutex_);
    auto it = subscriptions_by_event_sources_.find(source_id);
    if (it == subscriptions_by_event_sources_.end()) {
      return absl::NotFoundError(absl::StrCat(
          "Event source with ID ", source_id.ToString(), " not found."));
    }
    return it->second;
  }

  // Converts SubscriptionStore to JSON format
  nlohmann::json ToJSON() override {
    nlohmann::json json;
    nlohmann::json& subscriptions_json = json["Subscriptions"];
    subscriptions_json = nlohmann::json::array();
    absl::MutexLock lock(&subscriptions_mutex_);
    for (const auto& key_value_pair : subscriptions_) {
      nlohmann::json subscription_json;
      subscription_json["SubscriptionId"] = key_value_pair.first.Id();
      nlohmann::json& uri_json = subscription_json["URI"];
      uri_json = nlohmann::json::array();
      for (const auto& [event_source_id, set_of_uris] :
          key_value_pair.second->event_source_to_uri) {
        for (const auto& uri : set_of_uris) {
          uri_json.push_back(uri);
        }
      }
      nlohmann::json& trigger_json = subscription_json["Triggers"];
      trigger_json = nlohmann::json::array();
      for (const auto& [trigger_id, trigger] :
          key_value_pair.second->id_to_triggers) {
          trigger_json.push_back(trigger.ToJSON());
      }
      subscriptions_json.push_back(subscription_json);
    }
    json["NumSubscriptions"] = subscriptions_json.size();
    return json;
  }

  // Converts SubscriptionStore to string format
  std::string ToString() override { return ToJSON().dump(); }

 private:
  void DeleteSubscriptionFromEventSources(
      const EventSourceId& event_source_id,
      const SubscriptionId& subscription_id) {
    absl::MutexLock lock(&subscriptions_by_event_sources_mutex_);
    std::vector<const SubscriptionContext*>& subscription_contexts =
        subscriptions_by_event_sources_[event_source_id];
    for (auto iter = subscription_contexts.begin();
         iter != subscription_contexts.end(); ++iter) {
      if ((*iter)->subscription_id != subscription_id) {
        continue;
      }
      subscription_contexts.erase(iter);
      if (subscription_contexts.empty()) {
        subscriptions_by_event_sources_.erase(event_source_id);
      }
      return;
    }
  }

  absl::Mutex subscriptions_mutex_;
  // pointer-stability of values (but not keys) is needed
  absl::flat_hash_map<SubscriptionId, std::unique_ptr<SubscriptionContext>>
      subscriptions_ ABSL_GUARDED_BY(subscriptions_mutex_);

  absl::Mutex subscriptions_by_event_sources_mutex_;
  absl::flat_hash_map<EventSourceId, std::vector<const SubscriptionContext*>>
      subscriptions_by_event_sources_
          ABSL_GUARDED_BY(subscriptions_by_event_sources_mutex_);

  absl::Mutex event_sources_by_subscription_mutex_;
  absl::flat_hash_map<SubscriptionId, std::vector<EventSourceId>>
      event_sources_by_subscription_
          ABSL_GUARDED_BY(event_sources_by_subscription_mutex_);
};

}  // namespace

std::unique_ptr<SubscriptionStore> CreateSubscriptionStore() {
  return std::make_unique<SubscriptionStoreImpl>();
}

}  // namespace ecclesia
