/*
 * Copyright 2022 Google LLC
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

#include "ecclesia/lib/redfish/transport/grpc.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/network/testing.h"
#include "ecclesia/lib/redfish/testing/fake_redfish_server.h"
#include "ecclesia/lib/redfish/testing/grpc_dynamic_mockup_server.h"
#include "ecclesia/lib/redfish/transport/grpc_tls_options.h"
#include "ecclesia/lib/status/rpc.h"
#include "ecclesia/lib/testing/status.h"
#include "ecclesia/lib/time/clock_fake.h"

namespace ecclesia {
namespace {

using ::testing::Eq;

TEST(GrpcRedfishTransport, Get) {
  absl::flat_hash_map<std::string, std::string> headers;
  GrpcDynamicMockupServer mockup_server("barebones_session_auth/mockup.shar",
                                        "[::1]", 0);
  StaticBufferBasedTlsOptions options;
  options.SetToInsecure();
  auto port = mockup_server.Port();
  ASSERT_TRUE(port.has_value());
  auto transport = CreateGrpcRedfishTransport(absl::StrCat("[::1]:", *port), {},
                                              options.GetChannelCredentials());
  ASSERT_THAT(transport, IsOk());
  absl::string_view expected_str = R"json({
    "@odata.context": "/redfish/v1/$metadata#ServiceRoot.ServiceRoot",
    "@odata.id": "/redfish/v1",
    "@odata.type": "#ServiceRoot.v1_5_0.ServiceRoot",
    "Chassis": {
      "@odata.id": "/redfish/v1/Chassis"
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
  nlohmann::json expected = nlohmann::json::parse(expected_str, nullptr, false);
  absl::StatusOr<RedfishTransport::Result> res_get =
      (*transport)->Get("/redfish/v1");
  ASSERT_THAT(res_get, IsOk());
  EXPECT_THAT(res_get->body, Eq(expected));
  EXPECT_THAT(res_get->code, Eq(200));
}

TEST(GrpcRedfishTransport, PostPatchGetDelete) {
  absl::flat_hash_map<std::string, std::string> headers;
  GrpcDynamicMockupServer mockup_server("barebones_session_auth/mockup.shar",
                                        "[::1]", 0);
  StaticBufferBasedTlsOptions options;
  options.SetToInsecure();
  auto port = mockup_server.Port();
  ASSERT_TRUE(port.has_value());
  auto transport = CreateGrpcRedfishTransport(absl::StrCat("[::1]:", *port), {},
                                              options.GetChannelCredentials());
  ASSERT_THAT(transport, IsOk());
  nlohmann::json expected_post =
      nlohmann::json::parse(R"json({})json", nullptr, false);
  absl::StatusOr<RedfishTransport::Result> res_post =
      (*transport)->Post("/redfish/v1/Chassis", R"json({
    "ChassisType": "RackMount",
    "Name": "MyChassis"
  })json");
  ASSERT_THAT(res_post, IsOk());
  EXPECT_THAT(res_post->body, expected_post);
  EXPECT_THAT(res_post->code, Eq(204));

  absl::string_view expected_get_str = R"json({
    "@odata.context":"/redfish/v1/$metadata#ChassisCollection.ChassisCollection",
    "@odata.id":"/redfish/v1/Chassis",
    "@odata.type":"#ChassisCollection.ChassisCollection",
    "Members":[
      {
        "@odata.id":"/redfish/v1/Chassis/chassis"
      },
      {
        "@odata.id":"/redfish/v1/Chassis/Member1",
        "ChassisType":"RackMount",
        "Id":"Member1",
        "Name":"MyChassis"
      }
    ],
    "Members@odata.count":2.0,"Name":"Chassis Collection"
  })json";
  nlohmann::json expected_get =
      nlohmann::json::parse(expected_get_str, nullptr, false);
  absl::StatusOr<RedfishTransport::Result> res_get =
      (*transport)->Get("/redfish/v1/Chassis");
  ASSERT_THAT(res_get, IsOk());
  EXPECT_THAT(res_get->body, Eq(expected_get));
  EXPECT_THAT(res_get->code, Eq(200));

  absl::string_view data_patch = R"json({
    "Name": "MyPatchChassis"
  })json";
  nlohmann::json expected_patch =
      nlohmann::json::parse(R"json({})json", nullptr, false);
  absl::StatusOr<RedfishTransport::Result> res_patch =
      (*transport)->Patch("/redfish/v1/Chassis/Member1", data_patch);
  ASSERT_THAT(res_patch, IsOk());
  EXPECT_THAT(res_patch->body, Eq(expected_patch));
  EXPECT_THAT(res_patch->code, Eq(204));

  expected_get_str = R"json({
    "@odata.context":"/redfish/v1/$metadata#ChassisCollection.ChassisCollection",
    "@odata.id":"/redfish/v1/Chassis",
    "@odata.type":"#ChassisCollection.ChassisCollection",
    "Members":[
      {
        "@odata.id":"/redfish/v1/Chassis/chassis"
      },
      {
        "@odata.id":"/redfish/v1/Chassis/Member1",
        "ChassisType":"RackMount",
        "Id":"Member1",
        "Name":"MyPatchChassis"
      }
    ],
    "Members@odata.count":2.0,"Name":"Chassis Collection"
  })json";
  expected_get = nlohmann::json::parse(expected_get_str, nullptr, false);
  res_get = (*transport)->Get("/redfish/v1/Chassis");
  ASSERT_THAT(res_get, IsOk());
  EXPECT_THAT(res_get->body, Eq(expected_get));
  EXPECT_THAT(res_get->code, Eq(200));

  EXPECT_THAT(
      (*transport)->Delete("/redfish/v1/Chassis/Member1", "{}"),
      internal_status::IsStatusPolyMatcher(absl::StatusCode::kUnimplemented));
}

TEST(GrpcRedfishTransport, GetRootUri) {
  GrpcDynamicMockupServer mockup_server("barebones_session_auth/mockup.shar",
                                        "[::1]", 0);
  StaticBufferBasedTlsOptions options;
  options.SetToInsecure();
  auto port = mockup_server.Port();
  ASSERT_TRUE(port.has_value());
  auto transport = CreateGrpcRedfishTransport(absl::StrCat("[::1]:", *port), {},
                                              options.GetChannelCredentials());
  ASSERT_THAT(transport, IsOk());
  EXPECT_EQ((*transport)->GetRootUri(), "/redfish/v1");
}

TEST(GrpcRedfishTransport, ResourceNotFound) {
  GrpcDynamicMockupServer mockup_server("barebones_session_auth/mockup.shar",
                                        "[::1]", 0);
  StaticBufferBasedTlsOptions options;
  options.SetToInsecure();
  auto port = mockup_server.Port();
  ASSERT_TRUE(port.has_value());
  auto transport = CreateGrpcRedfishTransport(absl::StrCat("[::1]:", *port), {},
                                              options.GetChannelCredentials());
  ASSERT_THAT(transport, IsOk());

  auto result_get = (*transport)->Get("/redfish/v1/Chassis/noexist");
  EXPECT_THAT(result_get, IsOk());
  nlohmann::json expected_get =
      nlohmann::json::parse(R"json({})json", nullptr, false);
  EXPECT_THAT(result_get->body, Eq(expected_get));
  EXPECT_THAT(result_get->code, Eq(404));
}

TEST(GrpcRedfishTransport, NotAllowed) {
  GrpcDynamicMockupServer mockup_server("barebones_session_auth/mockup.shar",
                                        "[::1]", 0);
  StaticBufferBasedTlsOptions options;
  options.SetToInsecure();
  auto port = mockup_server.Port();
  ASSERT_TRUE(port.has_value());
  auto transport = CreateGrpcRedfishTransport(absl::StrCat("[::1]:", *port), {},
                                              options.GetChannelCredentials());
  ASSERT_THAT(transport, IsOk());

  absl::string_view data_post = R"json({
    "ChassisType": "RackMount",
    "Name": "MyChassis"
  })json";
  auto result_post = (*transport)->Post("/redfish", data_post);
  EXPECT_THAT(result_post, IsOk());
  nlohmann::json expected_post =
      nlohmann::json::parse(R"json({})json", nullptr, false);
  EXPECT_THAT(result_post->body, Eq(expected_post));
  EXPECT_THAT(result_post->code, Eq(405));

  auto result_patch = (*transport)->Patch("/redfish", data_post);
  EXPECT_THAT(result_patch, IsOk());
  nlohmann::json expected_patch =
      nlohmann::json::parse(R"json({})json", nullptr, false);
  EXPECT_THAT(result_patch->body, Eq(expected_patch));
  EXPECT_THAT(result_patch->code, Eq(204));
}

TEST(GrpcRedfishTransport, Timeout) {
  int port = ecclesia::FindUnusedPortOrDie();
  GrpcTransportParams params;
  StaticBufferBasedTlsOptions options;
  FakeClock clock;

  // Using a fake clock that point the time to negative, making the context
  // deadline always in the past
  params.timeout = absl::AbsDuration(absl::Milliseconds(50));
  params.clock = &clock;

  std::string endpoint = absl::StrCat("[::1]:", port);
  testing::internal::Notification notification;
  GrpcDynamicMockupServer mockup_server("barebones_session_auth/mockup.shar",
                                        "[::1]", port);

  if (auto transport = CreateGrpcRedfishTransport(
          endpoint, params, options.GetChannelCredentials());
      transport.ok()) {
    EXPECT_THAT((*transport)->Get("/redfish/v1"),
                internal_status::IsStatusPolyMatcher(
                    absl::StatusCode::kDeadlineExceeded));
  }
}

TEST(GrpcRedfishTransport, EndpointFqdn) {
  StaticBufferBasedTlsOptions options;
  auto transport =
      CreateGrpcRedfishTransport("/:", {}, options.GetChannelCredentials());
  EXPECT_THAT(transport, IsStatusInvalidArgument());

  transport =
      CreateGrpcRedfishTransport("dns:/:", {}, options.GetChannelCredentials());
  EXPECT_THAT(transport, IsStatusInvalidArgument());

  transport = CreateGrpcRedfishTransport("dns:/:80", {},
                                         options.GetChannelCredentials());
  EXPECT_THAT(transport, IsOk());
}
}  // namespace
}  // namespace ecclesia
