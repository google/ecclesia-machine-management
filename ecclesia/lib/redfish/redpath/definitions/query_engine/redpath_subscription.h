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

#ifndef ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_ENGINE_REDPATH_SUBSCRIPTION_H_
#define ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_ENGINE_REDPATH_SUBSCRIPTION_H_

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/transport/interface.h"

namespace ecclesia {

// Interface that allows RedPath query engine to manage an event subscription.
class RedPathSubscription {
 public:
  // Context provided to QueryEngine for each event.
  struct EventContext {
    std::string query_id;
    std::string redpath;
    std::string event_id;
    std::string event_timestamp;
  };

  // Subscription Configuration provided by QueryEngine.
  struct Configuration {
    std::string query_id;
    std::string redpath;

    // URIs to subscribe.
    std::vector<std::string> uris;

    // Trigger to pass along in the subscription.
    std::string predicate;
  };

  using OnEventCallback =
      std::function<void(const RedfishVariant & /*origin_of_condition*/,
                         const EventContext & /*context*/)>;
  using OnStopCallback = std::function<void(const absl::Status &)>;

  virtual ~RedPathSubscription() = default;
  virtual absl::Status CancelSubscription() = 0;
};

// Concrete Implementation of RedPathSubscription.
class RedPathSubscriptionImpl : public RedPathSubscription {
 public:
  // Creates an Event subscription and Returns instance of
  // `RedPathSubscription` to allow caller to manage subscription.
  //
  // Subscription triggers are configured using `configurations`.
  // The callback `on_event_callback` is invoked for each OriginOfCondition
  // in the RedfishEvent after parsing `Context` and converting
  // OriginOfCondition into RedfishVariant.
  // The callback `on_stop_callback` is invoked when streaming stops.
  static absl::StatusOr<std::unique_ptr<RedPathSubscriptionImpl>> Create(
      const std::vector<Configuration> &configurations,
      RedfishInterface &redfish_interface, OnEventCallback on_event_callback,
      OnStopCallback on_stop_callback);

  absl::Status CancelSubscription() override {
    redfish_event_stream_->CancelStreaming();
    return absl::OkStatus();
  }

 private:
  explicit RedPathSubscriptionImpl(
      std::unique_ptr<RedfishEventStream> redfish_event_stream)
      : redfish_event_stream_(std::move(redfish_event_stream)) {}
  std::unique_ptr<RedfishEventStream> redfish_event_stream_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_ENGINE_REDPATH_SUBSCRIPTION_H_
