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

#include "ecclesia/lib/redfish/event/server/event_store.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "ecclesia/lib/redfish/event/server/subscription.h"
#include "ecclesia/lib/redfish/event/server/subscription_mock.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {

namespace {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::SizeIs;

constexpr size_t kEventStoreSize = 3;

TEST(EventStoreImplTest, ShouldReturnAddedEvents) {
  // Create event store
  std::unique_ptr<EventStore> event_store = CreateEventStore(kEventStoreSize);

  nlohmann::json event = {{"key", "value"}};

  EventId event_id = EventId{SubscriptionId(1234),
                        {/*key_in=*/ "1", EventSourceId::Type::kDbusObjects},
                          absl::Now()};

  // Add event and check if it is stored
  event_store->AddNewEvent(event_id, event);
  EXPECT_THAT(event_store->GetEventsSince(/*redfish_event_id=*/std::nullopt),
              SizeIs(1));
  EXPECT_EQ(event_store->GetEventsSince(/*redfish_event_id=*/std::nullopt)[0],
            event);
}

TEST(EventStoreImplTest, ShouldReturnAllEventsByTime) {
  absl::Time time_old = absl::Now();
  absl::Time time_new = absl::Now();

  // Event Store should return older event before newer event.
  {
    // Create event store
    std::unique_ptr<EventStore> event_store = CreateEventStore(kEventStoreSize);

    event_store->AddNewEvent(
        EventId{SubscriptionId(123),
                {/*key_in=*/ "1", EventSourceId::Type::kDbusObjects},
                time_old},
        {{"_", 1}});
    event_store->AddNewEvent(
        EventId{SubscriptionId(123),
                {/*key_in=*/ "1", EventSourceId::Type::kDbusObjects},
                time_new},
        {{"_", 2}});

    // Check if old event is returned before new event.
    EXPECT_THAT(
        event_store->GetEventsSince(/*redfish_event_id=*/std::nullopt),
        ElementsAre(nlohmann::json{{"_", 1}}, nlohmann::json{{"_", 2}}));
  }

  // Test scenario in which newer event is added before the older event.
  // Event Store should return older event before newer event.
  {
    // Create event store
    std::unique_ptr<EventStore> event_store = CreateEventStore(kEventStoreSize);

    // New Event
    event_store->AddNewEvent(
        EventId{SubscriptionId(123),
                {/*key_in=*/ "1", EventSourceId::Type::kDbusObjects},
                time_new},
        {{"_", 2}});

    // Old Event
    event_store->AddNewEvent(
        EventId{SubscriptionId(123),
                {/*key_in=*/ "1", EventSourceId::Type::kDbusObjects},
                time_old},
        {{"_", 1}});

    // Check if old event is returned before new event.
    EXPECT_THAT(
        event_store->GetEventsSince(/*redfish_event_id=*/std::nullopt),
        ElementsAre(nlohmann::json{{"_", 1}}, nlohmann::json{{"_", 2}}));
  }
}

TEST(EventStoreImplTest, ShouldReturnAllEventsByInsertionOrder) {
  std::unique_ptr<EventStore> event_store = CreateEventStore(kEventStoreSize);

  absl::Time time = absl::Now();

  // New Event
  event_store->AddNewEvent(
      EventId{SubscriptionId(123),
              {/*key_in=*/ "1", EventSourceId::Type::kDbusObjects},
              time},
      {{"_", 2}});

  // Old Event
  event_store->AddNewEvent(
      EventId{SubscriptionId(123),
              {/*key_in=*/ "1", EventSourceId::Type::kDbusObjects},
              time},
      {{"_", 1}});

  // In this case insertion order shall be honored.
  // Check if new event is returned before old event.
  std::vector<nlohmann::json> retrieved_events =
      event_store->GetEventsSince(/*redfish_event_id=*/std::nullopt);
  EXPECT_THAT(event_store->GetEventsSince(/*redfish_event_id=*/std::nullopt),
              ElementsAre(nlohmann::json{{"_", 2}}, nlohmann::json{{"_", 1}}));
}

TEST(EventStoreImplTest, ShouldReturnEventsSinceValidLastEventId) {
  std::unique_ptr<EventStore> event_store = CreateEventStore(kEventStoreSize);

  // Prefill 2 events
  for (size_t i = 0; i < 2; ++i) {
    event_store->AddNewEvent(
        EventId{SubscriptionId(/*subscription_id_in=*/1),
                {/*key_in=*/ "1", EventSourceId::Type::kDbusObjects},
                absl::Now()},
        {{"key1", i}});
  }

  // Add a test event whose uuid will be specified in last_event_id.
  EventId test_event{SubscriptionId(1),
                     {/*key_in=*/ "1", EventSourceId::Type::kDbusObjects},
                     absl::Now()};
  size_t test_event_uuid = test_event.redfish_event_id;
  event_store->AddNewEvent(test_event, {{"key3", "value3"}});

  // Add 2 more events on top of test event.
  for (size_t i = 4; i < 6; ++i) {
    event_store->AddNewEvent(
        EventId{SubscriptionId(1),
                {/*key_in=*/ "1", EventSourceId::Type::kDbusObjects},
                absl::Now()},
        {{"key1", i}});
  }

  {
    // Retrieve events after specific last_event_id
    EXPECT_THAT(
        event_store->GetEventsSince(test_event_uuid),
        ElementsAre(nlohmann::json{{"key1", 4}}, nlohmann::json{{"key1", 5}}));
  }

  // Retrieve events after unknown last_event_id
  {
    // Retrieve events after specific last_event_id
    std::vector<nlohmann::json> retrieved_events =
        event_store->GetEventsSince(4);
    EXPECT_THAT(retrieved_events, SizeIs(0));
  }
}

TEST(EventStoreImplTest, ShouldOverwriteOldestEventOnFull) {
  std::unique_ptr<EventStore> event_store = CreateEventStore(kEventStoreSize);

  for (size_t i = 0; i < kEventStoreSize + 1; ++i) {
    event_store->AddNewEvent(
        EventId{SubscriptionId(i),
                {/*key_in=*/ "1", EventSourceId::Type::kDbusObjects},
                absl::Now()},
        {{"key1", i}});
  }

  // Check if the oldest event is removed
  EXPECT_THAT(
      event_store->GetEventsSince(/*redfish_event_id=*/std::nullopt),
      ElementsAre(nlohmann::json{{"key1", 1}}, nlohmann::json{{"key1", 2}},
                  nlohmann::json{{"key1", 3}}));
}

TEST(EventStoreImplTest, ShouldReturnValidJSON) {
  std::unique_ptr<EventStore> event_store = CreateEventStore(kEventStoreSize);

  // Prefill 2 events
  for (size_t i = 0; i < 2; ++i) {
    event_store->AddNewEvent(
        EventId{SubscriptionId(/*subscription_id_in=*/1),
                {/*key_in=*/ "1", EventSourceId::Type::kDbusObjects},
                absl::Now()},
        {{"key1", i}});
  }
  nlohmann::json json = event_store->ToJSON();
  EXPECT_THAT(json["NumEvents"], Eq(2));
  nlohmann::json json_0 = json["EventStore"][0];
  nlohmann::json json_1 = json["EventStore"][1];
  EXPECT_THAT(json_0["Event"], Eq(nlohmann::json{{"key1", 0}}));
  EXPECT_THAT(json_1["Event"], Eq(nlohmann::json{{"key1", 1}}));
}

TEST(EventStoreImplTest, GetEventReturnsValidResult) {
  std::unique_ptr<EventStore> event_store = CreateEventStore(kEventStoreSize);

  EventId event_id_0 = EventId{SubscriptionId(1),
                        {/*key_in=*/ "1", EventSourceId::Type::kDbusObjects},
                        absl::Now()};
  EventId event_id_1 = EventId{SubscriptionId(1),
                        {/*key_in=*/ "1", EventSourceId::Type::kDbusObjects},
                        absl::Now()};
  EventId event_id_2 = EventId{SubscriptionId(1),
                        {/*key_in=*/ "1", EventSourceId::Type::kDbusObjects},
                        absl::Now()};

  // Prefill 2 events
  event_store->AddNewEvent(event_id_0, {{"key1", 0}});
  event_store->AddNewEvent(event_id_1, {{"key1", 1}});

  nlohmann::json json = event_store->ToJSON();
  EXPECT_THAT(json["NumEvents"], Eq(2));
  nlohmann::json json_0 = json["EventStore"][0];
  nlohmann::json json_1 = json["EventStore"][1];
  EXPECT_THAT(json_0["Event"], Eq(nlohmann::json{{"key1", 0}}));
  EXPECT_THAT(json_1["Event"], Eq(nlohmann::json{{"key1", 1}}));
  nlohmann::json event_0 = event_store->GetEvent(event_id_0);
  EXPECT_THAT(event_0, Eq(nlohmann::json{{"key1", 0}}));
  nlohmann::json event_1 = event_store->GetEvent(event_id_1);
  EXPECT_THAT(event_1, Eq(nlohmann::json{{"key1", 1}}));
  nlohmann::json event_2 = event_store->GetEvent(event_id_2);
  EXPECT_THAT(event_2, Eq(nlohmann::json{}));
}

TEST(EventStoreImplTest, TestGetEventsBySubscriptionId) {
  std::unique_ptr<EventStore> event_store = CreateEventStore(10);

  // Prefill 2 events per subscriber
  for (size_t i = 0; i < 2; ++i) {
    event_store->AddNewEvent(
        EventId{SubscriptionId(/*subscription_id_in=*/1),
                {/*key_in=*/ "1", EventSourceId::Type::kDbusObjects},
                absl::Now()},
        {{"key01", i}});
  }
  for (size_t i = 0; i < 2; ++i) {
    event_store->AddNewEvent(
        EventId{SubscriptionId(/*subscription_id_in=*/2),
                {/*key_in=*/ "1", EventSourceId::Type::kDbusObjects},
                absl::Now()},
        {{"key02", i}});
  }
  nlohmann::json json = event_store->GetEventsBySubscriptionId(1);
  EXPECT_THAT(json[0], Eq(nlohmann::json{{"key01", 0}}));
  EXPECT_THAT(json[1], Eq(nlohmann::json{{"key01", 1}}));

  json = event_store->GetEventsBySubscriptionId(2);
  EXPECT_THAT(json[0], Eq(nlohmann::json{{"key02", 0}}));
  EXPECT_THAT(json[1], Eq(nlohmann::json{{"key02", 1}}));
}

TEST(EventStoreImplTest, ClearEventStore) {
  std::unique_ptr<EventStore> event_store = CreateEventStore(kEventStoreSize);

  // Prefill 2 events
  for (size_t i = 0; i < 2; ++i) {
    event_store->AddNewEvent(
        EventId{SubscriptionId(/*subscription_id_in=*/1),
                {/*key_in=*/ "1", EventSourceId::Type::kDbusObjects},
                absl::Now()},
        {{"key1", i}});
  }
  nlohmann::json json = event_store->ToJSON();
  EXPECT_THAT(json["NumEvents"], Eq(2));
  nlohmann::json json_0 = json["EventStore"][0];
  nlohmann::json json_1 = json["EventStore"][1];
  EXPECT_THAT(json_0["Event"], Eq(nlohmann::json{{"key1", 0}}));
  EXPECT_THAT(json_1["Event"], Eq(nlohmann::json{{"key1", 1}}));
  event_store->Clear();
  json = event_store->ToJSON();
  EXPECT_THAT(json["NumEvents"], Eq(0));
}

TEST(EventStoreImplTest, TestEventId) {
  EventId event_id_0 = EventId{SubscriptionId(1),
                        {/*key_in=*/ "1", EventSourceId::Type::kDbusObjects},
                        absl::Now()};
  // event_id_0 and event_id_1 should be identical.
  EventId event_id_1 = EventId{SubscriptionId(1),
                        {/*key_in=*/ "1", EventSourceId::Type::kDbusObjects},
                        event_id_0.timestamp};
  // event_id_1 and event_id_2 differ in the timestamp.
  EventId event_id_2 = EventId{SubscriptionId(1),
                        {/*key_in=*/ "1", EventSourceId::Type::kDbusObjects},
                        absl::Now()};
  // event_id_3 and event_id_2 differ in the EventSourceId::Type.
  EventId event_id_3 = EventId{SubscriptionId(1),
                        {/*key_in=*/ "1", EventSourceId::Type::kSocketIO},
                        event_id_2.timestamp};
  // event_id_4 and event_id_3 differ in the SubscriptionId.
  EventId event_id_4 = EventId{SubscriptionId(2),
                        {/*key_in=*/ "1", EventSourceId::Type::kSocketIO},
                        event_id_3.timestamp};

  EXPECT_EQ(event_id_0, event_id_1);
  EXPECT_NE(event_id_1, event_id_2);
  EXPECT_NE(event_id_2, event_id_3);
  EXPECT_NE(event_id_3, event_id_4);
}
}  // namespace

}  // namespace ecclesia
