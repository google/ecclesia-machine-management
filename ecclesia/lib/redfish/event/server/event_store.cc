/*
 * Copyright 2023 Google LLC
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

#include "ecclesia/lib/redfish/event/server/event_store.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/btree_set.h"
#include "absl/synchronization/mutex.h"
#include "ecclesia/lib/redfish/event/server/subscription.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {

namespace {

// Stores JSON formatted Redfish Event with associated unique identifier.
// The Redfish Event schema is based on standard defined at
// http://redfish.dmtf.org/schemas/v1/Event.v1_9_0.json with OEM extensions.
struct EventContext {
  EventContext(const EventId &event_id, const nlohmann::json &event)
      : event_id(event_id), event(event) {}

  EventId event_id;
  nlohmann::json event;
};

struct TimestampComparator {
  bool operator()(const EventContext &a, const EventContext &b) const {
    return a.event_id.timestamp < b.event_id.timestamp;
  }
};

class EventStoreImpl : public EventStore {
 public:
  explicit EventStoreImpl(size_t store_size) : store_size_(store_size) {}

  void AddNewEvent(const EventId &event_id,
                   const nlohmann::json &event) override {
    absl::MutexLock lock(&event_contexts_mutex_);
    if (event_contexts_.size() == store_size_) {
      event_contexts_.erase(event_contexts_.begin());
    }

    event_contexts_.insert(EventContext(event_id, event));
  }

  // Retrieves all events that have been added since the given
  // `redfish_event_id` in chronological order based on event timestamp.
  std::vector<nlohmann::json> GetEventsSince(
      std::optional<size_t> redfish_event_id) override {
    absl::MutexLock lock(&event_contexts_mutex_);

    // Return all events when last_event_id is not provided
    std::vector<nlohmann::json> events_to_return = {};
    if (!redfish_event_id.has_value()) {
      for (const auto &context : event_contexts_) {
        events_to_return.push_back(context.event);
      }
      return events_to_return;
    }

    // Stores subscription id of the `redfish_event_id` which will be used to
    // find related events so that subscriber gets only the events it previously
    // subscribed for.
    std::optional<SubscriptionId> subscription_id;

    for (auto it = event_contexts_.begin(); it != event_contexts_.end(); ++it) {
      if (subscription_id.has_value() &&
          it->event_id.subscription_id == subscription_id.value()) {
        events_to_return.push_back(it->event);
      }
      if (it->event_id.redfish_event_id == redfish_event_id.value()) {
        subscription_id = it->event_id.subscription_id;
      }
    }

    return events_to_return;
  }

  nlohmann::json GetEvent(const EventId &event_id) override {
    absl::MutexLock lock(&event_contexts_mutex_);
    for (auto it = event_contexts_.begin(); it != event_contexts_.end(); ++it) {
      if (it->event_id == event_id) {
        return it->event;
      }
    }
    return {};
  }

  std::vector<nlohmann::json> GetEventsBySubscriptionId(
      const size_t subscription_id) override {
    std::vector<nlohmann::json> events_to_return = {};
    absl::MutexLock lock(&event_contexts_mutex_);
    for (auto it = event_contexts_.begin(); it != event_contexts_.end(); ++it) {
      if (it->event_id.subscription_id.Id() == subscription_id) {
        events_to_return.push_back(it->event);
      }
    }
    return events_to_return;
  }

  nlohmann::json ToJSON() override {
    nlohmann::json json;
    nlohmann::json& events_json = json["EventStore"];
    events_json = nlohmann::json::array();
    absl::MutexLock lock(&event_contexts_mutex_);
    for (auto it = event_contexts_.begin(); it != event_contexts_.end(); ++it) {
      nlohmann::json event;
      event["EventId"] = it->event_id.ToJSON();
      event["Event"] = it->event;
      events_json.push_back(event);
    }
    json["NumEvents"] = events_json.size();
    return json;
  }

  // Converts EventStore to string format
  std::string ToString() override { return ToJSON().dump(); }

  // Clear all events
  void Clear() override {
    absl::MutexLock lock(&event_contexts_mutex_);
    event_contexts_.clear();
  }

 private:
  absl::Mutex event_contexts_mutex_;
  absl::btree_multiset<EventContext, TimestampComparator> event_contexts_
      ABSL_GUARDED_BY(event_contexts_mutex_);
  size_t store_size_;
};

}  // namespace

std::unique_ptr<EventStore> CreateEventStore(size_t store_size) {
  return std::make_unique<EventStoreImpl>(store_size);
}

}  // namespace ecclesia
