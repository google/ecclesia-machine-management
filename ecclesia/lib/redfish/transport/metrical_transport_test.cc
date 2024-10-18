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

#include "ecclesia/lib/redfish/transport/metrical_transport.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ecclesia/lib/redfish/testing/grpc_dynamic_mockup_server.h"
#include "ecclesia/lib/redfish/transport/grpc.h"
#include "ecclesia/lib/redfish/transport/grpc_tls_options.h"
#include "ecclesia/lib/testing/status.h"
#include "ecclesia/lib/time/clock.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {
namespace {

using ::testing::Eq;
using ::ecclesia::IsOk;

TEST(MetricalRedfishTransportTest, GetWithTimeout) {
  // Initialize a GrpcRedfishTransport and wrap it in a MetricalRedfishTransport
  absl::flat_hash_map<std::string, std::string> headers;
  GrpcDynamicMockupServer mockup_server("barebones_session_auth/mockup.shar",
                                        "localhost", 0);
  StaticBufferBasedTlsOptions options;
  options.SetToInsecure();
  std::optional<int> port = mockup_server.Port();
  ASSERT_TRUE(port.has_value());
  auto transport = CreateGrpcRedfishTransport(
      absl::StrCat("localhost:", *port), {}, options.GetChannelCredentials());
  ASSERT_THAT(transport, IsOk());
  auto metrical_transport = std::make_unique<MetricalRedfishTransport>(
      std::move(*transport), ecclesia::Clock::RealClock());

  absl::string_view expected_str = R"json({
    "@odata.context": "/redfish/v1/$metadata#ServiceRoot.ServiceRoot",
    "@odata.id": "/redfish/v1",
    "@odata.type": "#ServiceRoot.v1_5_0.ServiceRoot",
    "Chassis": {
      "@odata.id": "/redfish/v1/Chassis"
    },
    "EventService": {
        "@odata.id": "/redfish/v1/EventService"
    },
    "Id": "RootService",
    "Links": {
      "Sessions": {
          "@odata.id": "/redfish/v1/SessionService/Sessions"
      }
    },
    "Name": "Root Service",
    "RedfishVersion": "1.6.1"
  })json";
  nlohmann::json expected =
      nlohmann::json::parse(std::string(expected_str), nullptr, false);

  // Get with 0 timeout should return deadline exceeded.
  // Ensure metrics are still collected.
  ASSERT_THAT(metrical_transport->Get("/redfish/v1", absl::ZeroDuration())
                  .status()
                  .code(),
              Eq(absl::StatusCode::kDeadlineExceeded));
  ASSERT_TRUE(metrical_transport->GetMetrics().uri_to_metrics_map().contains(
      "/redfish/v1"));
  RedfishMetrics::Metrics metrics =
      MetricalRedfishTransport::GetMetrics().uri_to_metrics_map().at(
          "/redfish/v1");
  EXPECT_THAT(
      metrics.request_type_to_metadata_failures().at("GET").request_count(),
      Eq(1));
}

}  // namespace
}  // namespace ecclesia
