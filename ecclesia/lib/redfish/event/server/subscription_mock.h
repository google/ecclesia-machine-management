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

#ifndef ECCLESIA_LIB_REDFISH_EVENT_SERVER_SUBSCRIPTION_MOCK_H_
#define ECCLESIA_LIB_REDFISH_EVENT_SERVER_SUBSCRIPTION_MOCK_H_

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ecclesia/lib/redfish/event/server/subscription.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {

class SubscriptionStoreMock : public SubscriptionStore {
 public:
  MOCK_METHOD(absl::Status, AddNewSubscription,
              (std::unique_ptr<SubscriptionContext> subscription_context),
              (override));
  MOCK_METHOD(
      absl::StatusOr<absl::Span<const ecclesia::SubscriptionContext *const>>,
      GetSubscriptionsByEventSourceId, (const EventSourceId &id), (override));
  MOCK_METHOD(absl::StatusOr<const SubscriptionContext *>, GetSubscription,
              (const SubscriptionId &subscription_id), (override));
  MOCK_METHOD(void, DeleteSubscription, (const SubscriptionId &subscription_id),
              (override));
  MOCK_METHOD(nlohmann::json, ToJSON, (), (override));
  MOCK_METHOD(std::string, ToString, (), (override));
};

class RedfishHandlerMock : public RedfishHandler {
 public:
  MOCK_METHOD(absl::StatusOr<std::vector<EventSourceId>>, Subscribe,
              (absl::string_view url), (override));
  MOCK_METHOD(absl::Status, Query, (const absl::string_view, QueryCallback),
              (override));
};

class EventStoreMock : public EventStore {
 public:
  MOCK_METHOD(void, AddNewEvent,
              (const EventId &event_id, const nlohmann::json &event),
              (override));
  MOCK_METHOD(std::vector<nlohmann::json>, GetEventsSince,
              (std::optional<size_t> redfish_event_id), (override));
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_EVENT_SERVER_SUBSCRIPTION_MOCK_H_
