/*
 * Copyright 2025 Google LLC
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

#include "ecclesia/lib/redfish/transport/fake_interface.h"

#include <memory>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "ecclesia/lib/redfish/testing/fake_redfish_server.h"
#include "ecclesia/lib/status/test_macros.h"
#include "ecclesia/lib/testing/status.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {
namespace {

constexpr absl::string_view kMockupFilePath = "indus_hmb_shim/mockup.shar";
constexpr absl::string_view kServiceRootResponse = R"json({
        "@odata.context": "/redfish/v1/$metadata#ServiceRoot.ServiceRoot",
        "@odata.id": "/redfish/v1",
        "@odata.type": "#ServiceRoot.v1_5_0.ServiceRoot",
        "Chassis": {
            "@odata.id": "/redfish/v1/Chassis"
        },
        "Id": "RootService",
        "Name": "Root Service",
        "RedfishVersion": "1.6.1",
        "Systems": {
            "@odata.id": "/redfish/v1/Systems"
        },
        "Oem" : {
            "Google" : {
              "TopologyRepresentation" : "redfish-devpath-v1"
            }
        }
        })json";

TEST(FakeRedfishTransportTest, GetFromFakeRedfishServer) {
  auto fake_redfish_server =
      std::make_unique<ecclesia::FakeRedfishServer>(kMockupFilePath);
  FakeRedfishTransport transport(
      fake_redfish_server->RedfishClientTransport(),
      [](absl::string_view, absl::StatusOr<FakeRedfishTransport::Result>&)
          -> bool { return false; });

  ECCLESIA_ASSIGN_OR_FAIL(FakeRedfishTransport::Result result,
                          transport.Get("/redfish/v1"));

  EXPECT_EQ(result.code, 200);
  ASSERT_TRUE(absl::holds_alternative<nlohmann::json>(result.body));

  nlohmann::json expected =
      nlohmann::json::parse(kServiceRootResponse, nullptr, false);
  EXPECT_EQ(absl::get<nlohmann::json>(result.body), expected);
}

TEST(FakeRedfishTransportTest, GetWithOverrideCallback) {
  auto fake_redfish_server =
      std::make_unique<ecclesia::FakeRedfishServer>(kMockupFilePath);
  FakeRedfishTransport transport(
      fake_redfish_server->RedfishClientTransport(),
      [](absl::string_view,
         absl::StatusOr<FakeRedfishTransport::Result>& result) {
        result = absl::NotFoundError("Not found");
        return true;
      });

  EXPECT_THAT(transport.Get("/redfish/v1"), IsStatusNotFound());
}

TEST(FakeRedfishTransportTest, GetWithDynamicCallback) {
  bool apply_overrides = true;
  auto fake_redfish_server =
      std::make_unique<ecclesia::FakeRedfishServer>(kMockupFilePath);
  FakeRedfishTransport transport(
      fake_redfish_server->RedfishClientTransport(),
      [&apply_overrides](absl::string_view,
                         absl::StatusOr<FakeRedfishTransport::Result>& result) {
        if (apply_overrides) {
          result = absl::NotFoundError("Not found");
          return true;
        }
        return false;
      });

  // First call should return a 404.
  EXPECT_THAT(transport.Get("/redfish/v1"), IsStatusNotFound());

  // Second call should return the service root response.
  apply_overrides = false;
  ECCLESIA_ASSIGN_OR_FAIL(FakeRedfishTransport::Result result,
                          transport.Get("/redfish/v1"));
  EXPECT_EQ(result.code, 200);
  ASSERT_TRUE(absl::holds_alternative<nlohmann::json>(result.body));
  nlohmann::json expected =
      nlohmann::json::parse(kServiceRootResponse, nullptr, false);
  EXPECT_EQ(absl::get<nlohmann::json>(result.body), expected);
}

}  // namespace
}  // namespace ecclesia
