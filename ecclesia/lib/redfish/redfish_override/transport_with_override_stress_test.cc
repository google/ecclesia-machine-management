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

#include <atomic>
#include <memory>
#include <thread>  // NOLINT: Needed for std::thread, okay for 3P test-only use.
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/redfish/redfish_override/rf_override.pb.h"
#include "ecclesia/lib/redfish/redfish_override/transport_with_override.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/redfish/transport/mocked_interface.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {
namespace {

using ::testing::Return;

OverridePolicy CreateTestOverridePolicy() {
  return ParseTextProtoOrDie(R"pb(
    override_content_map_regex: {
      key: "/redfish/v1/Chassis/(.*)"
      value: {
        override_field:
        [ {
          action_replace: {
            object_identifier: {
              individual_object_identifier:
              [ { field_name: "Name" }]
            }
            override_value: { value: { string_value: "Overridden Name" } }
          }
        }]
      }
    }
  )pb");
}

nlohmann::json CreateOriginalJsonResponse() {
  return nlohmann::json::parse(R"json({
    "@odata.id": "/redfish/v1/Chassis/1",
    "Name": "Original Name"
  })json");
}

class RedfishTransportWithOverrideStressTest : public ::testing::Test {
 protected:
  RedfishTransportWithOverrideStressTest()
      : kPolicy(CreateTestOverridePolicy()),
        kOriginalJsonResponse(CreateOriginalJsonResponse()) {}

  const OverridePolicy kPolicy;
  // The original, non-overridden, JSON response from the Redfish transport.
  const nlohmann::json kOriginalJsonResponse;
};

TEST_F(RedfishTransportWithOverrideStressTest, ConcurrentInitializationRace) {
  // Tests the race condition of concurrent initialization of
  // `RedfishTransportWithOverride` to ensure that it is thread-safe (e.g.,
  // there are no double-free or other memory issues).
  constexpr int kNumThreads = 50;
  constexpr int kNumIterations = 20;

  for (int iter = 0; iter < kNumIterations; ++iter) {
    auto mock_transport = std::make_unique<RedfishTransportMock>();
    EXPECT_CALL(*mock_transport, Get("/redfish/v1/Chassis/1"))
        .WillRepeatedly(Return(RedfishTransport::Result{
            .code = 200,
            .body = kOriginalJsonResponse,
        }));

    std::atomic<int> call_count{0};
    auto transport_with_override =
        std::make_unique<RedfishTransportWithOverride>(
            std::move(mock_transport),
            [this, &call_count]() -> absl::StatusOr<OverridePolicy> {
              if (call_count.fetch_add(1) == 0) {
                return absl::InternalError("first fail");
              }
              absl::SleepFor(absl::Milliseconds(10));
              return kPolicy;
            });

    absl::Notification start_notification;
    std::vector<std::thread> threads;
    threads.reserve(kNumThreads);

    for (int i = 0; i < kNumThreads; ++i) {
      threads.emplace_back([&transport_with_override, &start_notification]() {
        start_notification.WaitForNotification();
        (void)transport_with_override->Get("/redfish/v1/Chassis/1");
      });
    }

    start_notification.Notify();
    for (std::thread& t : threads) {
      t.join();
    }
  }
}

}  // namespace
}  // namespace ecclesia
