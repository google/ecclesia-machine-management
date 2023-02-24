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

#include <memory>
#include <optional>
#include <string>
#include <variant>

#include "google/protobuf/struct.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ecclesia/lib/network/testing.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.pb.h"
#include "ecclesia/lib/redfish/testing/grpc_dynamic_mockup_server.h"
#include "ecclesia/lib/redfish/transport/grpc_tls_options.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/redfish/transport/struct_proto_conversion.h"
#include "ecclesia/lib/redfish/utils.h"
#include "ecclesia/lib/testing/status.h"
#include "ecclesia/lib/time/clock_fake.h"
#include "grpcpp/server_context.h"
#include "grpcpp/support/status.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {
namespace {

using ::redfish::v1::Response;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::Pair;

TEST(GrpcRedfishTransport, Get) {
  absl::flat_hash_map<std::string, std::string> headers;
  GrpcDynamicMockupServer mockup_server("barebones_session_auth/mockup.shar",
                                        "localhost", 0);
  StaticBufferBasedTlsOptions options;
  options.SetToInsecure();
  auto port = mockup_server.Port();
  ASSERT_TRUE(port.has_value());
  auto transport = CreateGrpcRedfishTransport(
      absl::StrCat("localhost:", *port), {}, options.GetChannelCredentials());
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
  ASSERT_TRUE(std::holds_alternative<nlohmann::json>(res_get->body));
  EXPECT_THAT(std::get<nlohmann::json>(res_get->body), Eq(expected));
  EXPECT_THAT(res_get->code, Eq(200));
}

TEST(GrpcRedfishTransport, PostPatchGetDelete) {
  absl::flat_hash_map<std::string, std::string> headers;
  GrpcDynamicMockupServer mockup_server("barebones_session_auth/mockup.shar",
                                        "localhost", 0);
  StaticBufferBasedTlsOptions options;
  options.SetToInsecure();
  auto port = mockup_server.Port();
  ASSERT_TRUE(port.has_value());
  auto transport = CreateGrpcRedfishTransport(
      absl::StrCat("localhost:", *port), {}, options.GetChannelCredentials());
  ASSERT_THAT(transport, IsOk());
  absl::StatusOr<RedfishTransport::Result> res_post =
      (*transport)->Post("/redfish/v1/Chassis", R"json({
    "ChassisType": "RackMount",
    "Name": "MyChassis"
  })json");
  ASSERT_THAT(res_post, IsOk());
  // Expect HTTP 204 (No Content) code and the body is unset (default to
  // discarded)
  EXPECT_THAT(res_post->code, Eq(204));
  ASSERT_TRUE(std::holds_alternative<nlohmann::json>(res_post->body));
  EXPECT_TRUE(std::get<nlohmann::json>(res_post->body).is_discarded());

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
    "Members@odata.count":2,
    "Name":"Chassis Collection"
  })json";
  nlohmann::json expected_get =
      nlohmann::json::parse(expected_get_str, nullptr, false);
  absl::StatusOr<RedfishTransport::Result> res_get =
      (*transport)->Get("/redfish/v1/Chassis");
  ASSERT_THAT(res_get, IsOk());
  ASSERT_TRUE(std::holds_alternative<nlohmann::json>(res_get->body));
  EXPECT_THAT(std::get<nlohmann::json>(res_get->body), Eq(expected_get));
  EXPECT_THAT(res_get->code, Eq(200));

  absl::string_view data_patch = R"json({
    "Name": "MyPatchChassis"
  })json";
  absl::StatusOr<RedfishTransport::Result> res_patch =
      (*transport)->Patch("/redfish/v1/Chassis/Member1", data_patch);
  ASSERT_THAT(res_patch, IsOk());
  // Expect HTTP 204 (No Content) code and the body is unset (default to
  // discarded)
  EXPECT_THAT(res_patch->code, Eq(204));
  ASSERT_TRUE(std::holds_alternative<nlohmann::json>(res_patch->body));
  EXPECT_TRUE(std::get<nlohmann::json>(res_patch->body).is_discarded());

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
  ASSERT_TRUE(std::holds_alternative<nlohmann::json>(res_get->body));
  EXPECT_THAT(std::get<nlohmann::json>(res_get->body), Eq(expected_get));
  EXPECT_THAT(res_get->code, Eq(200));

  EXPECT_THAT(
      (*transport)->Delete("/redfish/v1/Chassis/Member1", "{}"),
      internal_status::IsStatusPolyMatcher(absl::StatusCode::kUnimplemented));
}

TEST(GrpcRedfishTransport, GetRootUri) {
  GrpcDynamicMockupServer mockup_server("barebones_session_auth/mockup.shar",
                                        "localhost", 0);
  StaticBufferBasedTlsOptions options;
  options.SetToInsecure();
  auto port = mockup_server.Port();
  ASSERT_TRUE(port.has_value());
  auto transport = CreateGrpcRedfishTransport(
      absl::StrCat("localhost:", *port), {}, options.GetChannelCredentials());
  ASSERT_THAT(transport, IsOk());
  EXPECT_EQ((*transport)->GetRootUri(), "/redfish/v1");
}

TEST(GrpcRedfishTransport, ResourceNotFound) {
  GrpcDynamicMockupServer mockup_server("barebones_session_auth/mockup.shar",
                                        "localhost", 0);
  StaticBufferBasedTlsOptions options;
  options.SetToInsecure();
  auto port = mockup_server.Port();
  ASSERT_TRUE(port.has_value());
  auto transport = CreateGrpcRedfishTransport(
      absl::StrCat("localhost:", *port), {}, options.GetChannelCredentials());
  ASSERT_THAT(transport, IsOk());

  auto result_get = (*transport)->Get("/redfish/v1/Chassis/noexist");
  ASSERT_THAT(result_get, IsOk());

  // Expect HTTP 404 (Not found) code and the body is unset (default to
  // discarded)
  EXPECT_THAT(result_get->code, Eq(404));
  ASSERT_TRUE(std::holds_alternative<nlohmann::json>(result_get->body));
  EXPECT_TRUE(std::get<nlohmann::json>(result_get->body).is_discarded());
}

TEST(GrpcRedfishTransport, NotAllowed) {
  GrpcDynamicMockupServer mockup_server("barebones_session_auth/mockup.shar",
                                        "localhost", 0);
  StaticBufferBasedTlsOptions options;
  options.SetToInsecure();
  auto port = mockup_server.Port();
  ASSERT_TRUE(port.has_value());
  auto transport = CreateGrpcRedfishTransport(
      absl::StrCat("localhost:", *port), {}, options.GetChannelCredentials());
  ASSERT_THAT(transport, IsOk());

  absl::string_view data_post = R"json({
    "ChassisType": "RackMount",
    "Name": "MyChassis"
  })json";
  auto result_post = (*transport)->Post("/redfish", data_post);
  ASSERT_THAT(result_post, IsOk());
  // Expect HTTP 405 (Method Not Allowed) code and the body is unset (default to
  // discarded)
  EXPECT_THAT(result_post->code, Eq(405));
  ASSERT_TRUE(std::holds_alternative<nlohmann::json>(result_post->body));
  EXPECT_TRUE(std::get<nlohmann::json>(result_post->body).is_discarded());

  auto result_patch = (*transport)->Patch("/redfish", data_post);
  ASSERT_THAT(result_patch, IsOk());
  // Expect HTTP 204 (No Content) code and the body is unset (default to
  // discarded)
  EXPECT_THAT(result_patch->code, Eq(204));
  ASSERT_TRUE(std::holds_alternative<nlohmann::json>(result_patch->body));
  EXPECT_TRUE(std::get<nlohmann::json>(result_patch->body).is_discarded());
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

  std::string endpoint = absl::StrCat("localhost:", port);
  testing::internal::Notification notification;
  GrpcDynamicMockupServer mockup_server("barebones_session_auth/mockup.shar",
                                        "localhost", port);

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

  transport =
      CreateGrpcRedfishTransport("google:///test.blade.gslb.googleprod.com", {},
                                 options.GetChannelCredentials());
  EXPECT_THAT(transport, IsOk());
}

TEST(GrpcRedfishTransport, GetCorrectResultBodyAndRequestHeaders) {
  GrpcDynamicMockupServer mockup_server("barebones_session_auth/mockup.shar",
                                        "localhost", 0);
  StaticBufferBasedTlsOptions options;
  options.SetToInsecure();
  auto port = mockup_server.Port();
  ASSERT_TRUE(port.has_value());
  auto transport = CreateGrpcRedfishTransport(
      absl::StrCat("localhost:", *port), {}, options.GetChannelCredentials());
  ASSERT_THAT(transport, IsOk());

  nlohmann::json json_body = nlohmann::json::parse(R"json_str({
                                "@odata.id": "/redfish/v1/json_resource",
                                "Type": "JSON"
                                })json_str");

  google::protobuf::Struct json_body_struct = JsonToStruct(json_body);

  // When the gRPC response is returning a JSON struct, expect the result body
  // from RedfishTransport Get to be set to JSON.
  mockup_server.AddHttpGetHandler(
      "/redfish/v1/json_resource",
      [&](grpc::ServerContext *context, const ::redfish::v1::Request *request,
          Response *response) {
        auto it = request->headers().find("Host");
        EXPECT_NE(it, request->headers().end());
        if (it != request->headers().end()) {
          EXPECT_EQ(it->second, "localhost");
        }
        *response->mutable_json() = json_body_struct;
        response->set_code(200);
        response->mutable_headers()->insert({"OData-Version", "4.0"});
        return grpc::Status::OK;
      });
  auto result = (*transport)->Get("/redfish/v1/json_resource");
  ASSERT_THAT(result, IsOk());
  ASSERT_TRUE(std::holds_alternative<nlohmann::json>(result->body));
  EXPECT_THAT(std::get<nlohmann::json>(result->body), Eq(json_body));
  EXPECT_THAT(result->code, Eq(200));
  EXPECT_THAT(result->headers, Contains(Pair("OData-Version", "4.0")));

  // When the gRPC response is returning an octet stream, expect the result body
  // from RedfishTransport Get to be set to bytes.
  std::string octet_stream = "octet_stream 123456789abcxyz";
  mockup_server.AddHttpGetHandler(
      "/redfish/v1/octet_stream",
      [&](grpc::ServerContext *context, const ::redfish::v1::Request *request,
          Response *response) {
        *response->mutable_octet_stream() = octet_stream;
        response->set_code(200);
        response->mutable_headers()->insert({"OData-Version", "4.0"});
        return grpc::Status::OK;
      });
  result = (*transport)->Get("/redfish/v1/octet_stream");
  ASSERT_THAT(result, IsOk());
  ASSERT_TRUE(std::holds_alternative<RedfishTransport::bytes>(result->body));
  EXPECT_THAT(RedfishTransportBytesToString(
                  std::get<RedfishTransport::bytes>(result->body)),
              Eq(octet_stream));
  EXPECT_THAT(result->code, Eq(200));
  EXPECT_THAT(result->headers, Contains(Pair("OData-Version", "4.0")));
}

}  // namespace
}  // namespace ecclesia
