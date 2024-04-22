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

#include "ecclesia/lib/redfish/event/server/subscription_impl.h"

#include <cstddef>
#include <ctime>
#include <functional>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/hash/hash.h"
#include "absl/log/die_if_null.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ecclesia/lib/redfish/event/server/subscription.h"
// #include
// "ecclesia/lib/redfish/redpath/definitions/query_predicates/predicates.h"
#include "ecclesia/lib/status/macros.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {

namespace {

// Redfish properties to parse in Subscription request.
constexpr absl::string_view kPropertyOem = "Oem";
constexpr absl::string_view kPropertyGoogle = "Google";
constexpr absl::string_view kPropertyTriggers = "Triggers";
constexpr absl::string_view kPropertyId = "Id";
constexpr absl::string_view kPropertyLastEventId = "LastEventId";

std::string PropertyNotPopulatedError(absl::string_view property) {
  return absl::StrCat(property, " not populated");
}

// Constructs a RedfishEvent from `query_response`.
void BuildRedfishEvent(const Trigger &trigger,
                       const nlohmann::json &query_response,
                       nlohmann::json &redfish_event) {
  // Generate Event Id
  size_t event_id = absl::HashOf(absl::Now(), query_response.dump());
  redfish_event["EventId"] = absl::StrCat(event_id);

  // Get ISO extended string for timestamp
  redfish_event["EventTimestamp"] =
      absl::FormatTime(absl::RFC3339_full, absl::Now(), absl::UTCTimeZone());

  // Add origin of condition.
  redfish_event["OriginOfCondition"] = query_response;

  // Add trigger id as context.
  redfish_event["Context"] = trigger.id;
}

// Constructs RedfishEvents for redfish resources meeting trigger criteria.
void BuildRedfishEventsForTrigger(
    const Trigger &trigger,
    const absl::flat_hash_map<std::string, nlohmann::json> &uri_to_response,
    std::vector<nlohmann::json> &events) {
  for (const auto &[uri, response] : uri_to_response) {
    // Skip event if URI is not part of the trigger origin resources.
    if (!trigger.origin_resources.contains(uri)) {
      continue;
    }

    // // Apply predicate in trigger to check if event meets the user defined
    // // criteria.
    // if (!trigger.predicate.empty()) {
    //   absl::StatusOr<bool> predicate_result =
    //       ApplyPredicateRule(response, {.predicate = trigger.predicate,
    //                                     .node_index = 0,
    //                                     .node_set_size = 1});
    //   if (!predicate_result.ok()) {
    //     LOG(ERROR) << "Cannot apply trigger predicate rule for URI: " << uri
    //                << " error: " << predicate_result.status();
    //     continue;
    //   }

    //   if (!*predicate_result) {
    //     // Skip event if predicate is not satisfied.
    //     continue;
    //   }
    // }

    // Construct RedfishEvent.
    BuildRedfishEvent(trigger, response, events.emplace_back());
  }
}

// Constructs RedfishEvents for a given subscription.
std::vector<nlohmann::json> BuildRedfishEventsForSubscription(
    const SubscriptionContext &subscription_context,
    const absl::flat_hash_map<std::string, nlohmann::json> &uri_to_response) {
  std::vector<nlohmann::json> events;
  for (const auto &[id, trigger] : subscription_context.id_to_triggers) {
    BuildRedfishEventsForTrigger(trigger, uri_to_response, events);
  }
  return events;
}

// Captures asynchronous responses received on subscribing each OriginResource.
// Invokes `OnAsyncSubscribeComplete` when all OriginResources are subscribed.
class AsyncSubscribeResponse {
 public:
  using OnAsyncSubscribeComplete = std::function<void(
      const absl::Status &,
      const absl::flat_hash_map<EventSourceId, absl::flat_hash_set<std::string>>
          &)>;

  AsyncSubscribeResponse(
      const absl::flat_hash_set<std::string> &unique_resources_to_subscribe,
      OnAsyncSubscribeComplete &&on_async_subscribe_complete)
      : async_subscribe_complete_(std::move(on_async_subscribe_complete)),
        status_(absl::OkStatus()),
        responses_pending_(unique_resources_to_subscribe.begin(),
                           unique_resources_to_subscribe.end()) {}

  void ProcessResponse(absl::string_view uri, const absl::Status &status,
                       const std::vector<EventSourceId> &event_source_ids) {
    absl::MutexLock lock(&mutex_);

    // Check if we are expecting a response for the given URI.
    if (responses_pending_.erase(uri) == 0) {
      status_ = absl::InternalError(
          absl::StrCat("Received unexpected response for URI: ", uri));
      return;
    }

    if (!status_.ok()) {
      // Exit without processing response if even one subscription request has
      // failed. We don't want to have partial subscriptions in the system.
      return;
    }

    if (status_.ok() && !status.ok()) {
      LOG(ERROR) << "Subscribe request to URI failed. URI: " << uri
                 << " status: " << status;
      status_ = status;
    }

    for (const EventSourceId &event_source_id : event_source_ids) {
      event_source_id_to_origin_resources_[event_source_id].insert(
          std::string(uri));
    }

    if (responses_pending_.empty() || !status_.ok()) {
      async_subscribe_complete_(status_, event_source_id_to_origin_resources_);
    }
  }

 private:
  // Callback to invoke when all OriginResources are subscribed.
  const OnAsyncSubscribeComplete async_subscribe_complete_;

  absl::Mutex mutex_;

  // EventSourceIds returned from
  absl::flat_hash_map<EventSourceId, absl::flat_hash_set<std::string>>
      event_source_id_to_origin_resources_ ABSL_GUARDED_BY(mutex_);
  absl::Status status_ ABSL_GUARDED_BY(mutex_);

  // Tracks async subscribe responses received from Subscription Backend.
  // Response can be valid or invalid.
  absl::flat_hash_set<std::string> responses_pending_ ABSL_GUARDED_BY(mutex_);
};

// Aggregates responses received from querying multiple URIs asynchronously.
// Invokes `done_callback` when all OriginResources are queried.
class AggregatedQueryResponse {
 public:
  AggregatedQueryResponse(
      const absl::flat_hash_set<std::string> &unique_resources_to_query,
      std::function<void(absl::flat_hash_map<std::string, nlohmann::json>)>
          done_callback)
      : responses_pending_(unique_resources_to_query.begin(),
                           unique_resources_to_query.end()),
        done_callback_(std::move(done_callback)) {}

  void AddQueryResponse(absl::string_view uri, const absl::Status &status,
                        const nlohmann::json &query_response) {
    absl::MutexLock lock(&query_mutex_);
    if (responses_pending_.erase(uri) == 0) {
      LOG(ERROR) << "Received unexpected response for URI: " << uri;
      return;
    }

    if (!status.ok()) {
      LOG(ERROR) << "Cannot create RedfishEvent for uri: " << uri
                 << " error: " << status.message();
      return;
    }
    uri_to_response_[uri] = query_response;

    if (responses_pending_.empty()) {
      done_callback_(uri_to_response_);
    }
  }

 private:
  absl::Mutex query_mutex_;
  absl::flat_hash_set<std::string> responses_pending_
      ABSL_GUARDED_BY(query_mutex_);
  absl::flat_hash_map<std::string, nlohmann::json> uri_to_response_
      ABSL_GUARDED_BY(query_mutex_);
  std::function<void(absl::flat_hash_map<std::string, nlohmann::json>)>
      done_callback_;
};

// Concrete implementation of SubscriptionService.
// Thread safe
class SubscriptionServiceImpl
    : public SubscriptionService,
      public std::enable_shared_from_this<SubscriptionServiceImpl> {
 public:
  SubscriptionServiceImpl(
      std::unique_ptr<SubscriptionBackend> subscription_backend,
      std::unique_ptr<SubscriptionStore> subscription_store,
      std::unique_ptr<EventStore> event_store)
      : subscription_backend_(
            ABSL_DIE_IF_NULL(std::move(subscription_backend))),
        subscription_store_(ABSL_DIE_IF_NULL(std::move(subscription_store))),
        event_store_(ABSL_DIE_IF_NULL(std::move(event_store))) {}

  // Creates Event Subscription.
  // Invokes `on_subscribe_callback` with unique event subscription id after
  // successful subscription or with an error status if subscription
  // `request` is not formatted per the EventDestination schema.
  // The parameter `on_event_callback` is stored with the subscription to be
  // invoked to dispatch events to the subscriber.
  void CreateSubscription(
      const nlohmann::json &request,
      const std::unordered_set<std::string> &peer_privileges,
      std::function<void(const absl::StatusOr<SubscriptionId> &)>
          on_subscribe_callback,
      std::function<void(const nlohmann::json &)> on_event_callback) override {
    // Check if Oem, Google, and Triggers exist
    auto find_oem = request.find(kPropertyOem);
    if (find_oem == request.end()) {
      on_subscribe_callback(
          absl::InvalidArgumentError(PropertyNotPopulatedError(kPropertyOem)));
      return;
    }

    auto find_google = find_oem->find(kPropertyGoogle);
    if (find_google == find_oem->end()) {
      on_subscribe_callback(absl::InvalidArgumentError(
          PropertyNotPopulatedError(kPropertyGoogle)));
      return;
    }

    auto find_triggers = find_google->find(kPropertyTriggers);
    if (find_triggers == find_google->end() || find_triggers->empty()) {
      on_subscribe_callback(absl::InvalidArgumentError(
          PropertyNotPopulatedError(kPropertyTriggers)));
      return;
    }

    // Maps trigger id to trigger object.
    absl::flat_hash_map<std::string, Trigger> trigger_id_to_trigger_obj;

    // Store unique Redfish resources to subscribe.
    absl::flat_hash_set<std::string> unique_resources_to_subscribe;

    // Parse Triggers
    for (const auto &trigger : *find_triggers) {
      if (trigger.is_null()) {
        on_subscribe_callback(
            absl::InvalidArgumentError("Trigger not populated"));
        return;
      }

      absl::StatusOr<Trigger> trigger_obj = Trigger::Create(trigger);
      if (!trigger_obj.ok()) {
        on_subscribe_callback(trigger_obj.status());
        return;
      }

      // Update trigger id associations.
      trigger_id_to_trigger_obj.insert_or_assign(trigger_obj->id, *trigger_obj);

      for (const std::string &origin_resource : trigger_obj->origin_resources) {
        // Skip subscribing if mask is set.
        if (trigger_obj->mask) continue;
        unique_resources_to_subscribe.insert(origin_resource);
      }
    }

    auto on_subscribe = std::make_shared<
        std::function<void(const absl::StatusOr<SubscriptionId> &)>>(
        std::move(on_subscribe_callback));
    auto on_async_subscribe_complete =
        [on_subscribe(on_subscribe), google_obj(*find_google),
         on_event_callback(std::move(on_event_callback)),
         trigger_id_to_trigger_obj, request,
         this, privileges = peer_privileges](
            const absl::Status &status,
               const absl::flat_hash_map<EventSourceId,
                                         absl::flat_hash_set<std::string>>
                   &event_source_to_origin_resources) mutable {
          if (!status.ok()) {
            (*on_subscribe)(status);
            return;
          }

          // If subscriber requested to stream events queued after given
          // `last_event_id`, pull the events from the event store and dispatch.
          if (auto find_last_event_id = google_obj.find(kPropertyLastEventId);
              find_last_event_id != google_obj.end()) {
            // Dispatch events since last event id.
            absl::c_for_each(
                event_store_->GetEventsSince(find_last_event_id->get<size_t>()),
                on_event_callback);
          }

          // Create subscription id.
          SubscriptionId subscription_id(
              absl::HashOf(absl::Now(), request.dump()));

          // Build and store subscription context.
          auto subscription_context = std::make_unique<SubscriptionContext>(
              subscription_id, event_source_to_origin_resources,
              trigger_id_to_trigger_obj, std::move(on_event_callback));
          subscription_context->privileges = privileges;

          // Add subscription to subscription store.
          absl::Status new_subscription_status =
              subscription_store_->AddNewSubscription(
                  std::move(subscription_context));

          if (!new_subscription_status.ok()) {
            (*on_subscribe)(new_subscription_status);
            return;
          }

          // If we reach this point, we have created a subscription
          // successfully. Invoke the callback to complete the subscription
          // sequence.
          (*on_subscribe)(subscription_id);
        };

    // Now we invoke subscribe on the subscription backend. This should send
    // subscription requests to each event source which will return one or more
    // event source ids.
    auto async_subscribe_response = std::make_shared<AsyncSubscribeResponse>(
        unique_resources_to_subscribe, std::move(on_async_subscribe_complete));
    for (const std::string &origin_resource : unique_resources_to_subscribe) {
      absl::Status status = subscription_backend_->Subscribe(
          origin_resource,
          [async_subscribe_response, origin_resource](
              const absl::Status &status,
              const std::vector<EventSourceId> &event_source_ids) {
            async_subscribe_response->ProcessResponse(origin_resource, status,
                                                      event_source_ids);
          },
          peer_privileges);
      if (!status.ok()) {
        (*on_subscribe)(status);
        return;
      }
    }
  }

  void DeleteSubscription(const SubscriptionId &subscription_id) override {
    subscription_store_->DeleteSubscription(subscription_id);
  }

  // Retrieves all subscriptions managed by the service.
  absl::Span<const SubscriptionContext> GetAllSubscriptions() override {
    LOG(ERROR) << "GetAllSubscriptions: Unimplemented!";
    return {};
  }

  nlohmann::json GetSubscriptionsToJSON() override {
    return subscription_store_->ToJSON();
  }

  std::string GetSubscriptionsToString() override {
    return subscription_store_->ToString();
  }

  nlohmann::json GetEventsToJSON() override {
    return event_store_->ToJSON();
  }

  std::string GetEventsToString() override {
    return event_store_->ToString();
  }

  void ClearEventStore() override {
    event_store_->Clear();
  }

  nlohmann::json GetEventsBySubscriptionIdToJSON(
      size_t subscription_id) override {
    nlohmann::json json;
    // Check if the subscription_id is valid
    absl::StatusOr<const SubscriptionContext*> sc =
        subscription_store_->GetSubscription(SubscriptionId(subscription_id));
    if (!sc.ok()) {
      json["Error"] = "Invalid subscription id";
      return json;
    }
    json["SubscriptionId"] = subscription_id;
    json["Events"] = event_store_->GetEventsBySubscriptionId(subscription_id);
    return json;
  }
  // Invoked by Redfish event sources which are typically implementations that
  // monitor sources like DBus monitor, file i/o, socket ingress etc.
  //
  // Notify function is invoked with `event_source_id` that uniquely identifies
  // event source and `status` to indicate the subscription service of an error
  // condition at the event source which would trigger delete subscription
  // sequence.
  absl::Status Notify(EventSourceId event_source_id,
                      [[maybe_unused]] const absl::Status &status) override {
    // Pull subscription context from Subscription Store
    ECCLESIA_ASSIGN_OR_RETURN(
        auto contexts,
        subscription_store_->GetSubscriptionsByEventSourceId(event_source_id));

    auto event_source_id_new =
        std::make_shared<EventSourceId>(std::move(event_source_id));
    for (const SubscriptionContext *subscription_context : contexts) {
      // Find URIs to query.
      // A subscription context is always expected to have a set of URIs
      // associated with event source. Therefore, we don't handle the not found
      // case here.
      auto find_uris_to_query =
          subscription_context->event_source_to_uri.find(*event_source_id_new);
      auto aggregated_response = std::make_shared<AggregatedQueryResponse>(
          find_uris_to_query->second,
          [this, subscription_context, event_source_id_new](
              const absl::flat_hash_map<std::string, nlohmann::json>
                  &uri_to_response) {
            HandleEventsForSubscription(*subscription_context,
                                        *event_source_id_new, uri_to_response);
          });

      // Query Redfish URIs to build origin of condition.
      for (const std::string &uri : find_uris_to_query->second) {
        ECCLESIA_RETURN_IF_ERROR(subscription_backend_->Query(
            uri,
            [uri, aggregated_response](
                const absl::Status &sc,
                const nlohmann::json &query_response) mutable {
              aggregated_response->AddQueryResponse(uri, sc, query_response);
            },
            subscription_context->privileges));
      }
    }
    return absl::OkStatus();
  }

  // Just like Notify() with an additional parameter |data| that represents
  // Redfish resource associated with event source.
  absl::Status NotifyWithData(
      [[maybe_unused]] EventSourceId key, [[maybe_unused]] absl::Status status,
      [[maybe_unused]] const nlohmann::json &data) override {
    return absl::UnimplementedError("NotifyWithData:: Unimplemented!");
  }

 private:
  void HandleEventsForSubscription(
      const SubscriptionContext &context, const EventSourceId &source_id,
      const absl::flat_hash_map<std::string, nlohmann::json> &uri_to_response) {
    // Create event id as a function of source id and time.
    EventId event_id(context.subscription_id, source_id, absl::Now());

    // Construct RedfishEvents for subscription.
    std::vector<nlohmann::json> events =
        BuildRedfishEventsForSubscription(context, uri_to_response);

    // If there are no events to dispatch, Redfish Event response is not
    // created and we return early.
    if (events.empty()) return;

    nlohmann::json redfish_event_obj;
    redfish_event_obj["Id"] = event_id.redfish_event_id;
    redfish_event_obj["@odata.type"] = "#Event.v1_7_0.Event";
    redfish_event_obj["Name"] = "RedfishEvent";
    redfish_event_obj["Events"] = events;

    // Add constructed Redfish event to event store.
    event_store_->AddNewEvent(event_id, redfish_event_obj);

    // Send event to the destination.
    context.on_event_callback(redfish_event_obj);
  }

  std::unique_ptr<SubscriptionBackend> subscription_backend_;
  std::unique_ptr<SubscriptionStore> subscription_store_;
  std::unique_ptr<EventStore> event_store_;
};

}  // namespace

std::unique_ptr<SubscriptionService> CreateSubscriptionService(
    std::unique_ptr<SubscriptionBackend> subscription_backend,
    std::unique_ptr<SubscriptionStore> subscription_store,
    std::unique_ptr<EventStore> event_store) {
  return std::make_unique<SubscriptionServiceImpl>(
      std::move(subscription_backend), std::move(subscription_store),
      std::move(event_store));
}

}  // namespace ecclesia
