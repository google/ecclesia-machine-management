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

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "gmock/gmock.h"
#include "absl/log/log.h"
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

class SubscriptionBackendMock : public SubscriptionBackend {
 public:
  MOCK_METHOD(absl::Status, Subscribe,
              (absl::string_view, SubscribeCallback &&,
               const std::unordered_set<std::string> &),
              (override));
  MOCK_METHOD(absl::Status, Query,
              (const absl::string_view, QueryCallback &&,
               const std::unordered_set<std::string> &),
              (override));
};

class EventStoreMock : public EventStore {
 public:
  MOCK_METHOD(void, AddNewEvent,
              (const EventId &event_id, const nlohmann::json &event),
              (override));
  MOCK_METHOD(std::vector<nlohmann::json>, GetEventsSince,
              (std::optional<size_t> redfish_event_id), (override));
  MOCK_METHOD(nlohmann::json, GetEvent, (const EventId &event_id), (override));
  MOCK_METHOD(nlohmann::json, ToJSON, (), (override));
  MOCK_METHOD(std::string, ToString, (), (override));
  MOCK_METHOD(void, Clear, (), (override));
};

class SubscriptionServiceMock : public SubscriptionService {
 public:
  absl::Status Notify(EventSourceId event_source_id,
                      const absl::Status &status) override {
    // to appease the compiler - this code path will not be called
    if (!status.ok()) {
      LOG(WARNING) << "Bad status for " << event_source_id.ToString();
    }
    ++num_notifications_;
    return absl::OkStatus();
  }

  int NumNotifications() const { return num_notifications_; }

  MOCK_METHOD(void, CreateSubscription,
              (const nlohmann::json &, const std::unordered_set<std::string> &,
               std::function<void(const absl::StatusOr<SubscriptionId> &)>,
               std::function<void(const nlohmann::json &)>),
              (override));

  MOCK_METHOD(void, DeleteSubscription, (const SubscriptionId &subscription_id),
              (override));

  MOCK_METHOD(absl::Span<const SubscriptionContext>, GetAllSubscriptions, (),
              (override));

  MOCK_METHOD(absl::Status, NotifyWithData,
              (EventSourceId key, absl::Status status,
               const nlohmann::json &data),
              (override));

  MOCK_METHOD(nlohmann::json, GetSubscriptionsToJSON, (),
              (override));

  MOCK_METHOD(std::string, GetSubscriptionsToString, (),
              (override));

  MOCK_METHOD(nlohmann::json, GetEventsToJSON, (),
              (override));

  MOCK_METHOD(std::string, GetEventsToString, (),
              (override));

  MOCK_METHOD(void, ClearAllEvents, (),
              (override));

 private:
  std::atomic_int num_notifications_ = 0;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_EVENT_SERVER_SUBSCRIPTION_MOCK_H_
