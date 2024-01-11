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

#include "ecclesia/lib/redfish/event/server/subscription.h"

#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {

namespace {

TEST(EventSourceIdTest, Constructor) {
  EventSourceId source_id(123, EventSourceId::Type::kDbusObjects);
  EXPECT_EQ(source_id.key, 123);
  EXPECT_EQ(source_id.type, EventSourceId::Type::kDbusObjects);
}

TEST(EventSourceIdTest, ToJSON) {
  EventSourceId event_source_id(123, EventSourceId::Type::kDbusObjects);
  nlohmann::json json = event_source_id.ToJSON();

  EXPECT_EQ(json["key"], 123);
  EXPECT_EQ(json["type"], "kDbusObjects");
}

TEST(EventSourceIdTest, ToString) {
  EventSourceId source_id(123, EventSourceId::Type::kSocketIO);
  std::string str = source_id.ToString();

  std::string expected_string = "{\"key\":123,\"type\":\"kSocketIO\"}";
  EXPECT_EQ(str, expected_string);
}

TEST(EventSourceIdTest, Comparison) {
  EventSourceId source_id1(123, EventSourceId::Type::kDbusObjects);
  EventSourceId source_id2(123, EventSourceId::Type::kDbusObjects);
  EventSourceId source_id3(456, EventSourceId::Type::kSocketIO);

  EXPECT_TRUE(source_id1 == source_id2);
  EXPECT_FALSE(source_id1 == source_id3);
  EXPECT_FALSE(source_id1 != source_id2);
  EXPECT_TRUE(source_id1 != source_id3);
}

TEST(EventIdTest, Constructor) {
  SubscriptionId subscription_id(123);
  EventSourceId source_id(456, EventSourceId::Type::kDbusObjects);
  absl::Time timestamp = absl::Now();

  EventId event_id(subscription_id, source_id, timestamp);
  EXPECT_EQ(event_id.subscription_id, subscription_id);
  EXPECT_EQ(event_id.source_id, source_id);
  EXPECT_EQ(event_id.timestamp, timestamp);
}

TEST(EventIdTest, UniqueUuid) {
  SubscriptionId subscription_id(123);
  EventSourceId source_id(456, EventSourceId::Type::kSocketIO);
  absl::Time timestamp1 = absl::Now();
  absl::Time timestamp2 = timestamp1 + absl::Seconds(1);

  EventId event_id1(subscription_id, source_id, timestamp1);
  EventId event_id2(subscription_id, source_id, timestamp2);

  EXPECT_NE(event_id1.redfish_event_id, event_id2.redfish_event_id);
}

TEST(EventIdTest, ToJSON) {
  SubscriptionId subscription_id(123);
  EventSourceId source_id(456, EventSourceId::Type::kFileIO);
  absl::Time timestamp = absl::Now();

  EventId event_id(subscription_id, source_id, timestamp);
  nlohmann::json json = event_id.ToJSON();

  EXPECT_EQ(json["source_id"], source_id.ToJSON());
  EXPECT_EQ(json["timestamp"], absl::FormatTime(absl::RFC3339_full, timestamp,
                                                absl::UTCTimeZone()));
  EXPECT_EQ(json["uuid"], event_id.redfish_event_id);
}

TEST(EventIdTest, ToString) {
  SubscriptionId subscription_id(123);
  EventSourceId source_id(456, EventSourceId::Type::kDbusObjects);
  absl::Time timestamp = absl::Now();

  EventId event_id(subscription_id, source_id, timestamp);
  std::string output_str = event_id.ToString();

  std::string timestamp_string =
      absl::FormatTime(absl::RFC3339_full, timestamp, absl::UTCTimeZone());

  std::string uuid_string = absl::StrCat(event_id.redfish_event_id);

  std::string expected_string =
      "{\"source_id\":{\"key\":456,\"type\":\"kDbusObjects\"},\"timestamp\":"
      "\"" +
      timestamp_string + "\",\"uuid\":" + uuid_string + "}";
  EXPECT_EQ(output_str, expected_string);
}

TEST(SubscriptionServiceTriggerTest, Create_InvalidJSON) {
  nlohmann::json invalid_json;

  auto trigger_or_status = Trigger::Create(invalid_json);
  EXPECT_TRUE(absl::IsInvalidArgument(trigger_or_status.status()));
}

TEST(TriggerTest, CreateFromInvalidJson) {
  // Missing origin resources
  const std::string invalid_json_1 =
      R"({"predicate": "some predicate", "mask": false})";
  EXPECT_TRUE(absl::IsInvalidArgument(
      Trigger::Create(nlohmann::json::parse(invalid_json_1)).status()));

  // Invalid field type
  const std::string invalid_json_2 =
      R"({"origin_resources": 123, "predicate": "some predicate", "mask": false})";
  EXPECT_TRUE(absl::IsInvalidArgument(
      Trigger::Create(nlohmann::json::parse(invalid_json_2)).status()));

  // Missing id
  const std::string invalid_json_3 =
      R"({"origin_resources":[{"@odata.id": "/redfish/v1/node1"},{"@odata.id": "/redfish/v1/node2"}], "predicate": "some predicate", "mask": false})";
  EXPECT_TRUE(absl::IsInvalidArgument(
      Trigger::Create(nlohmann::json::parse(invalid_json_3)).status()));
}

TEST(SubscriptionServiceTriggerTest, Create_ValidTrigger) {
  static constexpr absl::string_view raw_data = R"json(
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

  auto json = nlohmann::json::parse(raw_data);
  ASSERT_TRUE(!json.is_discarded());
  auto trigger_or_status = Trigger::Create(json);
  ASSERT_TRUE(trigger_or_status.ok());

  const Trigger& trigger = trigger_or_status.value();
  EXPECT_EQ(
      trigger.origin_resources,
      std::vector<std::string>({"/redfish/v1/node1", "/redfish/v1/node2"}));
  EXPECT_EQ(trigger.predicate, "");
  EXPECT_TRUE(trigger.mask);
}

TEST(SubscriptionServiceTriggerTest, Create_WithPredicate) {
  static constexpr absl::string_view raw_data = R"json(
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
      "Predicate": "Value>23"
    }
  )json";
  auto json = nlohmann::json::parse(raw_data);
  ASSERT_TRUE(!json.is_discarded());

  auto trigger_or_status = Trigger::Create(json);
  ASSERT_TRUE(trigger_or_status.ok());

  const Trigger& trigger = trigger_or_status.value();
  EXPECT_EQ(trigger.predicate, "Value>23");
}

TEST(SubscriptionServiceTriggerTest, ToJSON) {
  static constexpr absl::string_view raw_data = R"json(
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
      "Predicate": "Value>23"
    }
  )json";

  auto json_in = nlohmann::json::parse(raw_data);
  ASSERT_TRUE(!json_in.is_discarded());
  auto trigger_or_status = Trigger::Create(json_in);

  EventSourceId source_id(123, EventSourceId::Type::kDbusObjects);
  trigger_or_status->event_source_to_uri[source_id] = {"/redfish/v1/node1",
                                                       "/redfish/v1/node2"};

  nlohmann::json json = trigger_or_status->ToJSON();

  EXPECT_EQ(json["origin_resources"].size(), 2);
  EXPECT_EQ(json["origin_resources"][0], "/redfish/v1/node1");
  EXPECT_EQ(json["origin_resources"][1], "/redfish/v1/node2");
  EXPECT_EQ(json["predicate"], "Value>23");
  EXPECT_EQ(json["mask"], false);
  EXPECT_TRUE(json["event_source_to_uri"].is_array());
  EXPECT_EQ(json["event_source_to_uri"].size(), 1);
  EXPECT_EQ(json["event_source_to_uri"][0]["event_source"]["key"], 123);
  EXPECT_EQ(json["event_source_to_uri"][0]["event_source"]["type"],
            "kDbusObjects");
  EXPECT_EQ(json["event_source_to_uri"][0]["uris"][0], "/redfish/v1/node1");
  EXPECT_EQ(json["event_source_to_uri"][0]["uris"][1], "/redfish/v1/node2");
}

TEST(SubscriptionServiceTriggerTest, ToString) {
  static constexpr absl::string_view raw_data = R"json(
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
      "Predicate": "Value>23"
    }
  )json";

  auto json_in = nlohmann::json::parse(raw_data);
  ASSERT_TRUE(!json_in.is_discarded());
  auto trigger_or_status = Trigger::Create(json_in);

  std::string output_str = trigger_or_status->ToString();
  std::string expected_str =
      "{\"event_source_to_uri\":[],\"mask\":false,\"origin_resources\":[\"/"
      "redfish/v1/node1\",\"/redfish/v1/node2\"],\"predicate\":\"Value>23\"}";

  EXPECT_EQ(output_str, expected_str);
}

}  // namespace

}  // namespace ecclesia
