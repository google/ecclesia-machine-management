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

#include <atomic>
#include <cstddef>
#include <ctime>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
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
#include "ecclesia/lib/redfish/event/server/subscription_impl.h"
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

// Updates EventSource to URI map in |trigger| object for given |source_ids| and
// |origin_resource|.
absl::Status AddEventSourcesToTrigger(
    const std::vector<EventSourceId> &source_ids,
    const std::string &origin_resource, Trigger &trigger) {
  for (const EventSourceId &source_id : source_ids) {
    auto find_event_source = trigger.event_source_to_uri.find(source_id);
    if (find_event_source == trigger.event_source_to_uri.end()) {
      trigger.event_source_to_uri.insert({source_id, {origin_resource}});
    } else {
      find_event_source->second.push_back(origin_resource);
    }
  }
  return absl::OkStatus();
}

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

  // Returns unique event subscription id after validating the request and
  // enabling event listeners.
  // Returns error if subscription |request| is not formatted per the
  // EventDestination schema and OEM extension.
  // The parameter |on_event_callback| is stored with the subscription to be
  // invoked to dispatch events to the subscriber.
  absl::StatusOr<SubscriptionId> CreateSubscription(
      const nlohmann::json &request,
      std::function<void(const nlohmann::json &)> &&on_event_callback)
      override {
    // Create subscription id.
    SubscriptionId subscription_id(absl::HashOf(absl::Now(), request.dump()));

    // Check if Oem, Google, and Triggers exist
    auto find_oem = request.find(kPropertyOem);
    if (find_oem == request.end()) {
      return absl::InvalidArgumentError(
          PropertyNotPopulatedError(kPropertyOem));
    }

    auto find_google = find_oem->find(kPropertyGoogle);
    if (find_google == find_oem->end()) {
      return absl::InvalidArgumentError(
          PropertyNotPopulatedError(kPropertyGoogle));
    }

    auto find_triggers = find_google->find(kPropertyTriggers);
    if (find_triggers == find_google->end() || find_triggers->empty()) {
      return absl::InvalidArgumentError(
          PropertyNotPopulatedError(kPropertyTriggers));
    }

    absl::flat_hash_map<std::string, Trigger> id_to_triggers;

    // Parse Triggers array
    for (const auto &trigger : *find_triggers) {
      if (trigger.is_null()) {
        return absl::InvalidArgumentError("Trigger not populated");
      }

      ECCLESIA_ASSIGN_OR_RETURN(Trigger trigger_obj, Trigger::Create(trigger));

      // Invoke subscribe on subscription_backend_ for each origin_resources in
      // the trigger and store the returned source ids.
      for (const std::string &origin_resource : trigger_obj.origin_resources) {
        if (trigger_obj.mask) continue;
        ECCLESIA_ASSIGN_OR_RETURN(
            std::vector<EventSourceId> source_ids,
            subscription_backend_->Subscribe(origin_resource));
        ECCLESIA_RETURN_IF_ERROR(
            AddEventSourcesToTrigger(source_ids, origin_resource, trigger_obj));
      }

      auto find_id = trigger.find(kPropertyId);
      if (find_id == trigger.end() || !find_id->is_string()) {
        return absl::InvalidArgumentError(
            PropertyNotPopulatedError(kPropertyId));
      }

      id_to_triggers.insert_or_assign(find_id->get<std::string>(), trigger_obj);
    }

    // If subscriber requested to stream events queued after given
    // "last_event_id", pull the events from the event store and dispatch.
    if (auto find_last_event_id = find_google->find(kPropertyLastEventId);
        find_last_event_id != find_google->end()) {
      // Dispatch events since last event id.
      absl::c_for_each(
          event_store_->GetEventsSince(find_last_event_id->get<size_t>()),
          on_event_callback);
    }

    // Build and store subscription context.
    auto subscription_context = std::make_unique<SubscriptionContext>(
        subscription_id, id_to_triggers, std::move(on_event_callback));

    ECCLESIA_RETURN_IF_ERROR(subscription_store_->AddNewSubscription(
        std::move(subscription_context)));
    return subscription_id;
  }

  void DeleteSubscription(const SubscriptionId& subscription_id) override {
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
                      [[maybe_unused]] absl::Status status) override {
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
          &&done_callback) {
    const std::size_t uri_count = uri_collection.size();

    // Aggregates query responses from each origin resource.
    // Shared pointer is used to allow OriginOfCondition to build asynchronously
    // if needed to for Redfish backends that use async mode of execution.
    auto aggregated_response = std::make_shared<AggregatedQueryResponse>(
        uri_count, std::move(done_callback));

    for (const std::string &uri : uri_collection) {
      ECCLESIA_RETURN_IF_ERROR(subscription_backend_->Query(
          uri, [uri, aggregated_response](
                   const absl::Status &sc,
                   const nlohmann::json &query_response) mutable {
            if (!sc.ok()) {
              LOG(ERROR) << "Cannot create RedfishEvent for uri: " << uri
                         << " error: " << sc.message();
              return;
            }
            aggregated_response->AddQueryResponse(query_response);
          }));
    }
    return absl::OkStatus();
  }

  absl::Status HandleEventsForSubscription(const SubscriptionContext &context,
                                           const EventSourceId &source_id) {
    // Get Trigger
    for (const auto &[_, trigger] : context.id_to_triggers) {
      // Get URI collection associated with the event source.
      auto uri_collection = trigger.event_source_to_uri.find(source_id);
      if (uri_collection == trigger.event_source_to_uri.end()) {
        return absl::InvalidArgumentError(
            absl::StrCat("Cannot find URI Collection for event source id ",
                         source_id.ToString()));
      }

      // Get Origin of condition
      ECCLESIA_RETURN_IF_ERROR(BuildOriginOfCondition(
          uri_collection->second,
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
          }));
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
