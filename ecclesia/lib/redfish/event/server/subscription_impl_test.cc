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

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ecclesia/lib/redfish/event/server/subscription.h"
#include "ecclesia/lib/redfish/event/server/subscription_mock.h"
#include "ecclesia/lib/testing/status.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {

namespace {

using ::testing::_;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::UnorderedElementsAre;
using ::testing::WithArg;

class SubscriptionServiceImplTest : public ::testing::Test {
 protected:
  SubscriptionServiceImplTest() {
    auto subscription_backend = std::make_unique<SubscriptionBackendMock>();
    subscription_backend_ptr_ = subscription_backend.get();

    auto subscription_store = std::make_unique<SubscriptionStoreMock>();
    subscription_store_ptr_ = subscription_store.get();

    auto event_store = std::make_unique<EventStoreMock>();
    event_store_ptr_ = event_store.get();

    // Create subscription service with mock objects.
    subscription_service_ = CreateSubscriptionService(
        std::move(subscription_backend), std::move(subscription_store),
        std::move(event_store));
  }

  // Mock objects.
  SubscriptionBackendMock* subscription_backend_ptr_ = nullptr;
  SubscriptionStoreMock* subscription_store_ptr_ = nullptr;
  EventStoreMock* event_store_ptr_ = nullptr;

  // Subscription service under test.
  std::unique_ptr<SubscriptionService> subscription_service_ = nullptr;
};

TEST_F(SubscriptionServiceImplTest,
       DeleteSubscriptionShouldDeleteSameInSubscriptionStore) {
  SubscriptionId fake(123);
  EXPECT_CALL(*subscription_store_ptr_, DeleteSubscription(fake))
      .Times(1)
      .WillOnce(Return());
  subscription_service_->DeleteSubscription(fake);
}

TEST_F(SubscriptionServiceImplTest, ShouldCreateSubscriptionOnValidRequest) {
  // Create valid subscription request.
  static constexpr absl::string_view valid_request = R"json(
    {
      "EventFormatType": "Event",
      "Protocol": "Redfish",
      "SubscriptionType": "OEM",
      "HeartbeatIntervalMinutes": 2,
      "IncludeOriginOfCondition": true,
      "DeliveryRetryPolicy": "SuspendRetries",
      "OEMSubscriptionType": "gRPC",
      "Oem":{
        "Google": {
          "Triggers": [
            {
              "Id": "1",
              "OriginResources":[
                {
                  "@odata.id": "/redfish/v1/Chassis/Foo/Sensors/x"
                },
                {
                  "@odata.id": "/redfish/v1/Chassis/Foo/Sensors/y"
                }
              ],
              "Predicate": "Reading>23"
            }
          ]
        }
      }
    }
  )json";

  static constexpr absl::string_view valid_subscriptions = R"json(
    {
      "NumSubscriptions": 2,
      "Subscriptions": [
        {
          "SubscriptionId": 1,
          "URI": [
              "/redfish/v1/Chassis/Foo/Sensors/x",
              "/redfish/v1/Chassis/Foo/Sensors/y"
            ]
        },
        {
          "SubscriptionId": 2,
          "URI": [
              "/redfish/v1/Chassis/Foo/Sensors/x",
              "/redfish/v1/Chassis/Foo/Sensors/y"
            ]
        }
      ]
    })json";

  nlohmann::json request = nlohmann::json::parse(valid_request);
  ASSERT_TRUE(!request.is_discarded());

  nlohmann::json response = nlohmann::json::parse(valid_subscriptions);
  ASSERT_TRUE(!response.is_discarded());

  EXPECT_CALL(*subscription_backend_ptr_,
              Subscribe(Eq("/redfish/v1/Chassis/Foo/Sensors/x"), _,
                        UnorderedElementsAre("Privilege123")))
      .WillOnce(
          Invoke([](absl::string_view url,
                    SubscriptionBackend::SubscribeCallback&& subscribe_callback,
                    const std::unordered_set<std::string>& privileges) {
            subscribe_callback(absl::OkStatus(),
                               std::vector<EventSourceId>(
                                   {{"1", EventSourceId::Type::kDbusObjects},
                                    {"2", EventSourceId::Type::kDbusObjects}}));
            return absl::OkStatus();
          }));

  EXPECT_CALL(*subscription_backend_ptr_,
              Subscribe(Eq("/redfish/v1/Chassis/Foo/Sensors/y"), _,
                        UnorderedElementsAre("Privilege123")))
      .WillOnce(
          Invoke([](absl::string_view url,
                    SubscriptionBackend::SubscribeCallback&& subscribe_callback,
                    const std::unordered_set<std::string>& privileges) {
            subscribe_callback(absl::OkStatus(),
                               std::vector<EventSourceId>(
                                   {{"3", EventSourceId::Type::kDbusObjects}}));
            return absl::OkStatus();
          }));

  EXPECT_CALL(*subscription_store_ptr_, AddNewSubscription(_))
      .WillOnce(WithArg<0>([](std::unique_ptr<SubscriptionContext> context) {
        EXPECT_GT(context->event_source_to_uri.size(), 0);
        EXPECT_GT(context->id_to_triggers.size(), 0);
        return absl::OkStatus();
      }));

  EXPECT_CALL(*subscription_store_ptr_, ToJSON()).WillRepeatedly(
      testing::Return(response));

  // Expect subscription creation to succeed.
  subscription_service_->CreateSubscription(
      request, {"Privilege123"},
      [](const absl::StatusOr<SubscriptionId>& subscription_id) {
        EXPECT_THAT(subscription_id, IsOk());
      },
      [](const std::string&) {});

  auto json = subscription_service_->GetSubscriptionsToJSON();
  EXPECT_THAT(json["NumSubscriptions"], Eq(2));
  nlohmann::json json_0 = json["Subscriptions"][0];
  nlohmann::json json_1 = json["Subscriptions"][1];

  EXPECT_THAT(json_0["SubscriptionId"],
              testing::AllOf(testing::Ge(1), testing::Le(2)));
  EXPECT_THAT(json_1["SubscriptionId"],
              testing::AllOf(testing::Ge(1), testing::Le(2)));

  EXPECT_THAT(json_0["URI"],
              testing::UnorderedElementsAre("/redfish/v1/Chassis/Foo/Sensors/x",
              "/redfish/v1/Chassis/Foo/Sensors/y"));
  EXPECT_THAT(json_1["URI"],
              testing::UnorderedElementsAre("/redfish/v1/Chassis/Foo/Sensors/x",
              "/redfish/v1/Chassis/Foo/Sensors/y"));
}

TEST_F(SubscriptionServiceImplTest,
       ShouldNotCreateWhenBackendSubscriptionRequestFails) {
  // Create valid subscription request.
  static constexpr absl::string_view valid_request = R"json(
    {
      "EventFormatType": "Event",
      "Protocol": "Redfish",
      "SubscriptionType": "OEM",
      "HeartbeatIntervalMinutes": 2,
      "IncludeOriginOfCondition": true,
      "DeliveryRetryPolicy": "SuspendRetries",
      "OEMSubscriptionType": "gRPC",
      "Oem":{
        "Google": {
          "Triggers": [
            {
              "Id": "1",
              "OriginResources":[
                {
                  "@odata.id": "/redfish/v1/Chassis/Foo/Sensors/x"
                },
                {
                  "@odata.id": "/redfish/v1/Chassis/Foo/Sensors/y"
                }
              ],
              "Predicate": "Reading>23"
            }
          ]
        }
      }
    }
  )json";

  nlohmann::json request = nlohmann::json::parse(valid_request);
  ASSERT_TRUE(!request.is_discarded());

  // Backend->Subscribe returns an error.
  {
    EXPECT_CALL(*subscription_backend_ptr_,
                Subscribe(_, _, UnorderedElementsAre("Privilege123")))
        .WillOnce(Invoke(
            [](absl::string_view url,
               SubscriptionBackend::SubscribeCallback&& subscribe_callback,
               const std::unordered_set<std::string>& privileges) {
              subscribe_callback(
                  absl::OkStatus(),
                  std::vector<EventSourceId>(
                      {{"1", EventSourceId::Type::kDbusObjects},
                       {"2", EventSourceId::Type::kDbusObjects}}));
              return absl::InternalError("");
            }));

    // This verifies we don't partially subscribe.
    EXPECT_CALL(*subscription_store_ptr_, AddNewSubscription(_)).Times(0);

    subscription_service_->CreateSubscription(
        request, {"Privilege123"},
        [](const absl::StatusOr<SubscriptionId>& subscription_id) {
          EXPECT_THAT(subscription_id, IsStatusInternal());
        },
        [](const std::string&) {});
  }

  // Backend->Subscribe returns unexpected responses.
  {
    EXPECT_CALL(*subscription_backend_ptr_,
                Subscribe(Eq("/redfish/v1/Chassis/Foo/Sensors/x"), _,
                          UnorderedElementsAre("Privilege123")))
        .WillOnce(Invoke(
            [](absl::string_view url,
               SubscriptionBackend::SubscribeCallback&& subscribe_callback,
               const std::unordered_set<std::string>& privileges) {
              subscribe_callback(
                  absl::OkStatus(),
                  std::vector<EventSourceId>(
                      {{"1", EventSourceId::Type::kDbusObjects},
                       {"2", EventSourceId::Type::kDbusObjects}}));
              subscribe_callback(
                  absl::OkStatus(),
                  std::vector<EventSourceId>(
                      {{"1", EventSourceId::Type::kDbusObjects},
                       {"2", EventSourceId::Type::kDbusObjects}}));
              return absl::OkStatus();
            }));

    EXPECT_CALL(*subscription_backend_ptr_,
                Subscribe(Eq("/redfish/v1/Chassis/Foo/Sensors/y"), _,
                          UnorderedElementsAre("Privilege123")))
        .WillOnce(Invoke(
            [](absl::string_view url,
               SubscriptionBackend::SubscribeCallback&& subscribe_callback,
               const std::unordered_set<std::string>& privileges) {
              subscribe_callback(
                  absl::OkStatus(),
                  std::vector<EventSourceId>(
                      {{"3", EventSourceId::Type::kDbusObjects}}));
              subscribe_callback(
                  absl::OkStatus(),
                  std::vector<EventSourceId>(
                      {{"3", EventSourceId::Type::kDbusObjects}}));
              return absl::OkStatus();
            }));

    // This verifies we don't partially subscribe.
    EXPECT_CALL(*subscription_store_ptr_, AddNewSubscription(_)).Times(0);

    subscription_service_->CreateSubscription(
        request, {"Privilege123"},
        [](const absl::StatusOr<SubscriptionId>& subscription_id) {
          EXPECT_THAT(subscription_id, IsStatusInternal());
        },
        [](const std::string&) {});
  }

  // Backend->Subscribe returns an error in callback.
  {
    EXPECT_CALL(*subscription_backend_ptr_,
                Subscribe(Eq("/redfish/v1/Chassis/Foo/Sensors/x"), _,
                          UnorderedElementsAre("Privilege123")))
        .WillOnce(Invoke(
            [](absl::string_view url,
               SubscriptionBackend::SubscribeCallback&& subscribe_callback,
               const std::unordered_set<std::string>& privileges) {
              subscribe_callback(
                  absl::OkStatus(),
                  std::vector<EventSourceId>(
                      {{"1", EventSourceId::Type::kDbusObjects},
                       {"2", EventSourceId::Type::kDbusObjects}}));
              return absl::OkStatus();
            }));
    EXPECT_CALL(*subscription_backend_ptr_,
                Subscribe(Eq("/redfish/v1/Chassis/Foo/Sensors/y"), _,
                          UnorderedElementsAre("Privilege123")))
        .WillOnce(Invoke(
            [](absl::string_view url,
               SubscriptionBackend::SubscribeCallback&& subscribe_callback,
               const std::unordered_set<std::string>& privileges) {
              subscribe_callback(
                  absl::InternalError(""),
                  std::vector<EventSourceId>(
                      {{"3", EventSourceId::Type::kDbusObjects}}));
              return absl::OkStatus();
            }));

    EXPECT_CALL(*subscription_store_ptr_, AddNewSubscription(_)).Times(0);

    subscription_service_->CreateSubscription(
        request, {"Privilege123"},
        [](const absl::StatusOr<SubscriptionId>& subscription_id) {
          EXPECT_THAT(subscription_id, IsStatusInternal());
        },
        [](const std::string&) {});
  }
}

TEST_F(SubscriptionServiceImplTest,
       ShouldNotCreateSubscriptionOnInvalidRequest) {
  // Create invalid subscription request with missing "Oem" field.
  static constexpr absl::string_view invalid_request_missing_oem = R"json(
    {
      "EventFormatType": "Event",
      "Protocol": "Redfish",
      "SubscriptionType": "OEM",
      "HeartbeatIntervalMinutes": 2,
      "IncludeOriginOfCondition": true,
      "DeliveryRetryPolicy": "SuspendRetries",
      "OEMSubscriptionType": "gRPC"
    }
  )json";

  // Create invalid subscription request with missing "Google" field.
  static constexpr absl::string_view invalid_request_missing_google = R"json(
    {
      "EventFormatType": "Event",
      "Protocol": "Redfish",
      "SubscriptionType": "OEM",
      "HeartbeatIntervalMinutes": 2,
      "IncludeOriginOfCondition": true,
      "DeliveryRetryPolicy": "SuspendRetries",
      "OEMSubscriptionType": "gRPC",
      "Oem":{
      }
    }
  )json";

  // Create invalid subscription request with missing "Triggers" field.
  static constexpr absl::string_view invalid_request_missing_triggers = R"json(
    {
      "EventFormatType": "Event",
      "Protocol": "Redfish",
      "SubscriptionType": "OEM",
      "HeartbeatIntervalMinutes": 2,
      "IncludeOriginOfCondition": true,
      "DeliveryRetryPolicy": "SuspendRetries",
      "OEMSubscriptionType": "gRPC",
      "Oem":{
        "Google": {
          "Triggers": [
          ]
        }
      }
    }
  )json";

  // Create invalid subscription request with missing "Id" field in triggers.
  static constexpr absl::string_view invalid_request_missing_id = R"json(
    {
      "EventFormatType": "Event",
      "Protocol": "Redfish",
      "SubscriptionType": "OEM",
      "HeartbeatIntervalMinutes": 2,
      "IncludeOriginOfCondition": true,
      "DeliveryRetryPolicy": "SuspendRetries",
      "OEMSubscriptionType": "gRPC",
      "Oem":{
        "Google": {
          "Triggers": [
            {
              "OriginResources":[
                {
                  "@odata.id": "/redfish/v1/Chassis/Foo/Sensors/x"
                },
                {
                  "@odata.id": "/redfish/v1/Chassis/Foo/Sensors/y"
                }
              ],
              "Predicate": "Reading>23"
            }
          ]
        }
      }
    }
  )json";

  // Create invalid subscription request with null trigger object.
  static constexpr absl::string_view invalid_request_null_trigger = R"json(
    {
      "EventFormatType": "Event",
      "Protocol": "Redfish",
      "SubscriptionType": "OEM",
      "HeartbeatIntervalMinutes": 2,
      "IncludeOriginOfCondition": true,
      "DeliveryRetryPolicy": "SuspendRetries",
      "OEMSubscriptionType": "gRPC",
      "Oem":{
        "Google": {
          "Triggers": [
            {
            }
          ]
        }
      }
    }
  )json";

  // Add all invalid requests in a vector to run tests on each
  std::vector<absl::string_view> invalid_requests = {
      invalid_request_missing_oem, invalid_request_missing_google,
      invalid_request_missing_triggers, invalid_request_missing_id,
      invalid_request_null_trigger};

  for (const auto& invalid_request : invalid_requests) {
    nlohmann::json request = nlohmann::json::parse(invalid_request);
    ASSERT_TRUE(!request.is_discarded());

    // Verify mock calls.
    EXPECT_CALL(*subscription_backend_ptr_,
                Subscribe(_, _, UnorderedElementsAre("Privilege123")))
        .Times(0);
    EXPECT_CALL(*subscription_store_ptr_, AddNewSubscription(_)).Times(0);
    // Expect subscription creation to fail.
    subscription_service_->CreateSubscription(
        request, {"Privilege123"},
        [](const absl::StatusOr<SubscriptionId>& subscription_id) {
          EXPECT_NE(subscription_id.status(), absl::OkStatus());
        },
        [](const std::string&) {});
  }
}

TEST_F(SubscriptionServiceImplTest, ShouldSendEventIfOriginOfConditionIsValid) {
  static constexpr absl::string_view mock_query_response = R"json(
    {
       "@odata.id": "/redfish/v1/Chassis/chassis/Sensors/current_cpu0_pvccd_hv_Output_Current",
        "@odata.type": "#Sensor.v1_2_0.Sensor",
        "Id": "Sensors_cpu0_pvccd_hv_Output_Current",
        "Name": "cpu0 pvccd hv Output Current",
        "Reading": 3.7,
        "ReadingRangeMax": 38.0,
        "ReadingRangeMin": -6.0,
        "ReadingType": "Current",
        "ReadingUnits": "A",
        "Status": {
            "Health": "OK",
            "State": "Enabled"
        },
        "Thresholds": {
            "LowerCritical": {
                "Reading": -5.0
            },
            "UpperCritical": {
                "Reading": 39.9
            }
        }
      }
  )json";

  static constexpr absl::string_view mock_trigger_str = R"json(
    {
      "Id": "123",
      "OriginResources":[
        {
          "@odata.id": "/redfish/v1/node1"
        },
        {
          "@odata.id": "/redfish/v1/node2"
        }
      ],
      "Predicate": "Reading>2"
    }
  )json";

  static constexpr absl::string_view expected_event_str = R"json(
    {
      "@odata.type": "#Event.v1_7_0.Event",
      "Events": [
        {
          "Context": "123",
          "OriginOfCondition": {
            "@odata.id": "/redfish/v1/Chassis/chassis/Sensors/current_cpu0_pvccd_hv_Output_Current",
            "@odata.type": "#Sensor.v1_2_0.Sensor",
            "Id": "Sensors_cpu0_pvccd_hv_Output_Current",
            "Name": "cpu0 pvccd hv Output Current",
            "Reading": 3.7,
            "ReadingRangeMax": 38,
            "ReadingRangeMin": -6,
            "ReadingType": "Current",
            "ReadingUnits": "A",
            "Status": {
              "Health": "OK",
              "State": "Enabled"
            },
            "Thresholds": {
              "LowerCritical": {
                "Reading": -5
              },
              "UpperCritical": {
                "Reading": 39.9
              }
            }
          }
        }
      ],
      "Name": "RedfishEvent"
    }
  )json";

  auto query_response = nlohmann::json::parse(mock_query_response);
  ASSERT_TRUE(!query_response.is_discarded());

  auto trigger = nlohmann::json::parse(mock_trigger_str);
  ASSERT_TRUE(!trigger.is_discarded());

  auto trigger_internal = Trigger::Create(trigger);
  ASSERT_TRUE(trigger_internal.ok());

  auto expected_event = nlohmann::json::parse(expected_event_str);
  ASSERT_TRUE(!expected_event.is_discarded());

  // Create event source ID
  EventSourceId event_source_id("1", EventSourceId::Type::kDbusObjects);

  absl::flat_hash_map<std::string, Trigger> test_id_to_triggers = {
      {"1", *trigger_internal}};

  int on_event_callback_count = 0;
  absl::flat_hash_map<EventSourceId, absl::flat_hash_set<std::string>>
      test_event_source_to_uris;
  test_event_source_to_uris[event_source_id] = {"/redfish/v1/node1"};
  const auto context = std::make_unique<SubscriptionContext>(
      SubscriptionId(1), test_event_source_to_uris, test_id_to_triggers,
      [&on_event_callback_count, expected_event](const nlohmann::json& event) {
        ++on_event_callback_count;

        // We want to ignore Id, EventId and EventTimestamp for comparison with
        // expected event since these are random hash values.
        // But we also want to make sure these fields are presents.
        // So we create a copy of the event and remove these fields after
        // checking they are present.
        nlohmann::json event_scrubbed = event;
        EXPECT_TRUE(event_scrubbed.contains("Id"));
        auto find_events = event_scrubbed.find("Events");
        ASSERT_TRUE(find_events != event_scrubbed.end());
        ASSERT_EQ(find_events->size(), 1);
        EXPECT_TRUE(find_events->begin()->contains("EventId"));
        EXPECT_TRUE(find_events->begin()->contains("EventTimestamp"));
        event_scrubbed.erase("Id");
        find_events->begin()->erase("EventId");
        find_events->begin()->erase("EventTimestamp");

        // Finally, compare the event with expected event.
        EXPECT_EQ(event_scrubbed, expected_event);
      });
  context->privileges = {"Privilege123"};
  std::vector<const SubscriptionContext*> contexts = {context.get()};
  // Expect subscription store to be queried for subscriptions.
  EXPECT_CALL(*subscription_store_ptr_,
              GetSubscriptionsByEventSourceId(event_source_id))
      .WillRepeatedly(
          Return(absl::Span<const SubscriptionContext* const>(contexts)));

  EXPECT_CALL(
      *subscription_backend_ptr_,
      Query(Eq("/redfish/v1/node1"), _, UnorderedElementsAre("Privilege123")))
      .WillOnce(Invoke(
          [query_response](absl::string_view url,
                           SubscriptionBackend::QueryCallback&& query_callback,
                           const std::unordered_set<std::string>& privileges) {
            query_callback(absl::OkStatus(), query_response);
            return absl::OkStatus();
          }))
      .WillOnce(Invoke(
          [query_response](absl::string_view url,
                           SubscriptionBackend::QueryCallback&& query_callback,
                           const std::unordered_set<std::string>& privileges) {
            query_callback(absl::InternalError(""), {});
            return absl::OkStatus();
          }))
      .WillOnce(Return(absl::OkStatus()));

  // Check that only 1 Redfish event is added to the store.
  EXPECT_CALL(*event_store_ptr_, AddNewEvent).Times(1);

  // 1. Origin of condition is built, event is sent to the subscriber.
  ASSERT_THAT(subscription_service_->Notify(event_source_id, absl::OkStatus()),
              absl::OkStatus());

  // 2. Event source returns an error on query, OriginOfCondition is not built
  // and event is not sent out.
  ASSERT_THAT(subscription_service_->Notify(event_source_id, absl::OkStatus()),
              absl::OkStatus());
  // 3. Event source does not complete query, OriginOfCondition is not built
  // and event is not sent out.
  ASSERT_THAT(subscription_service_->Notify(event_source_id, absl::OkStatus()),
              absl::OkStatus());

  // Verify event is sent only once in above 3 cases.
  EXPECT_EQ(on_event_callback_count, 1);
}

TEST_F(SubscriptionServiceImplTest, GetAllSubscriptions_ReturnsEmpty) {
  EXPECT_EQ(subscription_service_->GetAllSubscriptions().size(), 0);
}

TEST_F(SubscriptionServiceImplTest, NotifyWithData_ReturnsUnimplementedError) {
  EventSourceId event_source_id("1", EventSourceId::Type::kDbusObjects);
  EXPECT_EQ(subscription_service_->NotifyWithData(event_source_id,
                                                  absl::OkStatus(), {}),
            absl::UnimplementedError("NotifyWithData:: Unimplemented!"));
}

TEST_F(SubscriptionServiceImplTest, ShouldDispatchEventsAfterLastEventId) {
  static constexpr absl::string_view valid_request = R"json(
    {
      "EventFormatType": "Event",
      "Protocol": "Redfish",
      "SubscriptionType": "OEM",
      "HeartbeatIntervalMinutes": 2,
      "IncludeOriginOfCondition": true,
      "DeliveryRetryPolicy": "SuspendRetries",
      "OEMSubscriptionType": "gRPC",
      "Oem":{
        "Google": {
          "LastEventId": 4,
          "Triggers": [
            {
              "Id": "1",
              "OriginResources":[
                {
                  "@odata.id": "/redfish/v1/Chassis/Foo/Sensors/x"
                },
                {
                  "@odata.id": "/redfish/v1/Chassis/Foo/Sensors/y"
                }
              ],
              "Predicate": "Reading>23"
            }
          ]
        }
      }
    }
  )json";

  nlohmann::json request = nlohmann::json::parse(valid_request);
  ASSERT_TRUE(!request.is_discarded());

  nlohmann::json event1 = {{"key1", "value1"}};
  nlohmann::json event2 = {{"key2", "value2"}};

  EXPECT_CALL(*event_store_ptr_, GetEventsSince(std::optional<size_t>(4)))
      .WillOnce(Return(std::vector<nlohmann::json>{event1, event2}));

  EXPECT_CALL(*subscription_backend_ptr_,
              Subscribe(Eq("/redfish/v1/Chassis/Foo/Sensors/x"), _,
                        UnorderedElementsAre("Privilege123")))
      .WillOnce(Invoke([](absl::string_view url,
                          SubscriptionBackend::SubscribeCallback&& callback,
                          const std::unordered_set<std::string>& privileges) {
        callback(absl::OkStatus(),
                 std::vector<EventSourceId>(
                     {{"1", EventSourceId::Type::kDbusObjects},
                      {"2", EventSourceId::Type::kDbusObjects}}));
        return absl::OkStatus();
      }));

  EXPECT_CALL(*subscription_backend_ptr_,
              Subscribe(Eq("/redfish/v1/Chassis/Foo/Sensors/y"), _,
                        UnorderedElementsAre("Privilege123")))
      .WillOnce(Invoke([](absl::string_view url,
                          SubscriptionBackend::SubscribeCallback&& callback,
                          const std::unordered_set<std::string>& privileges) {
        callback(absl::OkStatus(),
                 std::vector<EventSourceId>(
                     {{"3", EventSourceId::Type::kDbusObjects}}));
        return absl::OkStatus();
      }));

  EXPECT_CALL(*subscription_store_ptr_, AddNewSubscription(_)).Times(1);

  std::vector<nlohmann::json> actual_events;
  size_t callback_count = 0;
  subscription_service_->CreateSubscription(
      request, {"Privilege123"},
      [&callback_count](const absl::StatusOr<SubscriptionId>& subscription_id) {
        ++callback_count;
        EXPECT_THAT(subscription_id, IsOk());
      },
      [&](const nlohmann::json& data) { actual_events.push_back(data); });

  EXPECT_THAT(actual_events, UnorderedElementsAre(event1, event2));
  EXPECT_EQ(callback_count, 1);
}

// TEST_F(SubscriptionServiceImplTest,
// EventsAreNotSentWhenPredicateDoesNotApply) {
//   static constexpr absl::string_view kMockQueryResponse = R"json(
//     {
//        "@odata.id":
//        "/redfish/v1/Chassis/chassis/Sensors/current_cpu0_pvccd_hv_Output_Current",
//         "@odata.type": "#Sensor.v1_2_0.Sensor",
//         "Id": "Sensors_cpu0_pvccd_hv_Output_Current",
//         "Name": "cpu0 pvccd hv Output Current",
//         "Reading": 3.7,
//         "ReadingRangeMax": 38.0,
//         "ReadingRangeMin": -6.0,
//         "ReadingType": "Current",
//         "ReadingUnits": "A",
//         "Status": {
//             "Health": "OK",
//             "State": "Enabled"
//         },
//         "Thresholds": {
//             "LowerCritical": {
//                 "Reading": -5.0
//             },
//             "UpperCritical": {
//                 "Reading": 39.9
//             }
//         }
//       }
//   )json";

//   static constexpr absl::string_view kMockTrigger = R"json(
//     {
//       "Id": "123",
//       "OriginResources":[
//         {
//           "@odata.id": "/redfish/v1/node1"
//         },
//         {
//           "@odata.id": "/redfish/v1/node2"
//         }
//       ],
//       "Predicate": "Reading>4"
//     }
//   )json";

//   auto query_response = nlohmann::json::parse(kMockQueryResponse);
//   ASSERT_TRUE(!query_response.is_discarded());

//   int on_event_callback_count = 0;

//   // Prepare Subscription context.
//   // 1. Create Trigger
//   auto trigger = nlohmann::json::parse(kMockTrigger);
//   ASSERT_TRUE(!trigger.is_discarded());
//   auto trigger_internal = Trigger::Create(trigger);
//   ASSERT_TRUE(trigger_internal.ok());

//   // 2. Create event source ID
//   EventSourceId event_source_id("1", EventSourceId::Type::kDbusObjects);
//   absl::flat_hash_map<std::string, Trigger> test_id_to_triggers = {
//       {"1", *trigger_internal}};
//   absl::flat_hash_map<EventSourceId, absl::flat_hash_set<std::string>>
//       test_event_source_to_uris;
//   test_event_source_to_uris[event_source_id] = {"/redfish/v1/node1"};
//   const auto context = std::make_unique<SubscriptionContext>(
//       SubscriptionId(1), test_event_source_to_uris, test_id_to_triggers,
//       [&on_event_callback_count](const nlohmann::json& event) {
//         ++on_event_callback_count;
//       });
//   context->privileges = {"Privilege123"};
//   std::vector<const SubscriptionContext*> contexts = {context.get()};

//   // Expect subscription store to be queried for subscriptions.
//   EXPECT_CALL(*subscription_store_ptr_,
//               GetSubscriptionsByEventSourceId(event_source_id))
//       .WillRepeatedly(
//           Return(absl::Span<const SubscriptionContext* const>(contexts)));

//   // Expect subscription backend to be queried for events.
//   EXPECT_CALL(
//       *subscription_backend_ptr_,
//       Query(Eq("/redfish/v1/node1"), _,
//       UnorderedElementsAre("Privilege123"))) .WillOnce(Invoke(
//           [query_response](absl::string_view url,
//                            SubscriptionBackend::QueryCallback&&
//                            query_callback, const
//                            std::unordered_set<std::string>& privileges) {
//             query_callback(absl::OkStatus(), query_response);
//             return absl::OkStatus();
//           }));
//   ASSERT_THAT(subscription_service_->Notify(event_source_id,
//   absl::OkStatus()),
//               absl::OkStatus());

//   // Verify event is not sent because predicate condition is not satisfied.
//   EXPECT_EQ(on_event_callback_count, 0);
// }

// TEST_F(SubscriptionServiceImplTest, EventsAreNotSentWhenPredicateIsInvalid) {
//   static constexpr absl::string_view mock_trigger_str = R"json(
//     {
//       "Id": "123",
//       "OriginResources":[
//         {
//           "@odata.id": "/redfish/v1/node1"
//         },
//         {
//           "@odata.id": "/redfish/v1/node2"
//         }
//       ],
//       "Predicate": "Reading>"
//     }
//   )json";

//   int on_event_callback_count = 0;

//   // Prepare Subscription context.
//   // 1. Create Trigger
//   auto trigger = nlohmann::json::parse(mock_trigger_str);
//   ASSERT_TRUE(!trigger.is_discarded());
//   auto trigger_internal = Trigger::Create(trigger);
//   ASSERT_TRUE(trigger_internal.ok());

//   // 2. Create event source ID
//   EventSourceId event_source_id("1", EventSourceId::Type::kDbusObjects);
//   absl::flat_hash_map<std::string, Trigger> test_id_to_triggers = {
//       {"1", *trigger_internal}};
//   absl::flat_hash_map<EventSourceId, absl::flat_hash_set<std::string>>
//       test_event_source_to_uris;
//   test_event_source_to_uris[event_source_id] = {"/redfish/v1/node1"};
//   const auto context = std::make_unique<SubscriptionContext>(
//       SubscriptionId(1), test_event_source_to_uris, test_id_to_triggers,
//       [&on_event_callback_count](const nlohmann::json& event) {
//         ++on_event_callback_count;
//       });
//   std::vector<const SubscriptionContext*> contexts = {context.get()};

//   // Expect subscription store to be queried for subscriptions.
//   EXPECT_CALL(*subscription_store_ptr_,
//               GetSubscriptionsByEventSourceId(event_source_id))
//       .WillRepeatedly(
//           Return(absl::Span<const SubscriptionContext* const>(contexts)));

//   // Expect subscription backend to be queried for events.
//   EXPECT_CALL(*subscription_backend_ptr_, Query(Eq("/redfish/v1/node1"), _,
//   _))
//       .WillOnce(Invoke([](absl::string_view url,
//                           SubscriptionBackend::QueryCallback&&
//                           query_callback, const
//                           std::unordered_set<std::string>& privileges) {
//         query_callback(absl::OkStatus(), {});
//         return absl::OkStatus();
//       }));
//   ASSERT_THAT(subscription_service_->Notify(event_source_id,
//   absl::OkStatus()),
//               absl::OkStatus());

//   // Verify event is not sent because predicate condition is not satisfied.
//   EXPECT_EQ(on_event_callback_count, 0);
// }

}  // namespace

}  // namespace ecclesia
