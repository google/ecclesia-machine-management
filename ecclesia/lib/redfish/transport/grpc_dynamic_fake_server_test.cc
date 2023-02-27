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

#include "ecclesia/lib/redfish/transport/grpc_dynamic_fake_server.h"

#include <memory>
#include <string>

#include "google/protobuf/struct.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.grpc.pb.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.pb.h"
#include "ecclesia/lib/testing/proto.h"
#include "grpcpp/channel.h"
#include "grpcpp/client_context.h"
#include "grpcpp/create_channel.h"
#include "grpcpp/security/credentials.h"
#include "grpcpp/server_context.h"
#include "grpcpp/support/status.h"
#include "grpcpp/support/time.h"  // IWYU pragma: keep

namespace ecclesia {
namespace {

using ::google::protobuf::Struct;
using ::google::protobuf::Value;
using ::redfish::v1::Request;
using ::redfish::v1::Response;

TEST(FakeServerTest, InsecureChannelDefault) {
  GrpcDynamicFakeServer fake_server;
  std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel(
      fake_server.GetHostPort(), grpc::InsecureChannelCredentials());
  std::unique_ptr<GrpcRedfishV1::Stub> stub = GrpcRedfishV1::NewStub(channel);
  grpc::ClientContext context;
  context.set_deadline(absl::ToChronoTime(absl::Now() + absl::Seconds(1)));
  Request request;
  EXPECT_EQ(stub->Get(&context, request, nullptr).error_code(),
            grpc::StatusCode::CANCELLED);
}

TEST(FakeServerTest, InsecureChannelOk) {
  GrpcDynamicFakeServer fake_server;
  std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel(
      fake_server.GetHostPort(), grpc::InsecureChannelCredentials());
  std::unique_ptr<GrpcRedfishV1::Stub> stub = GrpcRedfishV1::NewStub(channel);
  Value value;
  value.set_string_value("world");
  Struct expected_response;
  expected_response.mutable_fields()->insert({"hello", value});
  std::string expected_url = "/fake";
  fake_server.SetCallback([expected_response, expected_url](
                              grpc::ServerContext *context,
                              const Request *request, Response *response) {
    EXPECT_EQ(request->url(), expected_url);
    *response->mutable_json() = expected_response;
    return grpc::Status::OK;
  });
  grpc::ClientContext context;
  context.set_deadline(absl::ToChronoTime(absl::Now() + absl::Seconds(1)));
  Response response;
  Request request;
  request.set_url(expected_url);
  EXPECT_TRUE(stub->Get(&context, request, &response).ok());
  EXPECT_THAT(response.json(), EqualsProto(expected_response));
}

TEST(FakeServerTest, InsecureChannelNotOk) {
  GrpcDynamicFakeServer fake_server;
  std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel(
      fake_server.GetHostPort(), grpc::InsecureChannelCredentials());
  std::unique_ptr<GrpcRedfishV1::Stub> stub = GrpcRedfishV1::NewStub(channel);

  fake_server.SetCallback(
      [](grpc::ServerContext *context, const Request *request,
         Response *response) { return grpc::Status::CANCELLED; });
  Request request;
  {
    SCOPED_TRACE("Get returns CANCELLED status");
    grpc::ClientContext context;
    context.set_deadline(absl::ToChronoTime(absl::Now() + absl::Seconds(1)));
    EXPECT_EQ(stub->Get(&context, request, nullptr).error_code(),
              grpc::StatusCode::CANCELLED);
  }
  {
    SCOPED_TRACE("Post returns CANCELLED status");
    grpc::ClientContext context;
    context.set_deadline(absl::ToChronoTime(absl::Now() + absl::Seconds(1)));
    EXPECT_EQ(stub->Post(&context, request, nullptr).error_code(),
              grpc::StatusCode::CANCELLED);
  }
  {
    SCOPED_TRACE("Delete returns CANCELLED status");
    grpc::ClientContext context;
    context.set_deadline(absl::ToChronoTime(absl::Now() + absl::Seconds(1)));
    EXPECT_EQ(stub->Delete(&context, request, nullptr).error_code(),
              grpc::StatusCode::CANCELLED);
  }
  {
    SCOPED_TRACE("Patch returns CANCELLED status");
    grpc::ClientContext context;
    context.set_deadline(absl::ToChronoTime(absl::Now() + absl::Seconds(1)));
    EXPECT_EQ(stub->Patch(&context, request, nullptr).error_code(),
              grpc::StatusCode::CANCELLED);
  }
  {
    SCOPED_TRACE("Put returns CANCELLED status");
    grpc::ClientContext context;
    context.set_deadline(absl::ToChronoTime(absl::Now() + absl::Seconds(1)));
    EXPECT_EQ(stub->Put(&context, request, nullptr).error_code(),
              grpc::StatusCode::CANCELLED);
  }
}

}  // namespace
}  // namespace ecclesia
