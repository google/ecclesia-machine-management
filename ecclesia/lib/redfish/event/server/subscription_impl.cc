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

#include <atomic>
#include <cstddef>
#include <ctime>
#include <functional>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/attributes.h"
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

// Constructs a RedfishEvent from |query_response|
void BuildRedfishEvent(const nlohmann::json &query_response,
                       nlohmann::json &redfish_event) {
  // Generate Event Id
  size_t event_id = absl::HashOf(absl::Now(), query_response.dump());
  redfish_event["EventId"] = event_id;

  // Get ISO extended string for timestamp
  redfish_event["EventTimestamp"] =
      absl::FormatTime(absl::RFC3339_full, absl::Now(), absl::UTCTimeZone());

  // Add origin of condition.
  redfish_event["OriginOfCondition"] = query_response;
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
  ;
};

// Aggregates responses received from querying each URI in OriginResources.
// Invokes |done_callback| when all OriginResources are queried.
class AggregatedQueryResponse {
 public:
  AggregatedQueryResponse(
      int max_origin_resources,
      std::function<void(absl::Status, nlohmann::json::array_t &)>
          &&done_callback)
      : max_origin_resources_(max_origin_resources),
        done_callback_(std::move(done_callback)) {}

  void AddQueryResponse(const nlohmann::json &query_response) {
    // Tracks received responses.
    ++responses_received_;

    // Redfish Event to format using the query response.
    nlohmann::json redfish_event;
    BuildRedfishEvent(query_response, redfish_event);
    {
      absl::MutexLock lock(&events_mutex_);
      events_.push_back(std::move(redfish_event));

      if (responses_received_ == max_origin_resources_) {
        done_callback_(absl::OkStatus(), events_);
      }
    }
  }

 private:
  // Counts response to sync/async query requests sent via Subscription Backend.
  // Response can be valid or invalid.
  std::atomic_size_t responses_received_{0};

  // Maximum number of OriginResources to be queried.
  size_t max_origin_resources_;

  std::function<void(absl::Status, nlohmann::json::array_t &)> done_callback_;

  absl::Mutex events_mutex_;

  // Redfish Events containing full RedfishResource as OriginOfCondition if
  // requested.
  nlohmann::json::array_t events_ ABSL_GUARDED_BY(events_mutex_);
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

  // Invoked by Redfish event sources which are typically implementations that
  // monitor sources like DBus monitor, file i/o, socket ingress etc.
  //
  // Notify function is invoked with |key| that uniquely identifies event source
  // and |status| to indicate the subscription service of an error condition at
  // the event source which would trigger delete subscription sequence.
  absl::Status Notify(EventSourceId key,
                      [[maybe_unused]] const absl::Status &status) override {
    // Pull subscription context from Subscription Store
    ECCLESIA_ASSIGN_OR_RETURN(
        auto contexts,
        subscription_store_->GetSubscriptionsByEventSourceId(key));

    for (const SubscriptionContext *subscription_context : contexts) {
      ECCLESIA_RETURN_IF_ERROR(
          HandleEventsForSubscription(*subscription_context, key));
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
  // Builds Redfish Resources necessary for embedding OriginOfCondition in
  // Redfish event.
  // Returns error if OriginOfCondition cannot be built for an event.
  //
  // This function is designed to execute Redfish queries to backend
  // asynchronously hence the careful use of std::shared_ptr to aggregate
  // resources and managing lifetime.
  absl::Status BuildOriginOfCondition(
      const std::vector<std::string> &uri_collection,
      std::function<void(absl::Status, nlohmann::json::array_t &)>
          &&done_callback,
      const std::unordered_set<std::string> &peer_privileges) {
    const std::size_t uri_count = uri_collection.size();

    // Aggregates query responses from each origin resource.
    // Shared pointer is used to allow OriginOfCondition to build asynchronously
    // if needed to for Redfish backends that use async mode of execution.
    auto aggregated_response = std::make_shared<AggregatedQueryResponse>(
        uri_count, std::move(done_callback));

    for (const std::string &uri : uri_collection) {
      ECCLESIA_RETURN_IF_ERROR(subscription_backend_->Query(
          uri,
          [uri, aggregated_response](
              const absl::Status &sc,
              const nlohmann::json &query_response) mutable {
            if (!sc.ok()) {
              LOG(ERROR) << "Cannot create RedfishEvent for uri: " << uri
                         << " error: " << sc.message();
              return;
            }
            aggregated_response->AddQueryResponse(query_response);
          },
          peer_privileges));
    }
    return absl::OkStatus();
  }

  absl::Status HandleEventsForSubscription(const SubscriptionContext &context,
                                           const EventSourceId &source_id) {
    // Check if given SubscriptionContext subscribes to given source_id for
    // events
    if (auto origin_resources = context.event_source_to_uri.find(source_id);
        origin_resources != context.event_source_to_uri.end()) {
      ECCLESIA_RETURN_IF_ERROR(BuildOriginOfCondition(
          std::vector<std::string>(origin_resources->second.begin(),
                                   origin_resources->second.end()),
          [source_id, context, this](const absl::Status &sc,
                                     nlohmann::json::array_t &events) mutable {
            if (!sc.ok()) {
              LOG(ERROR) << "Cannot create RedfishEvent for event source_id"
                         << source_id.ToString();
              return;
            }

            // Create event id as a function of source id and time.
            EventId event_id(context.subscription_id, source_id, absl::Now());

            nlohmann::json redfish_event_obj;
            redfish_event_obj["Id"] = event_id.redfish_event_id;
            redfish_event_obj["@odata.type"] = "#Event.v1_7_0.Event";
            redfish_event_obj["Name"] = "RedfishEvent";
            redfish_event_obj["Events"] = events;

            event_store_->AddNewEvent(event_id, redfish_event_obj);

            // Send event to the destination.
            context.on_event_callback(redfish_event_obj);
          },
          context.privileges));
    }
    return absl::OkStatus();
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
