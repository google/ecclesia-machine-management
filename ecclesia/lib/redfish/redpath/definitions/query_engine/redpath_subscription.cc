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

#include "ecclesia/lib/redfish/redpath/definitions/query_engine/redpath_subscription.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/function_ref.h"
#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/status/macros.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {

namespace {

absl::StatusOr<nlohmann::json> CreateTrigger(
    const RedPathSubscription::Configuration &config) {
  nlohmann::json origin_resources;
  if (config.uris.empty()) {
    return absl::InvalidArgumentError("No uris provided");
  }
  if (config.query_id.empty()) {
    return absl::InvalidArgumentError("No query id provided");
  }
  if (config.subquery_id.empty()) {
    return absl::InvalidArgumentError("No subquery id provided");
  }
  for (const auto &uri : config.uris) {
    origin_resources.push_back({{"@odata.id", uri}});
  }
  std::string trigger_id =
      absl::StrCat(config.query_id, ",", config.subquery_id);

  nlohmann::json trigger;
  trigger["OriginResources"] = origin_resources;
  trigger["Id"] = trigger_id;
  trigger["Predicate"] = config.predicate;
  return trigger;
}

absl::StatusOr<nlohmann::json> CreateSubscriptionRequest(
    const std::vector<RedPathSubscription::Configuration> &configurations) {
  nlohmann::json subscription_request;

  // Populate top-level fields.
  subscription_request["EventFormatType"] = "Event";
  subscription_request["Protocol"] = "Redfish";
  subscription_request["SubscriptionType"] = "Oem";
  subscription_request["OEMSubscriptionType"] = "gRPC";
  subscription_request["IncludeOriginOfCondition"] = true;
  subscription_request["DeliveryRetryPolicy"] = "SuspendRetries";

  // Add `Triggers` to `Google` object.
  nlohmann::json google_object;
  nlohmann::json &triggers = google_object["Triggers"];
  triggers = nlohmann::json::array();
  for (const auto &config : configurations) {
    ECCLESIA_ASSIGN_OR_RETURN(nlohmann::json trigger, CreateTrigger(config));
    triggers.push_back(std::move(trigger));
  }

  // Add `Google` to `Oem` object.
  nlohmann::json oem_object;
  oem_object["Google"] = google_object;

  // Add `Oem` to the main object
  subscription_request["Oem"] = oem_object;

  return subscription_request;
}

// Handles Redfish events.
// Responsible for parsing `event` to build `RedfishVariant` and `EventContext`.
// Invokes `on_event_callback` on valid events and `on_stop_callback` when
// streaming stops.
void HandleEvent(
    const RedfishVariant &event,
    absl::FunctionRef<void(const RedfishVariant &,
                           const RedPathSubscription::EventContext &)>
        on_event_callback) {
  if (!event.status().ok()) {
    LOG(ERROR) << "Event Redfish Variant error: " << event.status();
    return;
  }
  // Get the context from Redfish Event.
  std::unique_ptr<RedfishObject> event_object = event.AsObject();
  if (event_object == nullptr) {
    return;
  }

  RedfishVariant events = event_object->Get("Events");
  std::unique_ptr<RedfishIterable> events_iter = events.AsIterable();
  if (events_iter == nullptr) {
    LOG(ERROR) << "Events array formatted incorrectly in response!";
    return;
  }

  for (int i = 0; i < events_iter->Size(); ++i) {
    RedfishVariant single_event = (*events_iter)[i];
    std::unique_ptr<RedfishObject> single_event_object =
        single_event.AsObject();
    if (single_event_object == nullptr) {
      LOG(ERROR) << "Event formatted incorrectly in response!";
      continue;
    }

    std::optional<std::string> context =
        single_event_object->GetNodeValue<std::string>("Context");
    if (!context.has_value()) {
      LOG(ERROR) << "No context found in event";
      return;
    }

    std::vector<std::string> id_parts = absl::StrSplit(*context, ',');
    if (id_parts.size() != 2) {
      LOG(ERROR) << "Invalid context format: " << *context;
      return;
    }

    std::optional<std::string> event_id =
        single_event_object->GetNodeValue<std::string>("EventId");
    if (!event_id.has_value()) {
      LOG(ERROR) << "No event id found in event";
      return;
    }

    std::optional<std::string> timestamp =
        single_event_object->GetNodeValue<std::string>("EventTimestamp");
    if (!timestamp.has_value()) {
      LOG(ERROR) << "No timestamp found in event";
      return;
    }

    RedPathSubscription::EventContext event_context;
    event_context.query_id = id_parts[0];
    event_context.subquery_id = id_parts[1];
    event_context.event_id = *event_id;
    event_context.event_timestamp = *timestamp;

    RedfishVariant origin_of_condition =
        single_event_object->Get("OriginOfCondition");
    if (!origin_of_condition.status().ok()) {
      continue;
    }
    on_event_callback(origin_of_condition, event_context);
  }
}

}  // namespace

absl::StatusOr<std::unique_ptr<RedPathSubscriptionImpl>>
RedPathSubscriptionImpl::Create(
    const std::vector<Configuration> &configurations,
    RedfishInterface &redfish_interface, OnEventCallback on_event_callback,
    OnStopCallback on_stop_callback) {
  // Create Subscription request json.
  ECCLESIA_ASSIGN_OR_RETURN(nlohmann::json subscription_request,
                            CreateSubscriptionRequest(configurations));

  // Subscribe
  ECCLESIA_ASSIGN_OR_RETURN(
      std::unique_ptr<RedfishEventStream> event_stream,
      redfish_interface.Subscribe(
          subscription_request.dump(),
          [on_event_callback =
               std::move(on_event_callback)](const RedfishVariant &event) {
            HandleEvent(event, on_event_callback);
          },
          [on_stop_callback(std::move(on_stop_callback))](
              const absl::Status &end_status) {
            on_stop_callback(end_status);
          }));

  return absl::WrapUnique(new RedPathSubscriptionImpl(std::move(event_stream)));
}

}  // namespace ecclesia
