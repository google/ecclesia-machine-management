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
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/network/testing.h"
#include "ecclesia/lib/redfish/testing/fake_redfish_server.h"
#include "ecclesia/lib/redfish/testing/grpc_dynamic_mockup_server.h"
#include "ecclesia/lib/status/rpc.h"
#include "ecclesia/lib/testing/status.h"

namespace ecclesia {
namespace {

using ::testing::Eq;

TEST(GrpcRedfishTransport, Get) {
  absl::flat_hash_map<std::string, std::string> headers;
  int port = ecclesia::FindUnusedPortOrDie();
  GrpcDynamicMockupServer mockup_server("barebones_session_auth/mockup.shar",
                                        "[::1]", port);
  GrpcRedfishTransport transport(absl::StrCat("[::1]:", port));

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
  absl::StatusOr<GrpcRedfishTransport::Result> res_get =
      transport.Get("/redfish/v1");
  ASSERT_THAT(res_get, IsOk());
  EXPECT_THAT(res_get->body, Eq(expected));
  EXPECT_THAT(res_get->code, Eq(200));
}

TEST(GrpcRedfishTransport, PostPatchGetDelete) {
  absl::flat_hash_map<std::string, std::string> headers;
  int port = ecclesia::FindUnusedPortOrDie();
  GrpcDynamicMockupServer mockup_server("barebones_session_auth/mockup.shar",
                                        "[::1]", port);
  GrpcRedfishTransport transport(absl::StrCat("[::1]:", port));

  nlohmann::json expected_post =
      nlohmann::json::parse(R"json({})json", nullptr, false);
  absl::StatusOr<GrpcRedfishTransport::Result> res_post =
      transport.Post("/redfish/v1/Chassis", R"json({
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
  absl::StatusOr<GrpcRedfishTransport::Result> res_get =
      transport.Get("/redfish/v1/Chassis");
  ASSERT_THAT(res_get, IsOk());
  EXPECT_THAT(res_get->body, Eq(expected_get));
  EXPECT_THAT(res_get->code, Eq(200));

  absl::string_view data_patch = R"json({
    "Name": "MyPatchChassis"
  })json";
  nlohmann::json expected_patch =
      nlohmann::json::parse(R"json({})json", nullptr, false);
  absl::StatusOr<GrpcRedfishTransport::Result> res_patch =
      transport.Patch("/redfish/v1/Chassis/Member1", data_patch);
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
  res_get = transport.Get("/redfish/v1/Chassis");
  ASSERT_THAT(res_get, IsOk());
  EXPECT_THAT(res_get->body, Eq(expected_get));
  EXPECT_THAT(res_get->code, Eq(200));

  EXPECT_THAT(
      transport.Delete("/redfish/v1/Chassis/Member1", "{}"),
      internal_status::IsStatusPolyMatcher(absl::StatusCode::kUnimplemented));
}

TEST(GrpcRedfishTransport, GetRootUri) {
  int port = ecclesia::FindUnusedPortOrDie();
  GrpcDynamicMockupServer mockup_server("barebones_session_auth/mockup.shar",
                                        "[::1]", port);
  GrpcRedfishTransport transport(absl::StrCat("[::1]:", port));
  EXPECT_EQ(transport.GetRootUri(), "/redfish/v1");
}

TEST(GrpcRedfishTransport, UpdateToNetworkEndpoint) {
  absl::flat_hash_map<std::string, std::string> headers;
  int port1 = ecclesia::FindUnusedPortOrDie();
  GrpcDynamicMockupServer mockup_server1("barebones_session_auth/mockup.shar",
                                         "[::1]", port1);
  int port2 = ecclesia::FindUnusedPortOrDie();
  GrpcDynamicMockupServer mockup_server("barebones_session_auth/mockup.shar",
                                        "[::1]", port2);
  GrpcRedfishTransport transport(absl::StrCat("[::1]:", port1));
  transport.UpdateToNetworkEndpoint(absl::StrCat("[::1]:", port2));

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
  absl::StatusOr<GrpcRedfishTransport::Result> res_get =
      transport.Get("/redfish/v1");
  ASSERT_THAT(res_get, IsOk());
  EXPECT_THAT(res_get->body, Eq(expected));
  EXPECT_THAT(res_get->code, Eq(200));
}

TEST(GrpcRedfishTransport, UpdateToUdsEndpoint) {
  absl::flat_hash_map<std::string, std::string> headers;
  int port1 = ecclesia::FindUnusedPortOrDie();
  GrpcDynamicMockupServer mockup_server1("barebones_session_auth/mockup.shar",
                                         "[::1]", port1);
  std::string mockup_uds =
      absl::StrCat(GetTestTempUdsDirectory(), "/mockup.socket");
  GrpcDynamicMockupServer mockup_server2("barebones_session_auth/mockup.shar",
                                         mockup_uds);
  GrpcRedfishTransport transport(absl::StrCat("[::1]:", port1));
  transport.UpdateToUdsEndpoint(mockup_uds);

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
  absl::StatusOr<GrpcRedfishTransport::Result> res_get =
      transport.Get("/redfish/v1");
  ASSERT_THAT(res_get, IsOk());
  EXPECT_THAT(res_get->body, Eq(expected));
  EXPECT_THAT(res_get->code, Eq(200));
}

TEST(GrpcRedfishTransport, ResourceNotFound) {
  int port = ecclesia::FindUnusedPortOrDie();
  GrpcDynamicMockupServer mockup_server("barebones_session_auth/mockup.shar",
                                        "[::1]", port);
  GrpcRedfishTransport transport(absl::StrCat("[::1]:", port));

  auto result_get = transport.Get("/redfish/v1/Chassis/noexist");
  EXPECT_THAT(result_get, IsOk());
  nlohmann::json expected_get =
      nlohmann::json::parse(R"json({})json", nullptr, false);
  EXPECT_THAT(result_get->body, Eq(expected_get));
  EXPECT_THAT(result_get->code, Eq(404));
}

TEST(GrpcRedfishTransport, NotAllowed) {
  int port = ecclesia::FindUnusedPortOrDie();
  GrpcDynamicMockupServer mockup_server("barebones_session_auth/mockup.shar",
                                        "[::1]", port);
  GrpcRedfishTransport transport(absl::StrCat("[::1]:", port));

  std::string_view data_post = R"json({
    "ChassisType": "RackMount",
    "Name": "MyChassis"
  })json";
  auto result_post = transport.Post("/redfish", data_post);
  EXPECT_THAT(result_post, IsOk());
  nlohmann::json expected_post =
      nlohmann::json::parse(R"json({})json", nullptr, false);
  EXPECT_THAT(result_post->body, Eq(expected_post));
  EXPECT_THAT(result_post->code, Eq(405));

  auto result_patch = transport.Patch("/redfish", data_post);
  EXPECT_THAT(result_patch, IsOk());
  nlohmann::json expected_patch =
      nlohmann::json::parse(R"json({})json", nullptr, false);
  EXPECT_THAT(result_patch->body, Eq(expected_patch));
  EXPECT_THAT(result_patch->code, Eq(204));
}

TEST(GrpcRedfishTransport, Timeout) {
  int port = ecclesia::FindUnusedPortOrDie();
  testing::internal::Notification notification;
  GrpcDynamicMockupServer mockup_server("barebones_session_auth/mockup.shar",
                                         "[::1]", port);
  mockup_server.AddHttpGetHandler(
      "/redfish/v1",
      [&](grpc::ServerContext* context, const ::redfish::v1::Request* request,
          redfish::v1::Response* response) -> grpc::Status {
        absl::SleepFor(absl::Milliseconds(100));
        notification.Notify();
        return grpc::Status::OK;
      });
  GrpcRedfishTransport::Params params;
  params.timeout = absl::AbsDuration(absl::Milliseconds(50));
  GrpcRedfishTransport transport(absl::StrCat("[::1]:", port), params);
  EXPECT_THAT(transport.Get("/redfish/v1"),
              internal_status::IsStatusPolyMatcher(
                  absl::StatusCode::kDeadlineExceeded));
  notification.WaitForNotification();
  absl::SleepFor(absl::Milliseconds(100));
}
}  // namespace
}  // namespace ecclesia
