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
#include "ecclesia/lib/network/testing.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.grpc.pb.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.pb.h"
#include "ecclesia/lib/redfish/transport/grpc_dynamic_impl.h"
#include "ecclesia/lib/redfish/transport/grpc_dynamic_options.h"
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
using ::libredfish::RedfishInterface;
using ::libredfish::RedfishVariant;
using ::redfish::v1::Request;
using ::testing::Eq;
using ::testing::Test;

class GrpcRedfishMockUpServerTest : public Test {
 protected:
  GrpcRedfishMockUpServerTest() {
    GrpcDynamicImplOptions options;
    options.SetToInsecure();
    int port = FindUnusedPortOrDie();
    mockup_server_ = absl::make_unique<GrpcDynamicMockupServer>(
        "barebones_session_auth/mockup.shar", "[::1]", port);
    client_ = absl::make_unique<GrpcDynamicImpl>(
        GrpcDynamicImpl::Target{.fqdn = "[::1]", .port = port},
        RedfishInterface::TrustedEndpoint::kUntrusted, options);
    std::shared_ptr<grpc::Channel> channel = CreateChannel(
        absl::StrCat("[::1]:", port), grpc::InsecureChannelCredentials());
    stub_ = ::redfish::v1::RedfishV1::NewStub(channel);
  }

  std::unique_ptr<GrpcDynamicMockupServer> mockup_server_;
  std::unique_ptr<RedfishInterface> client_;
  std::unique_ptr<::redfish::v1::RedfishV1::Stub> stub_;
};

// Testing Post, Patch and Get requests. The Patch request can verify the
// resource has been Posted. And The Get Request can verify that the
// resource has been Patched.
TEST_F(GrpcRedfishMockUpServerTest, TestPostPatchAndGetRequest) {
  RedfishVariant post_redfish_variant = client_->PostUri(
      "/redfish/v1/Chassis", {{"ChassisType", "RackMount"}, {"Name", "MyChassis"}});
  EXPECT_TRUE(post_redfish_variant.status().ok())
      << post_redfish_variant.status().message();

  // Test Patch request
  EXPECT_TRUE(
      client_->PatchUri("/redfish/v1/Chassis/Member1", {{"Name", "MyNewName"}})
          .status()
          .ok())
      << post_redfish_variant.status().message();

  // Test Get Request
  RedfishVariant get_redfish_variant =
      client_->GetUri("/redfish/v1/Chassis/Member1");
  EXPECT_TRUE(get_redfish_variant.status().ok())
      << post_redfish_variant.status().message();
  std::string name;
  get_redfish_variant["Name"].GetValue(&name);
  EXPECT_EQ(name, "MyNewName");
}

TEST_F(GrpcRedfishMockUpServerTest, TestPutRequests) {
  grpc::ClientContext context;
  Request request;
  Struct response;
  EXPECT_THAT(AsAbslStatus(stub_->Put(&context, request, &response)),
              IsStatusUnimplemented());
}

TEST_F(GrpcRedfishMockUpServerTest, TestDeleteRequests) {
  grpc::ClientContext context;
  Request request;
  Struct response;
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
                   const ::redfish::v1::Request *request,
                   google::protobuf::Struct *response) {
        *response = kResponse;
        return grpc::Status::OK;
      });
  RedfishVariant get_redfish_variant =
      client_->GetUri("/redfish/v1/MyResource");
  EXPECT_TRUE(get_redfish_variant.status().ok())
      << get_redfish_variant.status().message();
  std::string name;
  get_redfish_variant["Name"].GetValue(&name);
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
          google::protobuf::Struct *response) {
        called = true;
        EXPECT_THAT(*request, EqualsProto(kRequest));
        *response = kResponse;
        return grpc::Status::OK;
      });
  RedfishVariant get_redfish_variant =
      client_->PostUri("/redfish/v1/MyResource", {{"num", 1}, {"str", "hi"}});
  EXPECT_TRUE(get_redfish_variant.status().ok())
      << get_redfish_variant.status().message();
  ASSERT_TRUE(called);
  std::string name;
  get_redfish_variant["Result"].GetValue(&name);
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
          google::protobuf::Struct *response) {
        called = true;
        EXPECT_THAT(*request, EqualsProto(kRequest));
        *response = kResponse;
        return grpc::Status::OK;
      });
  RedfishVariant get_redfish_variant =
      client_->PatchUri("/redfish/v1/MyResource", {{"num", 1}, {"str", "hi"}});
  EXPECT_TRUE(get_redfish_variant.status().ok())
      << get_redfish_variant.status().message();
  ASSERT_TRUE(called);
  std::string name;
  get_redfish_variant["Result"].GetValue(&name);
  EXPECT_THAT(name, Eq("OK"));
}

TEST_F(GrpcRedfishMockUpServerTest, TestPostReset) {
  bool called = false;
  // Register the handler.
  mockup_server_->AddHttpPostHandler(
      "/redfish/v1/Chassis",
      [&](grpc::ServerContext *context, const ::redfish::v1::Request *request,
          google::protobuf::Struct *response) {
        called = true;
        return grpc::Status::OK;
      });
  auto post_result = client_->PostUri("/redfish/v1/Chassis",
                                      {{"Id", "id"}, {"Name", "MyChassis"}});
  EXPECT_TRUE(post_result.status().ok()) << post_result.status().message();
  EXPECT_TRUE(called);

  // Clear the registered handler.
  called = false;
  mockup_server_->ClearHandlers();
  post_result = client_->PostUri("/redfish/v1/Chassis",
                                 {{"Id", "id"}, {"Name", "MyChassis"}});
  EXPECT_TRUE(post_result.status().ok()) << post_result.status().message();
  EXPECT_FALSE(called);
}

TEST_F(GrpcRedfishMockUpServerTest, TestPatchReset) {
  bool called = false;
  // Register the handler.
  mockup_server_->AddHttpPatchHandler(
      "/redfish/v1",
      [&](grpc::ServerContext *context, const ::redfish::v1::Request *request,
          google::protobuf::Struct *response) {
        called = true;
        return grpc::Status::OK;
      });
  auto patch_result = client_->PatchUri("/redfish/v1", {{"Name", "Test Name"}});
  EXPECT_TRUE(patch_result.status().ok()) << patch_result.status().message();
  EXPECT_TRUE(called);

  // Clear the registered handler.
  called = false;
  mockup_server_->ClearHandlers();
  patch_result = client_->PatchUri("/redfish/v1", {{"Name", "Test Name"}});
  EXPECT_TRUE(patch_result.status().ok()) << patch_result.status().message();
  EXPECT_FALSE(called);
}

TEST_F(GrpcRedfishMockUpServerTest, TestGetReset) {
  bool called = false;
  // Register the handler.
  mockup_server_->AddHttpGetHandler(
      "/redfish/v1",
      [&](grpc::ServerContext *context, const ::redfish::v1::Request *request,
          google::protobuf::Struct *response) {
        called = true;
        return grpc::Status::OK;
      });
  auto get_result = client_->GetUri("/redfish/v1");
  EXPECT_TRUE(get_result.status().ok()) << get_result.status().message();
  EXPECT_TRUE(called);

  // Clear the registered handler.
  called = false;
  mockup_server_->ClearHandlers();
  get_result = client_->GetUri("/redfish/v1");
  ASSERT_TRUE(get_result.status().ok()) << get_result.status().message();
  std::string name;
  EXPECT_TRUE(get_result["Name"].GetValue(&name));
  EXPECT_THAT(name, Eq("Root Service"));
  EXPECT_FALSE(called);
}

}  // namespace
}  // namespace ecclesia
