/*
 * Copyright 2021 Google LLC
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

#include "ecclesia/lib/redfish/testing/grpc_dynamic_mockup_server.h"

#include <memory>
#include <string>

#include "google/protobuf/struct.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/http/codes.h"
#include "ecclesia/lib/network/testing.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.grpc.pb.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.pb.h"
#include "ecclesia/lib/redfish/transport/grpc.h"
#include "ecclesia/lib/redfish/transport/grpc_tls_options.h"
#include "ecclesia/lib/status/rpc.h"
#include "ecclesia/lib/testing/proto.h"
#include "ecclesia/lib/testing/status.h"
#include "grpcpp/channel.h"
#include "grpcpp/client_context.h"
#include "grpcpp/create_channel.h"
#include "grpcpp/security/credentials.h"
#include "grpcpp/server_context.h"
#include "grpcpp/support/status.h"

namespace ecclesia {
namespace {

using ::google::protobuf::Struct;
using ::redfish::v1::Request;
using ::redfish::v1::Response;
using ::testing::Eq;
using ::testing::Test;

class GrpcRedfishCredentialsInTest : public grpc::MetadataCredentialsPlugin {
 public:
  explicit GrpcRedfishCredentialsInTest(absl::string_view resource)
      : resource_(resource) {}
  // Sends out the Redfish resource as part of gRPC credentials.
  grpc::Status GetMetadata(
      grpc::string_ref /*service_url*/, grpc::string_ref /*method_name*/,
      const grpc::AuthContext & /*channel_auth_context*/,
      std::multimap<grpc::string, grpc::string> *metadata) override {
    metadata->insert(std::make_pair("redfish-resource", resource_));
    return grpc::Status::OK;
  }

 private:
  std::string resource_;
};

class GrpcRedfishMockUpServerTest : public Test {
 protected:
  GrpcRedfishMockUpServerTest() {
    StaticBufferBasedTlsOptions options;
    options.SetToInsecure();
    int port = FindUnusedPortOrDie();
    mockup_server_ = absl::make_unique<GrpcDynamicMockupServer>(
        "barebones_session_auth/mockup.shar", "[::1]", port);
    auto transport = CreateGrpcRedfishTransport(
        absl::StrCat("[::1]:", port), {}, options.GetChannelCredentials());
    if (transport.ok()) {
      client_ = std::move(*transport);
    }
    std::shared_ptr<grpc::Channel> channel = CreateChannel(
        absl::StrCat("[::1]:", port), grpc::InsecureChannelCredentials());
    stub_ = ::redfish::v1::RedfishV1::NewStub(channel);
  }

  std::unique_ptr<GrpcDynamicMockupServer> mockup_server_;
  std::unique_ptr<RedfishTransport> client_;
  std::unique_ptr<::redfish::v1::RedfishV1::Stub> stub_;
};

// Testing Post, Patch and Get requests. The Patch request can verify the
// resource has been Posted. And The Get Request can verify that the
// resource has been Patched.
TEST_F(GrpcRedfishMockUpServerTest, TestPostPatchAndGetRequest) {
  // Test Post request
  std::string_view data_post = R"json({
    "ChassisType": "RackMount",
    "Name": "MyChassis"
  })json";
  absl::StatusOr<RedfishTransport::Result> result_post =
      client_->Post("/redfish/v1/Chassis", data_post);
  ASSERT_TRUE(result_post.status().ok()) << result_post.status().message();
  EXPECT_EQ(result_post->code,
            ecclesia::HttpResponseCode::HTTP_CODE_NO_CONTENT);

  // Test Patch request
  std::string_view data_patch = R"json({
    "Name": "MyNewName"
  })json";
  absl::StatusOr<RedfishTransport::Result> result_patch =
      client_->Patch("/redfish/v1/Chassis/Member1", data_patch);
  ASSERT_TRUE(result_patch.status().ok()) << result_patch.status().message();
  EXPECT_EQ(result_patch->code,
            ecclesia::HttpResponseCode::HTTP_CODE_NO_CONTENT);

  // Test Get Request
  absl::StatusOr<RedfishTransport::Result> result_get =
      client_->Get("/redfish/v1/Chassis/Member1");
  ASSERT_TRUE(result_get.status().ok()) << result_get.status().message();
  std::string name;
  name = (result_get->body)["Name"];
  EXPECT_EQ(name, "MyNewName");
  EXPECT_EQ(result_get->code, ecclesia::HttpResponseCode::HTTP_CODE_REQUEST_OK);
}

TEST_F(GrpcRedfishMockUpServerTest, TestPutRequests) {
  grpc::ClientContext context;
  Request request;
  request.set_url("/redfish/v1");
  Response response;
  context.set_credentials(grpc::experimental::MetadataCredentialsFromPlugin(
      std::unique_ptr<grpc::MetadataCredentialsPlugin>(
          std::make_unique<GrpcRedfishCredentialsInTest>("/redfish/v1")),
      GRPC_SECURITY_NONE));
  EXPECT_THAT(AsAbslStatus(stub_->Put(&context, request, &response)),
              IsStatusUnimplemented());
}

TEST_F(GrpcRedfishMockUpServerTest, TestDeleteRequests) {
  grpc::ClientContext context;
  Request request;
  request.set_url("/redfish/v1");
  Response response;
  context.set_credentials(grpc::experimental::MetadataCredentialsFromPlugin(
      std::unique_ptr<grpc::MetadataCredentialsPlugin>(
          std::make_unique<GrpcRedfishCredentialsInTest>("/redfish/v1")),
      GRPC_SECURITY_NONE));
  EXPECT_THAT(AsAbslStatus(stub_->Delete(&context, request, &response)),
              IsStatusUnimplemented());
}

TEST_F(GrpcRedfishMockUpServerTest, TestCustomGet) {
  const google::protobuf::Struct kResponse = ParseTextProtoOrDie(R"pb(
    fields {
      key: "Name"
      value: { string_value: "MyResource" }
    }
  )pb");
  mockup_server_->AddHttpGetHandler(
      "/redfish/v1/MyResource",
      [&kResponse](grpc::ServerContext *context,
                   const ::redfish::v1::Request *request, Response *response) {
        *response->mutable_message() = kResponse;
        return grpc::Status::OK;
      });
  absl::StatusOr<RedfishTransport::Result> result_get =
      client_->Get("/redfish/v1/MyResource");
  ASSERT_TRUE(result_get.status().ok()) << result_get.status().message();
  std::string name = (result_get->body)["Name"];
  EXPECT_THAT(name, Eq("MyResource"));
}

TEST_F(GrpcRedfishMockUpServerTest, TestCustomPost) {
  const ::redfish::v1::Request kRequest = ParseTextProtoOrDie(R"pb(
    url: "/redfish/v1/MyResource"
    message {
      fields {
        key: "num"
        value: { number_value: 1 }
      }
      fields {
        key: "str"
        value: { string_value: "hi" }
      }
    }
  )pb");
  const google::protobuf::Struct kResponse = ParseTextProtoOrDie(R"pb(
    fields {
      key: "Result"
      value: { string_value: "OK" }
    }
  )pb");
  bool called = false;
  mockup_server_->AddHttpPostHandler(
      "/redfish/v1/MyResource",
      [&](grpc::ServerContext *context, const ::redfish::v1::Request *request,
          Response *response) {
        called = true;
        EXPECT_THAT(*request, EqualsProto(kRequest));
        *response->mutable_message() = kResponse;
        return grpc::Status::OK;
      });
  std::string_view data_post = R"json({
    "num": 1,
    "str": "hi"
  })json";
  absl::StatusOr<RedfishTransport::Result> result_post =
      client_->Post("/redfish/v1/MyResource", data_post);
  ASSERT_TRUE(result_post.status().ok()) << result_post.status().message();
  ASSERT_TRUE(called);
  std::string name = (result_post->body)["Result"];
  EXPECT_THAT(name, Eq("OK"));
}

TEST_F(GrpcRedfishMockUpServerTest, TestCustomPatch) {
  const ::redfish::v1::Request kRequest = ParseTextProtoOrDie(R"pb(
    url: "/redfish/v1/MyResource"
    message {
      fields {
        key: "num"
        value: { number_value: 1 }
      }
      fields {
        key: "str"
        value: { string_value: "hi" }
      }
    }
  )pb");
  const google::protobuf::Struct kResponse = ParseTextProtoOrDie(R"pb(
    fields {
      key: "Result"
      value: { string_value: "OK" }
    }
  )pb");
  bool called = false;
  mockup_server_->AddHttpPatchHandler(
      "/redfish/v1/MyResource",
      [&](grpc::ServerContext *context, const ::redfish::v1::Request *request,
          Response *response) {
        called = true;
        EXPECT_THAT(*request, EqualsProto(kRequest));
        *response->mutable_message() = kResponse;
        return grpc::Status::OK;
      });
  std::string_view data_patch = R"json({
    "num": 1,
    "str": "hi"
  })json";
  absl::StatusOr<RedfishTransport::Result> result_patch =
      client_->Patch("/redfish/v1/MyResource", data_patch);
  ASSERT_TRUE(result_patch.status().ok()) << result_patch.status().message();
  EXPECT_TRUE(called);
  std::string name = (result_patch->body)["Result"];
  EXPECT_THAT(name, Eq("OK"));
}

TEST_F(GrpcRedfishMockUpServerTest, TestPostReset) {
  bool called = false;
  // Register the handler.
  mockup_server_->AddHttpPostHandler(
      "/redfish/v1/Chassis",
      [&](grpc::ServerContext *context, const ::redfish::v1::Request *request,
          Response *response) {
        called = true;
        return grpc::Status::OK;
      });
  std::string_view data_post = R"json({
    "Id": "id",
    "Name": "MyChassis"
  })json";
  absl::StatusOr<RedfishTransport::Result> result_post =
      client_->Post("/redfish/v1/Chassis", data_post);
  ASSERT_TRUE(result_post.status().ok()) << result_post.status().message();
  EXPECT_TRUE(called);

  // Clear the registered handler.
  called = false;
  mockup_server_->ClearHandlers();
  result_post = client_->Post("/redfish/v1/Chassis", data_post);
  ASSERT_TRUE(result_post.status().ok()) << result_post.status().message();
  EXPECT_FALSE(called);
}

TEST_F(GrpcRedfishMockUpServerTest, TestPatchReset) {
  bool called = false;
  // Register the handler.
  mockup_server_->AddHttpPatchHandler(
      "/redfish/v1",
      [&](grpc::ServerContext *context, const ::redfish::v1::Request *request,
          Response *response) {
        called = true;
        return grpc::Status::OK;
      });
  std::string_view data = R"json({
    "Name": "Test Name"
  })json";
  absl::StatusOr<RedfishTransport::Result> result_patch =
      client_->Patch("/redfish/v1", data);
  ASSERT_TRUE(result_patch.status().ok()) << result_patch.status().message();
  EXPECT_TRUE(called);

  // Clear the registered handler.
  called = false;
  mockup_server_->ClearHandlers();
  result_patch = client_->Patch("/redfish/v1", data);
  ASSERT_TRUE(result_patch.status().ok()) << result_patch.status().message();
  EXPECT_FALSE(called);
}

TEST_F(GrpcRedfishMockUpServerTest, TestGetReset) {
  bool called = false;
  // Register the handler.
  mockup_server_->AddHttpGetHandler(
      "/redfish/v1",
      [&](grpc::ServerContext *context, const ::redfish::v1::Request *request,
          Response *response) {
        called = true;
        return grpc::Status::OK;
      });
  absl::StatusOr<RedfishTransport::Result> result_get =
      client_->Get("/redfish/v1");
  ASSERT_TRUE(result_get.status().ok()) << result_get.status().message();
  EXPECT_TRUE(called);

  // Clear the registered handler.
  called = false;
  mockup_server_->ClearHandlers();
  result_get = client_->Get("/redfish/v1");
  ASSERT_TRUE(result_get.status().ok()) << result_get.status().message();
  std::string name = (result_get->body)["Name"];
  EXPECT_THAT(name, Eq("Root Service"));
  EXPECT_FALSE(called);
}

TEST(GrpcRedfishMockUpServerUdsTest, TestUds) {
  absl::flat_hash_map<std::string, std::string> headers;
  std::string mockup_uds =
      absl::StrCat(GetTestTempUdsDirectory(), "/mockup.socket");
  GrpcDynamicMockupServer mockup_server("barebones_session_auth/mockup.shar",
                                        mockup_uds);
  StaticBufferBasedTlsOptions options;
  options.SetToInsecure();
  auto transport = CreateGrpcRedfishTransport(
      absl::StrCat("unix:", mockup_uds), {}, options.GetChannelCredentials());
  ASSERT_THAT(transport, IsOk());
  std::string_view expexted_str = R"json({
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
  nlohmann::json expected = nlohmann::json::parse(expexted_str, nullptr, false);
  absl::StatusOr<RedfishTransport::Result> res_get =
      (*transport)->Get("/redfish/v1");
  ASSERT_THAT(res_get, IsOk());
  EXPECT_THAT(res_get->body, Eq(expected));
}

TEST_F(GrpcRedfishMockUpServerTest, MissAuthMetadataReturnsFailure) {
  redfish::v1::Request request;
  request.set_url("/redfish/v1");
  redfish::v1::Response response;
  grpc::ClientContext context;
  EXPECT_THAT(AsAbslStatus(stub_->Get(&context, request, &response)),
              IsStatusFailedPrecondition());
}

TEST_F(GrpcRedfishMockUpServerTest,
       AuthMetadataResourceAndRequestResourceMismatchReturnsFailure) {
  redfish::v1::Request request;
  request.set_url("/redfish/v1");
  redfish::v1::Response response;
  {
    grpc::ClientContext context;
    context.set_credentials(grpc::experimental::MetadataCredentialsFromPlugin(
        std::unique_ptr<grpc::MetadataCredentialsPlugin>(
            std::make_unique<GrpcRedfishCredentialsInTest>("/redfish/v2")),
        GRPC_SECURITY_NONE));
    EXPECT_THAT(AsAbslStatus(stub_->Get(&context, request, &response)),
                IsStatusFailedPrecondition());
  }
  {
    grpc::ClientContext context;
    context.set_credentials(grpc::experimental::MetadataCredentialsFromPlugin(
        std::unique_ptr<grpc::MetadataCredentialsPlugin>(
            std::make_unique<GrpcRedfishCredentialsInTest>("/redfish/v2")),
        GRPC_SECURITY_NONE));
    EXPECT_THAT(AsAbslStatus(stub_->Patch(&context, request, &response)),
                IsStatusFailedPrecondition());
  }
  {
    grpc::ClientContext context;
    context.set_credentials(grpc::experimental::MetadataCredentialsFromPlugin(
        std::unique_ptr<grpc::MetadataCredentialsPlugin>(
            std::make_unique<GrpcRedfishCredentialsInTest>("/redfish/v2")),
        GRPC_SECURITY_NONE));
    EXPECT_THAT(AsAbslStatus(stub_->Delete(&context, request, &response)),
                IsStatusFailedPrecondition());
  }
  {
    grpc::ClientContext context;
    context.set_credentials(grpc::experimental::MetadataCredentialsFromPlugin(
        std::unique_ptr<grpc::MetadataCredentialsPlugin>(
            std::make_unique<GrpcRedfishCredentialsInTest>("/redfish/v2")),
        GRPC_SECURITY_NONE));
    EXPECT_THAT(AsAbslStatus(stub_->Put(&context, request, &response)),
                IsStatusFailedPrecondition());
  }
  {
    grpc::ClientContext context;
    context.set_credentials(grpc::experimental::MetadataCredentialsFromPlugin(
        std::unique_ptr<grpc::MetadataCredentialsPlugin>(
            std::make_unique<GrpcRedfishCredentialsInTest>("/redfish/v2")),
        GRPC_SECURITY_NONE));
    EXPECT_THAT(AsAbslStatus(stub_->Post(&context, request, &response)),
                IsStatusFailedPrecondition());
  }
}

TEST_F(GrpcRedfishMockUpServerTest,
       AuthMetadataResourceAndRequestResourceMatchReturnsOk) {
  bool called = false;
  // Register the handler.
  mockup_server_->AddHttpGetHandler(
      "/redfish/v1",
      [&](grpc::ServerContext *context, const ::redfish::v1::Request *request,
          Response *response) {
        called = true;
        return grpc::Status::OK;
      });
  redfish::v1::Request request;
  request.set_url("/redfish/v1");
  redfish::v1::Response response;
  grpc::ClientContext context;
  context.set_credentials(grpc::experimental::MetadataCredentialsFromPlugin(
      std::unique_ptr<grpc::MetadataCredentialsPlugin>(
          std::make_unique<GrpcRedfishCredentialsInTest>("/redfish/v1")),
      GRPC_SECURITY_NONE));
  EXPECT_THAT(AsAbslStatus(stub_->Get(&context, request, &response)), IsOk());
  EXPECT_TRUE(called);
}

}  // namespace
}  // namespace ecclesia
