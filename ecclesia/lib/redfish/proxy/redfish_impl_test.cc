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

#include "ecclesia/lib/redfish/proxy/redfish_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/str_format.h"
#include "ecclesia/lib/file/path.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.grpc.pb.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.pb.h"
#include "ecclesia/lib/status/rpc.h"
#include "ecclesia/lib/testing/proto.h"
#include "ecclesia/lib/testing/status.h"
#include "grpc/grpc_security_constants.h"
#include "grpcpp/client_context.h"
#include "grpcpp/create_channel.h"
#include "grpcpp/security/credentials.h"
#include "grpcpp/security/server_credentials.h"
#include "grpcpp/server.h"
#include "grpcpp/server_builder.h"
#include "grpcpp/support/status.h"

namespace ecclesia {

class MockRedfishTransport : public RedfishTransport {
 public:
  MOCK_METHOD(absl::StatusOr<Result>, Get, (absl::string_view));
  MOCK_METHOD(absl::StatusOr<Result>, Post,
              (absl::string_view, absl::string_view));
  MOCK_METHOD(absl::StatusOr<Result>, Patch,
              (absl::string_view, absl::string_view));
  MOCK_METHOD(absl::StatusOr<Result>, Delete,
              (absl::string_view, absl::string_view));
  MOCK_METHOD(absl::string_view, GetRootUri, ());
};

namespace {

using ::testing::_;
using ::testing::Return;
using ::testing::StrictMock;

// Typed test for all GRPC proxy tests. This is parameterized on the verb being
// tested, using one of the RedfishXXX objects defined below (e.g. RedfishGet).
class HttpProxyTest : public ::testing::Test {
 protected:
  HttpProxyTest()
      : proxy_("test-proxy", &mock_redfish_transport_),
        proxy_uds_(JoinFilePaths(GetTestTempUdsDirectory(), "mm.socket")) {}
  void StartProxy() {
    auto creds = grpc::experimental::LocalServerCredentials(UDS);
    grpc::ServerBuilder builder;
    builder.AddListeningPort(absl::StrFormat("unix:%s", proxy_uds_), creds);
    builder.RegisterService(&proxy_);
    proxy_server_ = builder.BuildAndStart();
  }
  std::unique_ptr<GrpcRedfishV1::Stub> MakeProxyStub() {
    auto creds = grpc::experimental::LocalCredentials(UDS);
    auto channel =
        grpc::CreateChannel(absl::StrFormat("unix:%s", proxy_uds_), creds);
    return GrpcRedfishV1::RedfishV1::NewStub(channel);
  }
  StrictMock<MockRedfishTransport> mock_redfish_transport_;
  RedfishProxyRedfishBackend proxy_;
  std::string proxy_uds_;
  std::unique_ptr<grpc::Server> proxy_server_;
};

TEST_F(HttpProxyTest, GetTest) {
  this->StartProxy();
  auto stub = this->MakeProxyStub();
  RedfishTransport::Result mock_http_response;
  mock_http_response.code = 200;
  mock_http_response.body = "http transport success.";
  EXPECT_CALL(this->mock_redfish_transport_, Get(_))
      .WillOnce(Return(mock_http_response));
  grpc::ClientContext context;
  redfish::v1::Request request;
  request.set_url("/a/b/c");
  request.set_json_str("http transport.");
  redfish::v1::Response response;
  redfish::v1::Response mock_grpc_response;
  mock_grpc_response.set_code(200);
  mock_grpc_response.set_json_str(R"json("http transport success.")json");
  EXPECT_THAT(AsAbslStatus(stub->Get(&context, request, &response)), IsOk());
  EXPECT_THAT(response, EqualsProto(mock_grpc_response));
}

TEST_F(HttpProxyTest, GetTestFail) {
  this->StartProxy();
  auto stub = this->MakeProxyStub();
  RedfishTransport::Result mock_http_response;
  mock_http_response.code = 200;
  mock_http_response.body = "http transport success.";
  EXPECT_CALL(this->mock_redfish_transport_, Get(_))
      .WillOnce(Return(absl::AbortedError("bad mode")));
  grpc::ClientContext context;
  redfish::v1::Request request;
  request.set_url("/a/b/c");
  request.set_json_str("http transport.");
  redfish::v1::Response response;
  grpc::Status status = stub->Get(&context, request, &response);
  EXPECT_THAT(status.error_code(), grpc::StatusCode::INTERNAL);
}

TEST_F(HttpProxyTest, PostTest) {
  this->StartProxy();
  auto stub = this->MakeProxyStub();
  RedfishTransport::Result mock_http_response;
  mock_http_response.code = 200;
  mock_http_response.body = "http transport success.";
  EXPECT_CALL(this->mock_redfish_transport_, Post(_, _))
      .WillOnce(Return(mock_http_response));
  grpc::ClientContext context;
  redfish::v1::Request request;
  request.set_url("/a/b/c");
  request.set_json_str("http transport.");
  redfish::v1::Response response;
  redfish::v1::Response mock_grpc_response;
  mock_grpc_response.set_code(200);
  mock_grpc_response.set_json_str(R"json("http transport success.")json");
  EXPECT_THAT(AsAbslStatus(stub->Post(&context, request, &response)), IsOk());
  EXPECT_THAT(response, EqualsProto(mock_grpc_response));
}

TEST_F(HttpProxyTest, PostTestFail) {
  this->StartProxy();
  auto stub = this->MakeProxyStub();
  RedfishTransport::Result mock_http_response;
  mock_http_response.code = 200;
  mock_http_response.body = "http transport success.";
  EXPECT_CALL(this->mock_redfish_transport_, Post(_, _))
      .WillOnce(Return(absl::AbortedError("bad mode")));
  grpc::ClientContext context;
  redfish::v1::Request request;
  request.set_url("/a/b/c");
  request.set_json_str("http transport.");
  redfish::v1::Response response;
  grpc::Status status = stub->Post(&context, request, &response);
  EXPECT_THAT(status.error_code(), grpc::StatusCode::INTERNAL);
}

TEST_F(HttpProxyTest, PutTest) {
  this->StartProxy();
  auto stub = this->MakeProxyStub();
  RedfishTransport::Result mock_http_response;
  mock_http_response.code = 200;
  mock_http_response.body = "http transport success.";
  grpc::ClientContext context;
  redfish::v1::Request request;
  request.set_url("/a/b/c");
  request.set_json_str("http transport.");
  redfish::v1::Response response;
  grpc::Status status = stub->Put(&context, request, &response);
  EXPECT_THAT(status.error_code(), grpc::StatusCode::UNIMPLEMENTED);
}

TEST_F(HttpProxyTest, PatchTest) {
  this->StartProxy();
  auto stub = this->MakeProxyStub();
  RedfishTransport::Result mock_http_response;
  mock_http_response.code = 200;
  mock_http_response.body = "http transport success.";
  EXPECT_CALL(this->mock_redfish_transport_, Patch(_, _))
      .WillOnce(Return(mock_http_response));
  grpc::ClientContext context;
  redfish::v1::Request request;
  request.set_url("/a/b/c");
  request.set_json_str("http transport.");
  redfish::v1::Response response;
  redfish::v1::Response mock_grpc_response;
  mock_grpc_response.set_code(200);
  mock_grpc_response.set_json_str(R"json("http transport success.")json");
  EXPECT_THAT(AsAbslStatus(stub->Patch(&context, request, &response)), IsOk());
  EXPECT_THAT(response, EqualsProto(mock_grpc_response));
}

TEST_F(HttpProxyTest, DeleteTest) {
  this->StartProxy();
  auto stub = this->MakeProxyStub();
  RedfishTransport::Result mock_http_response;
  mock_http_response.code = 200;
  mock_http_response.body = "http transport success.";
  EXPECT_CALL(this->mock_redfish_transport_, Delete(_, _))
      .WillOnce(Return(mock_http_response));
  grpc::ClientContext context;
  redfish::v1::Request request;
  request.set_url("/a/b/c");
  request.set_json_str("http transport.");
  redfish::v1::Response response;
  redfish::v1::Response mock_grpc_response;
  mock_grpc_response.set_code(200);
  mock_grpc_response.set_json_str(R"json("http transport success.")json");
  EXPECT_THAT(AsAbslStatus(stub->Delete(&context, request, &response)), IsOk());
  EXPECT_THAT(response, EqualsProto(mock_grpc_response));
}

}  // namespace
}  // namespace ecclesia
