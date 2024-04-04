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

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/functional/function_ref.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/testing/json_mockup.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/testing/status.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {

namespace {

using Configuration = RedPathSubscription::Configuration;
using EventContext = RedPathSubscription::EventContext;

// Mock RedfishEventStream.
class MockRedfishEventStream : public RedfishEventStream {
 public:
  MOCK_METHOD(void, StartStreaming, (), (override));
  MOCK_METHOD(void, CancelStreaming, (), (override));
};

class MockRedfishClientInterface : public NullRedfish {
 public:
  MOCK_METHOD(absl::StatusOr<std::unique_ptr<RedfishEventStream>>, Subscribe,
              (absl::string_view data,
               absl::FunctionRef<void(const RedfishVariant &event)> on_event,
               absl::FunctionRef<void(const absl::Status &end_status)> on_stop),
              (override));
};

using ::testing::Return;

TEST(RedPathSubscriptionImplTest, SubscriptionIsCreated) {
  MockRedfishClientInterface redfish_interface;
  Configuration config;
  config.uris = {"bar", "baz"};
  config.redpath = "foo";
  config.query_id = "id";

  EXPECT_CALL(redfish_interface, Subscribe)
      .WillOnce([&](absl::string_view,
                    absl::FunctionRef<void(const RedfishVariant &event)>,
                    absl::FunctionRef<void(const absl::Status &end_status)>) {
        return nullptr;
      });
  absl::StatusOr<std::unique_ptr<RedPathSubscriptionImpl>>
      redpath_subscription = RedPathSubscriptionImpl::Create(
          {config}, redfish_interface,
          [](const RedfishVariant &variant, const EventContext &context) {},
          [](const absl::Status &status) {});
  EXPECT_THAT(redpath_subscription, IsOk());
}

TEST(RedPathSubscriptionImplTest, CancelSubscriptionReturnsOk) {
  MockRedfishClientInterface redfish_interface;
  Configuration config;
  config.uris = {"bar", "baz"};
  config.redpath = "foo";
  config.query_id = "id";

  auto mock_event_stream = std::make_unique<MockRedfishEventStream>();
  MockRedfishEventStream *mock_event_stream_raw = mock_event_stream.get();

  EXPECT_CALL(*mock_event_stream_raw, CancelStreaming()).WillOnce(Return());
  EXPECT_CALL(redfish_interface, Subscribe)
      .WillOnce([&](absl::string_view,
                    absl::FunctionRef<void(const RedfishVariant &event)>,
                    absl::FunctionRef<void(const absl::Status &end_status)>) {
        return std::move(mock_event_stream);
      });
  absl::StatusOr<std::unique_ptr<RedPathSubscriptionImpl>>
      redpath_subscription = RedPathSubscriptionImpl::Create(
          {config}, redfish_interface,
          [](const RedfishVariant &variant, const EventContext &context) {},
          [](const absl::Status &status) {});
  ASSERT_THAT(redpath_subscription, IsOk());
  EXPECT_THAT((*redpath_subscription)->CancelSubscription(), IsOk());
}

TEST(RedPathSubscriptionImplTest, SubscriptionIsNotCreated) {
  MockRedfishClientInterface redfish_interface;

  // When RedfishInterface::Subscribe returns error response.
  {
    Configuration config;
    config.uris = {"bar", "baz"};
    config.redpath = "foo";
    config.query_id = "id";
    EXPECT_CALL(redfish_interface, Subscribe)
        .WillOnce([&](absl::string_view,
                      absl::FunctionRef<void(const RedfishVariant &event)>,
                      absl::FunctionRef<void(const absl::Status &end_status)>) {
          return absl::InternalError("");
        });
    absl::StatusOr<std::unique_ptr<RedPathSubscriptionImpl>>
        redpath_subscription = RedPathSubscriptionImpl::Create(
            {config}, redfish_interface,
            [](const RedfishVariant &variant, const EventContext &context) {},
            [](const absl::Status &status) {});
    EXPECT_THAT(redpath_subscription, IsStatusInternal());
  }

  // When URIs are not present.
  {
    Configuration config;
    config.redpath = "foo";
    config.query_id = "id";
    absl::StatusOr<std::unique_ptr<RedPathSubscriptionImpl>>
        redpath_subscription = RedPathSubscriptionImpl::Create(
            {config}, redfish_interface,
            [](const RedfishVariant &variant, const EventContext &context) {},
            [](const absl::Status &status) {});
    EXPECT_THAT(redpath_subscription, IsStatusInvalidArgument());
  }

  // When subquery_id is not present.
  {
    Configuration config;
    config.uris = {"bar", "baz"};
    config.query_id = "id";
    absl::StatusOr<std::unique_ptr<RedPathSubscriptionImpl>>
        redpath_subscription = RedPathSubscriptionImpl::Create(
            {config}, redfish_interface,
            [](const RedfishVariant &variant, const EventContext &context) {},
            [](const absl::Status &status) {});
    EXPECT_THAT(redpath_subscription, IsStatusInvalidArgument());
  }

  // When query_id is not present.
  {
    Configuration config;
    config.uris = {"bar", "baz"};
    config.redpath = "foo";
    absl::StatusOr<std::unique_ptr<RedPathSubscriptionImpl>>
        redpath_subscription = RedPathSubscriptionImpl::Create(
            {config}, redfish_interface,
            [](const RedfishVariant &variant, const EventContext &context) {},
            [](const absl::Status &status) {});
    EXPECT_THAT(redpath_subscription, IsStatusInvalidArgument());
  }
}

TEST(RedPathSubscriptionImplTest, SubscriptionRequestFormattedCorrectly) {
  MockRedfishClientInterface redfish_interface;
  Configuration config;
  config.uris = {"bar", "baz"};
  config.redpath = "foo";
  config.query_id = "id";
  config.predicate = "Reading>10";

  // Subscription request we expect to send out to RedfishInterface.
  nlohmann::json expected_json = nlohmann::json::parse(R"json(
    {
      "DeliveryRetryPolicy": "SuspendRetries",
      "EventFormatType": "Event",
      "IncludeOriginOfCondition": true,
      "OEMSubscriptionType": "gRPC",
      "Oem": {
        "Google": {
          "Triggers": [
            {
              "Id": "id,foo",
              "OriginResources": [
                {
                  "@odata.id": "bar"
                },
                {
                  "@odata.id": "baz"
                }
              ],
              "Predicate": "Reading>10"
            }
          ]
        }
      },
      "Protocol": "Redfish",
      "SubscriptionType": "Oem"
    }
  )json");

  EXPECT_CALL(redfish_interface, Subscribe)
      .WillOnce(
          [&](absl::string_view data,
              absl::FunctionRef<void(const RedfishVariant &event)> on_event,
              absl::FunctionRef<void(const absl::Status &end_status)> on_stop) {
            nlohmann::json actual_json = nlohmann::json::parse(data);
            EXPECT_EQ(actual_json, expected_json);
            return nullptr;
          });
  absl::StatusOr<std::unique_ptr<RedPathSubscriptionImpl>>
      redpath_subscription = RedPathSubscriptionImpl::Create(
          {config}, redfish_interface,
          [](const RedfishVariant &variant, const EventContext &context) {},
          [](const absl::Status &status) {});
  EXPECT_THAT(redpath_subscription, IsOk());
}

TEST(RedPathSubscriptionImplTest, CallbackIsInvokedForEachOriginOfCondition) {
  std::unique_ptr<ecclesia::RedfishInterface> json_intf =
      ecclesia::NewJsonMockupInterface(R"json(
    {
      "@odata.id": "ODATA_ID_1",
      "Events": [
        {
          "Context": "Foo,Bar",
          "EventId": "ABC132489713478812346",
          "EventTimestamp": "2019-08-22T10:35:16+06:00",
          "OriginOfCondition": {
            "@odata.id": "ODATA_ID_1"
          }
        },
        {
          "Context": "Foo,Bar2",
          "EventId": "ABC132489713478812347",
          "EventTimestamp": "2019-08-22T10:35:16+06:00",
          "OriginOfCondition": {
            "@odata.id": "ODATA_ID_2"
          }
        }
      ]
    }
  )json");
  ecclesia::RedfishVariant event = json_intf->GetRoot();

  MockRedfishClientInterface redfish_interface;
  Configuration config;
  config.uris = {"bar", "baz"};
  config.redpath = "foo";
  config.query_id = "id";
  EXPECT_CALL(redfish_interface, Subscribe)
      .WillOnce(
          [&](absl::string_view data,
              absl::FunctionRef<void(const RedfishVariant &event)> on_event,
              absl::FunctionRef<void(const absl::Status &end_status)> on_stop) {
            on_event(event);
            return nullptr;
          });
  size_t callback_invoked = 0;
  absl::StatusOr<std::unique_ptr<RedPathSubscriptionImpl>>
      redpath_subscription = RedPathSubscriptionImpl::Create(
          {config}, redfish_interface,
          [&](const RedfishVariant &variant, const EventContext &context) {
            ++callback_invoked;
            EXPECT_THAT(variant.status(), IsOk());
          },
          [](const absl::Status &status) {});
  ASSERT_THAT(redpath_subscription, IsOk());
  EXPECT_EQ(callback_invoked, 2);
}

// Test Callback is not invoked on invalid event.
TEST(RedPathSubscriptionImplTest, CallbackIsNotInvokedOnInvalidEvent) {
  MockRedfishClientInterface redfish_interface;
  Configuration config;
  config.uris = {"bar", "baz"};
  config.redpath = "foo";
  config.query_id = "id";

  // When `Context` is missing in event.
  {
    std::unique_ptr<ecclesia::RedfishInterface> json_intf =
        ecclesia::NewJsonMockupInterface(R"json(
      {
        "@odata.id": "ODATA_ID_1",
        "Events": [
          {
            "OriginOfCondition": {
              "@odata.id": "ODATA_ID_1"
            }
          }
        ]
      }
    )json");
    ecclesia::RedfishVariant event = json_intf->GetRoot();
    EXPECT_CALL(redfish_interface, Subscribe)
        .WillOnce(
            [&](absl::string_view data,
                absl::FunctionRef<void(const RedfishVariant &event)> on_event,
                absl::FunctionRef<void(const absl::Status &end_status)>
                    on_stop) {
              on_event(event);
              return nullptr;
            });
    size_t callback_invoked = 0;
    absl::StatusOr<std::unique_ptr<RedPathSubscriptionImpl>>
        redpath_subscription = RedPathSubscriptionImpl::Create(
            {config}, redfish_interface,
            [&](const RedfishVariant &variant, const EventContext &context) {
              ++callback_invoked;
            },
            [](const absl::Status &status) {});
    ASSERT_THAT(redpath_subscription, IsOk());
    EXPECT_EQ(callback_invoked, 0);
  }

  // When `EventId` is missing in event.
  {
    std::unique_ptr<ecclesia::RedfishInterface> json_intf =
        ecclesia::NewJsonMockupInterface(R"json(
      {
        "@odata.id": "ODATA_ID_1",
        "Events": [
          {
            "Context": "Foo,Bar2",
            "OriginOfCondition": {
              "@odata.id": "ODATA_ID_1"
            }
          }
        ]
      }
    )json");
    ecclesia::RedfishVariant event = json_intf->GetRoot();
    EXPECT_CALL(redfish_interface, Subscribe)
        .WillOnce(
            [&](absl::string_view data,
                absl::FunctionRef<void(const RedfishVariant &event)> on_event,
                absl::FunctionRef<void(const absl::Status &end_status)>
                    on_stop) {
              on_event(event);
              return nullptr;
            });
    size_t callback_invoked = 0;
    absl::StatusOr<std::unique_ptr<RedPathSubscriptionImpl>>
        redpath_subscription = RedPathSubscriptionImpl::Create(
            {config}, redfish_interface,
            [&](const RedfishVariant &variant, const EventContext &context) {
              ++callback_invoked;
            },
            [](const absl::Status &status) {});
    ASSERT_THAT(redpath_subscription, IsOk());
    EXPECT_EQ(callback_invoked, 0);
  }

  // When `EventTimestamp` is missing in event.
  {
    std::unique_ptr<ecclesia::RedfishInterface> json_intf =
        ecclesia::NewJsonMockupInterface(R"json(
      {
        "@odata.id": "ODATA_ID_1",
        "Events": [
          {
            "Context": "Foo,Bar2",
            "EventId": "ABC132489713478812347",
            "OriginOfCondition": {
              "@odata.id": "ODATA_ID_1"
            }
          }
        ]
      }
    )json");
    ecclesia::RedfishVariant event = json_intf->GetRoot();
    EXPECT_CALL(redfish_interface, Subscribe)
        .WillOnce(
            [&](absl::string_view data,
                absl::FunctionRef<void(const RedfishVariant &event)> on_event,
                absl::FunctionRef<void(const absl::Status &end_status)>
                    on_stop) {
              on_event(event);
              return nullptr;
            });
    size_t callback_invoked = 0;
    absl::StatusOr<std::unique_ptr<RedPathSubscriptionImpl>>
        redpath_subscription = RedPathSubscriptionImpl::Create(
            {config}, redfish_interface,
            [&](const RedfishVariant &variant, const EventContext &context) {
              ++callback_invoked;
            },
            [](const absl::Status &status) {});
    ASSERT_THAT(redpath_subscription, IsOk());
    EXPECT_EQ(callback_invoked, 0);
  }

  // When `Events` is not in array object.
  {
    std::unique_ptr<ecclesia::RedfishInterface> json_intf =
        ecclesia::NewJsonMockupInterface(R"json(
      {
        "@odata.id": "ODATA_ID_1",
        "Events": {
          "OriginOfCondition": {
            "@odata.id": "ODATA_ID_1"
          }
        }
      }
    )json");
    ecclesia::RedfishVariant event = json_intf->GetRoot();
    EXPECT_CALL(redfish_interface, Subscribe)
        .WillOnce(
            [&](absl::string_view data,
                absl::FunctionRef<void(const RedfishVariant &event)> on_event,
                absl::FunctionRef<void(const absl::Status &end_status)>
                    on_stop) {
              on_event(event);
              return nullptr;
            });
    size_t callback_invoked = 0;
    absl::StatusOr<std::unique_ptr<RedPathSubscriptionImpl>>
        redpath_subscription = RedPathSubscriptionImpl::Create(
            {config}, redfish_interface,
            [&](const RedfishVariant &variant, const EventContext &context) {
              ++callback_invoked;
            },
            [](const absl::Status &status) {});
    ASSERT_THAT(redpath_subscription, IsOk());
    EXPECT_EQ(callback_invoked, 0);
  }

  // When `OriginOfCondition` is not found.
  {
    std::unique_ptr<ecclesia::RedfishInterface> json_intf =
        ecclesia::NewJsonMockupInterface(R"json(
      {
        "@odata.id": "ODATA_ID_1",
        "Events": [
          {
            "NoOriginOfCondition": {
              "@odata.id": "ODATA_ID_1"
            }
          }
        ]
      }
    )json");
    ecclesia::RedfishVariant event = json_intf->GetRoot();
    EXPECT_CALL(redfish_interface, Subscribe)
        .WillOnce(
            [&](absl::string_view data,
                absl::FunctionRef<void(const RedfishVariant &event)> on_event,
                absl::FunctionRef<void(const absl::Status &end_status)>
                    on_stop) {
              on_event(event);
              return nullptr;
            });
    size_t callback_invoked = 0;
    absl::StatusOr<std::unique_ptr<RedPathSubscriptionImpl>>
        redpath_subscription = RedPathSubscriptionImpl::Create(
            {config}, redfish_interface,
            [&](const RedfishVariant &variant, const EventContext &context) {
              ++callback_invoked;
            },
            [](const absl::Status &status) {});
    ASSERT_THAT(redpath_subscription, IsOk());
    EXPECT_EQ(callback_invoked, 0);
  }
}

// Test Callback is invoked on stop.
TEST(RedPathSubscriptionImplTest, CallbackIsInvokedOnStop) {
  MockRedfishClientInterface redfish_interface;
  Configuration config;
  config.uris = {"bar", "baz"};
  config.redpath = "foo";
  config.query_id = "id";

  EXPECT_CALL(redfish_interface, Subscribe)
      .WillOnce(
          [&](absl::string_view,
              absl::FunctionRef<void(const RedfishVariant &event)>,
              absl::FunctionRef<void(const absl::Status &end_status)> on_stop) {
            on_stop(absl::OkStatus());
            return nullptr;
          });
  bool callback_invoked = false;
  absl::StatusOr<std::unique_ptr<RedPathSubscriptionImpl>>
      redpath_subscription = RedPathSubscriptionImpl::Create(
          {config}, redfish_interface,
          [&](const RedfishVariant &variant, const EventContext &context) {},
          [&](const absl::Status &status) {
            callback_invoked = true;
            EXPECT_THAT(status, IsOk());
          });
  ASSERT_THAT(redpath_subscription, IsOk());
  EXPECT_TRUE(callback_invoked);
}

}  // namespace

}  // namespace ecclesia
