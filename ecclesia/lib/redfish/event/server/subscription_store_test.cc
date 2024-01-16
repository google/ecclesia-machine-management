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

#include "ecclesia/lib/redfish/event/server/subscription_store_impl.h"

#include <algorithm>
#include <memory>
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
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {

namespace {

using ::testing::_;
using ::testing::Eq;
using ::testing::MatchesRegex;

constexpr absl::string_view mock_trigger_str = R"json(
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

class SubscriptionStoreImplTest : public ::testing::Test {
 protected:
  SubscriptionStoreImplTest() {
    subscription_store_ = CreateSubscriptionStore();
    // Create event source ID
    auto trigger = nlohmann::json::parse(mock_trigger_str);
    EXPECT_THAT(trigger.is_discarded(), Eq(false));

    auto trigger_or_status_one = Trigger::Create(trigger);
    EXPECT_THAT(trigger_or_status_one.ok(), Eq(true));

    EventSourceId event_source_id_one(1, EventSourceId::Type::kDbusObjects);
    trigger_or_status_one->event_source_to_uri.insert(
        {event_source_id_one, {"/redfish/v1/Chassis/Foo/Sensors/x"}});

    test_id_to_triggers_.try_emplace("1", *trigger_or_status_one);

    auto trigger_or_status_two = Trigger::Create(trigger);
    EXPECT_THAT(trigger_or_status_two.ok(), Eq(true));

    EventSourceId event_source_id_two(2, EventSourceId::Type::kDbusObjects);
    trigger_or_status_two->event_source_to_uri.insert(
        {event_source_id_two, {"/redfish/v1/Chassis/Foo/Sensors/y"}});

    test_id_to_triggers_.try_emplace("2", *trigger_or_status_two);
  }

  // Subscription store under test.
  std::unique_ptr<SubscriptionStore> subscription_store_ = nullptr;
  absl::flat_hash_map<std::string, Trigger> test_id_to_triggers_;
};

TEST_F(SubscriptionStoreImplTest, CreateNewSubscriptionSuccess) {
  auto subscription_context = std::make_unique<SubscriptionContext>(
    SubscriptionId(1), test_id_to_triggers_, [](const nlohmann::json& event){});

  // Expect subscription creation to succeed.
  EXPECT_THAT(subscription_store_->AddNewSubscription(
      std::move(subscription_context)).ok(), Eq(true));
  EXPECT_THAT(subscription_store_->GetSubscription(SubscriptionId(1))
              .value()->subscription_id.Id(), Eq(1));
}

TEST_F(SubscriptionStoreImplTest, BadSubscriptionFail) {
  auto subscription_context_zero_id = std::make_unique<SubscriptionContext>(
    SubscriptionId(0), test_id_to_triggers_, [](const nlohmann::json& event){});

  // Reject subscription creation.
  EXPECT_THAT(subscription_store_->AddNewSubscription(
    std::move(subscription_context_zero_id)),
    Eq(absl::InvalidArgumentError("Invalid Id, must be >0")));
}

TEST_F(SubscriptionStoreImplTest, CreateDupSubscriptionFail) {
  auto subscription_context_one = std::make_unique<SubscriptionContext>(
    SubscriptionId(1), test_id_to_triggers_, [](const nlohmann::json& event){});
  auto subscription_context_one_again = std::make_unique<SubscriptionContext>(
    SubscriptionId(1), test_id_to_triggers_, [](const nlohmann::json& event){});

  // Expect subscription creation to succeed.
  EXPECT_THAT(subscription_store_->AddNewSubscription(
      std::move(subscription_context_one)).ok(), Eq(true));
  // Reject duplicate subscription id.
  EXPECT_THAT(subscription_store_->AddNewSubscription(
    std::move(subscription_context_one_again)).ok(), Eq(false));
}

TEST_F(SubscriptionStoreImplTest, GetSubscriptionByEventSourceIdSuccess) {
  auto subscription_context_one = std::make_unique<SubscriptionContext>(
    SubscriptionId(1), test_id_to_triggers_, [](const nlohmann::json& event){});
  auto subscription_context_two = std::make_unique<SubscriptionContext>(
    SubscriptionId(2), test_id_to_triggers_, [](const nlohmann::json& event){});

  EXPECT_THAT(subscription_store_->AddNewSubscription(
    std::move(subscription_context_one)).ok(), Eq(true));
  EXPECT_THAT(subscription_store_->AddNewSubscription(
    std::move(subscription_context_two)).ok(), Eq(true));

  // Expect subscriptions to be returned.
  auto result = subscription_store_->GetSubscriptionsByEventSourceId(
      EventSourceId(1, EventSourceId::Type::kDbusObjects));
  EXPECT_THAT(result.ok(), Eq(true));
  auto subscriptions = result.value();
  EXPECT_THAT(subscriptions[0]->subscription_id.Id(), Eq(1));
  EXPECT_THAT(subscriptions[1]->subscription_id.Id(), Eq(2));
}

TEST_F(SubscriptionStoreImplTest, GetSubscriptionByUnknownEventSourceIdFail) {
  auto subscription_context_one = std::make_unique<SubscriptionContext>(
    SubscriptionId(1), test_id_to_triggers_, [](const nlohmann::json& event){});
  auto subscription_context_two = std::make_unique<SubscriptionContext>(
    SubscriptionId(2), test_id_to_triggers_, [](const nlohmann::json& event){});

  EXPECT_THAT(subscription_store_->AddNewSubscription(
    std::move(subscription_context_one)).ok(), Eq(true));
  EXPECT_THAT(subscription_store_->AddNewSubscription(
    std::move(subscription_context_two)).ok(), Eq(true));

  // Expect unknown event_source_id to fail lookup.
  EXPECT_THAT(subscription_store_->GetSubscriptionsByEventSourceId(
    EventSourceId(111, EventSourceId::Type::kDbusObjects)).status(),
    absl::NotFoundError(
    "Event source with ID {\"key\":111,\"type\":\"kDbusObjects\"} not found."));
}

TEST_F(SubscriptionStoreImplTest, ToJSONAndToString) {
  auto subscription_context_one = std::make_unique<SubscriptionContext>(
    SubscriptionId(1), test_id_to_triggers_, [](const nlohmann::json& event){});
  auto subscription_context_two = std::make_unique<SubscriptionContext>(
    SubscriptionId(2), test_id_to_triggers_, [](const nlohmann::json& event){});

  EXPECT_THAT(subscription_store_->AddNewSubscription(
    std::move(subscription_context_one)).ok(), Eq(true));
  EXPECT_THAT(subscription_store_->AddNewSubscription(
    std::move(subscription_context_two)).ok(), Eq(true));

  // Verify the range of subscriptions as the iteration order of flat_hash_map
  // is non-deterministic.
  nlohmann::json json = subscription_store_->ToJSON();
  EXPECT_THAT(json["subscriptions"][0],
              testing::AllOf(testing::Ge(1), testing::Le(2)));
  EXPECT_THAT(json["subscriptions"][1],
              testing::AllOf(testing::Ge(1), testing::Le(2)));

  std::string result = subscription_store_->ToString();
  EXPECT_THAT(result, AllOf(MatchesRegex(".*subscriptions.*"),
                            MatchesRegex(".*[1-2],[1-2].*")));
}

TEST_F(SubscriptionStoreImplTest, DeleteSubscriptionSuccess) {
  auto subscription_context = std::make_unique<SubscriptionContext>(
    SubscriptionId(1), test_id_to_triggers_, [](const nlohmann::json& event){});

  EXPECT_THAT(subscription_store_->AddNewSubscription(
    std::move(subscription_context)).ok(), Eq(true));
  EXPECT_THAT(subscription_store_->GetSubscription(
    SubscriptionId(1)).value()->subscription_id.Id(), Eq(1));

  subscription_store_->DeleteSubscription(SubscriptionId(1));
  EXPECT_THAT(subscription_store_->GetSubscription(SubscriptionId(1)).status(),
    absl::NotFoundError("Subscription with ID 1 not found."));
}

TEST_F(SubscriptionStoreImplTest, DeleteUnknownSubscriptionNoop) {
  auto subscription_context = std::make_unique<SubscriptionContext>(
    SubscriptionId(1), test_id_to_triggers_, [](const nlohmann::json& event){});

  EXPECT_THAT(subscription_store_->AddNewSubscription(
    std::move(subscription_context)).ok(), Eq(true));
  EXPECT_THAT(subscription_store_->GetSubscription(
    SubscriptionId(1)).value()->subscription_id.Id(), Eq(1));

  // DeleteSubscription of an unknown subscription is a noop.
  subscription_store_->DeleteSubscription(SubscriptionId(10));
  EXPECT_THAT(subscription_store_->GetSubscription(
    SubscriptionId(1)).value()->subscription_id.Id(), Eq(1));
}

}  // namespace

}  // namespace ecclesia
