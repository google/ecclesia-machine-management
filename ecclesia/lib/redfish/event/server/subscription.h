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

#ifndef ECCLESIA_LIB_REDFISH_EVENT_SERVER_SUBSCRIPTION_H_
#define ECCLESIA_LIB_REDFISH_EVENT_SERVER_SUBSCRIPTION_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {

// Represents an event source identifier
struct EventSourceId {
  // Enum representing the type of event source
  enum class Type : uint8_t {
    // D-Bus objects as event sources
    kDbusObjects = 0,
    // Socket IO as event sources
    kSocketIO = 1,
    // File IO as event sources
    kFileIO = 2
  };

  EventSourceId(int key_in, Type type_in) : key(key_in), type(type_in) {}

  template <typename H>
  friend H AbslHashValue(H h, const EventSourceId &n) {
    return H::combine(std::move(h), n.key);
  }

  bool operator==(const EventSourceId &other) const {
    return key == other.key && type == other.type;
  }
  bool operator!=(const EventSourceId &other) const {
    return !(*this == other);
  }

  // Converts EventSourceId to JSON format
  nlohmann::json ToJSON() const;

  // Converts EventSourceId to string format
  std::string ToString() const;

  // Unique identifier for the event source
  int key;

  // Type of event source, represented by the Type enum
  Type type;
};

// Uniquely identifies subscription.
struct SubscriptionId {
  explicit SubscriptionId(size_t subscription_id_in)
      : subscription_id(subscription_id_in) {}

  template <typename H>
  friend H AbslHashValue(H h, const SubscriptionId &n) {
    return H::combine(std::move(h), n.subscription_id);
  }

  bool operator==(const SubscriptionId &other) const {
    return subscription_id == other.subscription_id;
  }

  bool operator!=(const SubscriptionId &other) const {
    return !(*this == other);
  }

  size_t subscription_id;
};

// EventId uniquely identifies an event at any given point in time from any
// event source.
//
// EventId satisfies the contract of uniqueness through `redfish_event_id` and
// monotonicity through `timestamp`. `EventId` has an association with
// `SubscriptionId` to allow subscription service to group events for a
// subscriber, useful to satisfy lossless-events contract.
struct EventId {
  EventId(const SubscriptionId &subscription_id_in,
          const EventSourceId &source_id_in, absl::Time timestamp_in);

  // Converts EventId to string format
  std::string ToString() const;

  // Converts EventId to JSON format
  nlohmann::json ToJSON() const;

  template <typename H>
  friend H AbslHashValue(H h, const EventId &n) {
    return H::combine(std::move(h), n.redfish_event_id);
  }

  EventSourceId source_id;
  absl::Time timestamp;
  SubscriptionId subscription_id;
  size_t redfish_event_id;
};

// Structure representing a Trigger, which defines event triggering conditions
struct Trigger {
  using EventSourceToUri =
      absl::flat_hash_map<EventSourceId, std::vector<std::string>>;

  // Static method to create a Trigger object from raw data
  static absl::StatusOr<Trigger> Create(const nlohmann::json &trigger_json);

  // Constructor for Trigger
  explicit Trigger(std::vector<std::string> origin_resources_in,
                   absl::string_view predicate_in = "", bool mask_in = false);

  // Converts Trigger to JSON format
  nlohmann::json ToJSON() const;

  // Converts Trigger to string format
  std::string ToString() const;

  // List of origin resources associated with the Trigger
  std::vector<std::string> origin_resources;

  // Map of event source to redfish resource URI
  EventSourceToUri event_source_to_uri;

  // Predicate expression for determining when to trigger the event
  std::string predicate;

  // Flag indicating whether to mask event source.
  bool mask;
};

struct SubscriptionContext {
  SubscriptionContext(
      const SubscriptionId &subscription_id_in,
      absl::flat_hash_map<std::string, Trigger> id_to_triggers_in,
      std::function<void(const nlohmann::json &)> &&on_event_callback_in);

  SubscriptionId subscription_id;
  // Map of trigger IDs to corresponding Trigger objects
  // This map associates each trigger ID with the corresponding Trigger
  // object, enabling efficient lookup and management of triggers for a given
  // subscription.
  absl::flat_hash_map<std::string, Trigger> id_to_triggers;

  // Event callback to be invoked for events related to this subscription
  // This callback function is defined for each subscription and will be
  // called whenever an event occurs that matches the criteria defined by the
  // subscription's triggers. The callback function receives the event data as
  // its argument.
  std::function<void(const nlohmann::json &)> on_event_callback;
};

// Interface for an event store.
// EventStore stores events in an overwriting circular buffer and allows
// looking up the events queued since a specific event_id to honor the
// lossless eventing contract of subscription service.
//
// The event store shall be thread safe.
class EventStore {
 public:
  virtual ~EventStore() = default;

  // Adds a new event to the overwriting circular buffer.
  virtual void AddNewEvent(const EventId &event_id,
                           const nlohmann::json &event) = 0;

  // Retrieves all events that have been added since (but not including) the
  // given redfish event id in a chronological order determined by
  // `event_id.timestamp`.
  // If `redfish_event_id` is std::nullopt value, all events shall be returned
  // in chronological order.
  virtual std::vector<nlohmann::json> GetEventsSince(
      std::optional<size_t> redfish_event_id) = 0;
};

// Interface for a subscription store
class SubscriptionStore {
 public:
  virtual ~SubscriptionStore() = default;

  // Adds a new subscription with the given subscription ID and event source IDs
  virtual void AddNewSubscription(
      SubscriptionContext &&subscription_context) = 0;

  // Deletes the subscription with the given subscription ID
  virtual void DeleteSubscription(const SubscriptionId &subscription_id) = 0;

  // Retrieves the subscriptions associated with the given event source ID
  virtual absl::StatusOr<absl::Span<const SubscriptionContext>>
  GetSubscriptionsBySourceId(const EventSourceId &source_id) = 0;

  // Converts SubscriptionStore to JSON format
  virtual nlohmann::json ToJSON() const = 0;

  // Converts SubscriptionStore to string format
  virtual std::string ToString() const = 0;
};

// Interface for Redfish handler
// This ensures that SubscriptionService interoperates with different redfish
// backends by standardizing the interface to subscribe and query Redfish
// resources.
class RedfishHandler {
 public:
  // Callback type for Redfish queries
  using QueryCallback =
      std::function<void(const absl::Status & /*Status Code*/,
                         const nlohmann::json & /*Redfish Resource json_str*/)>;

  // Destructor for RedfishHandler
  virtual ~RedfishHandler() = default;

  // Performs a Redfish query at the given URL and invokes the provided callback
  // with the query result
  virtual absl::Status Query(absl::string_view url,
                             QueryCallback query_callback) = 0;

  // Subscribes to Redfish events for the given URL and returns the list of
  // event source IDs
  virtual absl::StatusOr<std::vector<EventSourceId>> Subscribe(
      absl::string_view url) = 0;
};

// Interface for a subscription service
class SubscriptionService {
 public:
  virtual ~SubscriptionService() = default;

  // Creates a new subscription and returns the subscription ID of the newly
  // created subscription.
  virtual absl::StatusOr<SubscriptionId> CreateSubscription(
      const nlohmann::json &request,
      std::function<void(const nlohmann::json &)> &&on_event_callback) = 0;

  // Retrieves all subscriptions managed by the service.
  virtual absl::Span<const SubscriptionContext> GetAllSubscriptions() = 0;

  // Invoked by an EventSource to notify SubscriptionService about an event
  // occurrence.
  // Returns error if notification cannot be processed, typically used to
  // indicate event source that subscription is deleted and the source should
  // disable event listener.
  virtual absl::Status Notify(EventSourceId key, absl::Status status) = 0;

  // Invoked by an EventSource to notify SubscriptionService about an event
  // occurrence along with providing data associated with the event.
  // This method is preferred when the event source is capable of constructing
  // entire OriginOfCondition for event.
  virtual absl::Status NotifyWithData(EventSourceId key, absl::Status status,
                                      const nlohmann::json &data) = 0;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_EVENT_SERVER_SUBSCRIPTION_H_