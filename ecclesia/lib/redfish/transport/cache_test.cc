/*
 * Copyright 2026 Google LLC
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

#include "ecclesia/lib/redfish/transport/cache.h"

#include <memory>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/redfish/transport/mocked_interface.h"
#include "ecclesia/lib/testing/status.h"
#include "ecclesia/lib/thread/thread.h"
#include "ecclesia/lib/time/clock_fake.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {
namespace {

using ::testing::Eq;
using ::testing::Field;
using ::testing::Return;

RedfishTransport::Result MakeJsonResult(absl::string_view id,
                                        absl::string_view name) {
  return RedfishTransport::Result{
      .code = 200,
      .body = nlohmann::json{{"@odata.id", id}, {"Name", name}},
      .headers = {{"OData-Version", "4.0"},
                  {"Content-Type", "application/json"}},
  };
}

TEST(TimeBasedCacheTest,
     DestroyCacheDuringInFlightUncachedGetDoesNotUseAfterFree) {
  constexpr absl::string_view kPath = "/redfish/v1/Chassis/chassis";
  FakeClock clock;
  RedfishTransportMock transport;
  auto cache = TimeBasedCache::Create(&transport, absl::Minutes(1), &clock);

  // Populate the CacheNode with a prior result containing heap-allocated JSON
  // and headers, matching the production state where an existing CacheNode is
  // refreshed via UncachedGet during shutdown.
  EXPECT_CALL(transport, Get(kPath))
      .WillOnce(Return(MakeJsonResult(kPath, "initial_chassis")));
  auto initial = cache->CachedGet(kPath);
  ASSERT_THAT(initial.result, IsOk());
  EXPECT_TRUE(initial.is_fresh);

  absl::Notification get_started;
  absl::Notification finish_get;
  EXPECT_CALL(transport, Get(kPath))
      .WillOnce([&](absl::string_view path)
                    -> absl::StatusOr<RedfishTransport::Result> {
        get_started.Notify();
        finish_get.WaitForNotification();
        return MakeJsonResult(path, "updated_chassis");
      });

  RedfishCachedGetterInterface::OperationResult op_result;
  auto worker = GetDefaultThreadFactory()->New(
      [&]() { op_result = cache->UncachedGet(kPath); });

  get_started.WaitForNotification();

  absl::Notification destroyer_started;
  absl::Notification cache_destroyed;
  auto destroyer = GetDefaultThreadFactory()->New([&]() {
    destroyer_started.Notify();
    cache.reset();
    cache_destroyed.Notify();
  });

  destroyer_started.WaitForNotification();
  EXPECT_FALSE(
      cache_destroyed.WaitForNotificationWithTimeout(absl::Milliseconds(20)));
  finish_get.Notify();
  worker->Join();
  destroyer->Join();

  EXPECT_TRUE(op_result.is_fresh);
  EXPECT_THAT(op_result.result,
              IsOkAndHolds(Field(&RedfishTransport::Result::code, Eq(200))));
}

TEST(TimeBasedCacheTest,
     DestroyCacheDuringInFlightCachedGetDoesNotUseAfterFree) {
  constexpr absl::string_view kPath = "/redfish/v1/Systems/system";
  FakeClock clock;
  RedfishTransportMock transport;
  auto cache = TimeBasedCache::Create(&transport, absl::Minutes(1), &clock);

  absl::Notification get_started;
  absl::Notification finish_get;
  EXPECT_CALL(transport, Get(kPath))
      .WillOnce([&](absl::string_view path)
                    -> absl::StatusOr<RedfishTransport::Result> {
        get_started.Notify();
        finish_get.WaitForNotification();
        return MakeJsonResult(path, "system");
      });

  RedfishCachedGetterInterface::OperationResult op_result;
  auto worker = GetDefaultThreadFactory()->New(
      [&]() { op_result = cache->CachedGet(kPath); });

  get_started.WaitForNotification();

  absl::Notification destroyer_started;
  absl::Notification cache_destroyed;
  auto destroyer = GetDefaultThreadFactory()->New([&]() {
    destroyer_started.Notify();
    cache.reset();
    cache_destroyed.Notify();
  });

  destroyer_started.WaitForNotification();
  EXPECT_FALSE(
      cache_destroyed.WaitForNotificationWithTimeout(absl::Milliseconds(20)));
  finish_get.Notify();
  worker->Join();
  destroyer->Join();

  EXPECT_TRUE(op_result.is_fresh);
  EXPECT_THAT(op_result.result,
              IsOkAndHolds(Field(&RedfishTransport::Result::code, Eq(200))));
}

TEST(TimeBasedCacheTest,
     DestroyDeepCacheDuringInFlightCachedGetDoesNotUseAfterFree) {
  constexpr absl::string_view kPath = "/redfish/v1/Systems/system";
  FakeClock clock;
  RedfishTransportMock transport;
  auto cache =
      TimeBasedCache::CreateDeepCache(&transport, absl::Minutes(1), &clock);

  absl::Notification get_started;
  absl::Notification finish_get;
  EXPECT_CALL(transport, Get(kPath))
      .WillOnce([&](absl::string_view path)
                    -> absl::StatusOr<RedfishTransport::Result> {
        get_started.Notify();
        finish_get.WaitForNotification();
        return RedfishTransport::Result{
            .code = 200,
            .body =
                nlohmann::json{
                    {"@odata.id", path},
                    {"Name", "system"},
                    {"Processors", nlohmann::json::array({nlohmann::json{
                                       {"@odata.id",
                                        "/redfish/v1/Systems/system/"
                                        "Processors/cpu0"},
                                       {"Id", "cpu0"}}})},
                },
            .headers = {{"OData-Version", "4.0"},
                        {"Content-Type", "application/json"}},
        };
      });

  RedfishCachedGetterInterface::OperationResult op_result;
  auto worker = GetDefaultThreadFactory()->New(
      [&]() { op_result = cache->CachedGet(kPath); });

  get_started.WaitForNotification();

  absl::Notification destroyer_started;
  absl::Notification cache_destroyed;
  auto destroyer = GetDefaultThreadFactory()->New([&]() {
    destroyer_started.Notify();
    cache.reset();
    cache_destroyed.Notify();
  });

  destroyer_started.WaitForNotification();
  EXPECT_FALSE(
      cache_destroyed.WaitForNotificationWithTimeout(absl::Milliseconds(20)));
  finish_get.Notify();
  worker->Join();
  destroyer->Join();

  EXPECT_TRUE(op_result.is_fresh);
  EXPECT_THAT(op_result.result,
              IsOkAndHolds(Field(&RedfishTransport::Result::code, Eq(200))));
}

TEST(TimeBasedCacheTest,
     DestroyCacheDuringInFlightCachedPostDoesNotUseAfterFree) {
  constexpr absl::string_view kPath = "/redfish/v1/Chassis/Actions/Reset";
  constexpr absl::string_view kPayload = R"json({"ResetType":"On"})json";
  FakeClock clock;
  RedfishTransportMock transport;
  auto cache = TimeBasedCache::Create(&transport, absl::Minutes(1), &clock);

  absl::Notification post_started;
  absl::Notification finish_post;
  EXPECT_CALL(transport, Post(kPath, kPayload))
      .WillOnce(
          [&](absl::string_view path,
              absl::string_view) -> absl::StatusOr<RedfishTransport::Result> {
            post_started.Notify();
            finish_post.WaitForNotification();
            return MakeJsonResult(path, "reset_ok");
          });

  RedfishCachedGetterInterface::OperationResult op_result;
  auto worker = GetDefaultThreadFactory()->New([&]() {
    op_result = cache->CachedPost(kPath, kPayload, absl::Minutes(1));
  });

  post_started.WaitForNotification();

  absl::Notification destroyer_started;
  absl::Notification cache_destroyed;
  auto destroyer = GetDefaultThreadFactory()->New([&]() {
    destroyer_started.Notify();
    cache.reset();
    cache_destroyed.Notify();
  });

  destroyer_started.WaitForNotification();
  EXPECT_FALSE(
      cache_destroyed.WaitForNotificationWithTimeout(absl::Milliseconds(20)));
  finish_post.Notify();
  worker->Join();
  destroyer->Join();

  EXPECT_TRUE(op_result.is_fresh);
  EXPECT_THAT(op_result.result,
              IsOkAndHolds(Field(&RedfishTransport::Result::code, Eq(200))));
}

}  // namespace
}  // namespace ecclesia
