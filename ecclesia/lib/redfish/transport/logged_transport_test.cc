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

#include "ecclesia/lib/redfish/transport/logged_transport.h"

#include <memory>
#include <utility>
#include <variant>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/redfish/transport/mocked_interface.h"
#include "ecclesia/lib/testing/status.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {
namespace {

using ::testing::Eq;
using ::testing::Return;
using ::ecclesia::IsOk;

TEST(LoggedTransportTest, GetWithTimeout) {
  auto mock_transport = std::make_unique<RedfishTransportMock>();
  nlohmann::json expected_result = nlohmann::json::parse(R"json({
    "@odata.id": "/redfish/v1/Chassis/1",
    "Id": "Chassis1",
    "Name": "Chassis 1"
  })json",
                                                         nullptr, false);
  EXPECT_CALL(*mock_transport, Get("/redfish/v1/Chassis/1", absl::Seconds(1)))
      .WillOnce(Return(
          RedfishTransport::Result{.code = 200, .body = expected_result}));
  auto logged_transport =
      std::make_unique<RedfishLoggedTransport>(std::move(mock_transport));
  absl::StatusOr<RedfishTransport::Result> res_get =
      logged_transport->Get("/redfish/v1/Chassis/1", absl::Seconds(1));
  ASSERT_THAT(res_get, IsOk());
  ASSERT_TRUE(std::holds_alternative<nlohmann::json>(res_get->body));
  EXPECT_THAT(std::get<nlohmann::json>(res_get->body), Eq(expected_result));
  EXPECT_THAT(res_get->code, Eq(200));
}

}  // namespace
}  // namespace ecclesia
