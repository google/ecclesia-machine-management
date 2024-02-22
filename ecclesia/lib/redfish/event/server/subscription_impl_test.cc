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
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ecclesia/lib/redfish/event/server/subscription.h"
#include "ecclesia/lib/redfish/event/server/subscription_mock.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {

namespace {

using ::testing::_;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::InvokeArgument;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

bool CompareJson(const nlohmann::json& json1, const nlohmann::json& json2,
                 const std::vector<std::string>& excludedFields = {}) {
  // Use the diff function to efficiently compare objects without modifying them
  auto diff = nlohmann::json::diff(json1, json2);

  // Check if any differences exist, excluding those in excludedFields
  return std::none_of(
      diff.begin(), diff.end(), [&excludedFields](const nlohmann::json& patch) {
        // Ignore "remove" operations for excluded fields
        return patch["op"] == "remove" &&
               std::find(excludedFields.begin(), excludedFields.end(),
                         patch["path"].back()) != excludedFields.end();
      });
}

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

  nlohmann::json request = nlohmann::json::parse(valid_request);
  ASSERT_TRUE(!request.is_discarded());

  EXPECT_CALL(*subscription_backend_ptr_,
              Subscribe(Eq("/redfish/v1/Chassis/Foo/Sensors/x")))
      .WillOnce(Return(std::vector<EventSourceId>(
          {{1, EventSourceId::Type::kDbusObjects},
           {2, EventSourceId::Type::kDbusObjects}})));
  EXPECT_CALL(*subscription_backend_ptr_,
              Subscribe(Eq("/redfish/v1/Chassis/Foo/Sensors/y")))
      .WillOnce(Return(std::vector<EventSourceId>(
          {{3, EventSourceId::Type::kDbusObjects}})));

  EXPECT_CALL(*subscription_store_ptr_, AddNewSubscription(_)).Times(1);

  // Expect subscription creation to succeed.
  EXPECT_THAT(subscription_service_
                  ->CreateSubscription(request, [](const std::string&) {})
                  .status(),
              absl::OkStatus());
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
    EXPECT_CALL(*subscription_backend_ptr_, Subscribe(_)).Times(0);
    EXPECT_CALL(*subscription_store_ptr_, AddNewSubscription(_)).Times(0);
    // Expect subscription creation to fail.
    EXPECT_NE(subscription_service_
                  ->CreateSubscription(request, [](const std::string&) {})
                  .status(),
              absl::OkStatus());
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
      ]
    }
  )json";

  static constexpr absl::string_view expected_event_str = R"json(
    {
      "@odata.type": "#Event.v1_7_0.Event",
      "Events": [
        {
          "EventId": 1926226176603338800,
          "EventTimestamp": "2023-12-08T01:28:44.097510308+00:00",
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
      "Id": 6794752704039439000,
      "Name": "RedfishEvent"
    }
  )json";

  auto query_response = nlohmann::json::parse(mock_query_response);
  ASSERT_TRUE(!query_response.is_discarded());

  auto trigger = nlohmann::json::parse(mock_trigger_str);
  ASSERT_TRUE(!trigger.is_discarded());

  auto trigger_or_status = Trigger::Create(trigger);
  ASSERT_TRUE(trigger_or_status.ok());

  auto expected_event = nlohmann::json::parse(expected_event_str);
  ASSERT_TRUE(!expected_event.is_discarded());

  // Create event source ID
  EventSourceId event_source_id(1, EventSourceId::Type::kDbusObjects);
  trigger_or_status->event_source_to_uri.insert(
      {event_source_id, {"/redfish/v1/node1"}});

  absl::flat_hash_map<std::string, Trigger> test_id_to_triggers = {
      {"1", *trigger_or_status}};

  int on_event_callback_count = 0;
  const auto context = std::make_unique<SubscriptionContext>(
      SubscriptionId(1), test_id_to_triggers,
      [&on_event_callback_count, expected_event](const nlohmann::json& event) {
        ++on_event_callback_count;
        EXPECT_TRUE(CompareJson(event, expected_event,
                                {"EventId", "EventTimestamp", "Id"}));
      });

  std::vector<const SubscriptionContext*> contexts = {context.get()};
  // Expect subscription store to be queried for subscriptions.
  EXPECT_CALL(*subscription_store_ptr_,
              GetSubscriptionsByEventSourceId(event_source_id))
      .WillRepeatedly(
          Return(absl::Span<const SubscriptionContext* const>(contexts)));

  EXPECT_CALL(*subscription_backend_ptr_, Query(Eq("/redfish/v1/node1"), _))
      .WillOnce(DoAll(InvokeArgument<1>(absl::OkStatus(), query_response),
                      Return(absl::OkStatus())))
      .WillOnce(
          DoAll(InvokeArgument<1>(absl::InternalError(""), query_response),
                Return(absl::OkStatus())))
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
  EventSourceId event_source_id(1, EventSourceId::Type::kDbusObjects);
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
              Subscribe(Eq("/redfish/v1/Chassis/Foo/Sensors/x")))
      .WillOnce(Return(std::vector<EventSourceId>(
          {{/*key_in=*/1, EventSourceId::Type::kDbusObjects},
           {/*key_in=*/2, EventSourceId::Type::kDbusObjects}})));
  EXPECT_CALL(*subscription_backend_ptr_,
              Subscribe(Eq("/redfish/v1/Chassis/Foo/Sensors/y")))
      .WillOnce(Return(std::vector<EventSourceId>(
          {{3, EventSourceId::Type::kDbusObjects}})));

  std::vector<nlohmann::json> actual_events;
  EXPECT_THAT(subscription_service_
                  ->CreateSubscription(request,
                                       [&](const nlohmann::json& data) {
                                         actual_events.push_back(data);
                                       })
                  .status(),
              absl::OkStatus());

  EXPECT_THAT(actual_events, UnorderedElementsAre(event1, event2));
}

}  // namespace

}  // namespace ecclesia
