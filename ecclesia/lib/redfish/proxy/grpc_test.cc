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

#include "ecclesia/lib/redfish/proxy/grpc.h"

#include <memory>
#include <string>

#include "google/protobuf/struct.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/str_format.h"
#include "ecclesia/lib/file/path.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.grpc.pb.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.pb.h"
#include "ecclesia/lib/redfish/proto/redfish_v1_mock.grpc.pb.h"
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
namespace {

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrictMock;

class GrpcProxyTest : public ::testing::Test {
 protected:
  GrpcProxyTest()
      : proxy_("test-proxy", &mock_stub_),
        proxy_uds_(JoinFilePaths(GetTestTempUdsDirectory(), "mm.socket")) {}

  void StartProxy() {
    auto creds = grpc::experimental::LocalServerCredentials(UDS);
    grpc::ServerBuilder builder;
    builder.AddListeningPort(absl::StrFormat("unix:%s", proxy_uds_), creds);
    builder.RegisterService(&proxy_);
    proxy_server_ = builder.BuildAndStart();
  }

  std::unique_ptr<redfish::v1::RedfishV1::Stub> MakeProxyStub() {
    auto creds = grpc::experimental::LocalCredentials(UDS);
    auto channel =
        grpc::CreateChannel(absl::StrFormat("unix:%s", proxy_uds_), creds);
    return redfish::v1::RedfishV1::NewStub(channel);
  }

  StrictMock<redfish::v1::MockRedfishV1Stub> mock_stub_;
  RedfishV1GrpcProxy proxy_;

  std::string proxy_uds_;
  std::unique_ptr<grpc::Server> proxy_server_;
};

TEST_F(GrpcProxyTest, GetTest) {
  StartProxy();
  auto stub = MakeProxyStub();

  redfish::v1::Response mock_response =
      ParseTextProtoOrDie(R"pb(message {
                                 fields {
                                   key: "f"
                                   value { number_value: 1.5 }
                                 }
                               }
                               code: 200)pb");
  EXPECT_CALL(mock_stub_,
              Get(_, EqualsProto(R"pb(url: "/a/b/c"
                                      message {
                                        fields {
                                          key: "d"
                                          value { string_value: "e" }
                                        }
                                      })pb"),
                  _))
      .WillOnce(
          DoAll(SetArgPointee<2>(mock_response), Return(grpc::Status::OK)));

  grpc::ClientContext context;
  redfish::v1::Request request;
  request.set_url("/a/b/c");
  (*request.mutable_message()->mutable_fields())["d"].set_string_value("e");
  redfish::v1::Response response;
  EXPECT_THAT(AsAbslStatus(stub->Get(&context, request, &response)), IsOk());
  EXPECT_THAT(response, EqualsProto(mock_response));
}

TEST_F(GrpcProxyTest, PostTest) {
  StartProxy();
  auto stub = MakeProxyStub();

  redfish::v1::Response mock_response =
      ParseTextProtoOrDie(R"pb(message {
                                 fields {
                                   key: "f"
                                   value { number_value: 1.5 }
                                 }
                               }
                               code: 200)pb");
  EXPECT_CALL(mock_stub_,
              Post(_, EqualsProto(R"pb(url: "/a/b/c"
                                       message {
                                         fields {
                                           key: "d"
                                           value { string_value: "e" }
                                         }
                                       })pb"),
                   _))
      .WillOnce(
          DoAll(SetArgPointee<2>(mock_response), Return(grpc::Status::OK)));

  grpc::ClientContext context;
  redfish::v1::Request request;
  request.set_url("/a/b/c");
  (*request.mutable_message()->mutable_fields())["d"].set_string_value("e");
  redfish::v1::Response response;
  EXPECT_THAT(AsAbslStatus(stub->Post(&context, request, &response)), IsOk());
  EXPECT_THAT(response, EqualsProto(mock_response));
}

TEST_F(GrpcProxyTest, PatchTest) {
  StartProxy();
  auto stub = MakeProxyStub();

  redfish::v1::Response mock_response =
      ParseTextProtoOrDie(R"pb(message {
                                 fields {
                                   key: "f"
                                   value { number_value: 1.5 }
                                 }
                               }
                               code: 200)pb");
  EXPECT_CALL(mock_stub_,
              Patch(_, EqualsProto(R"pb(url: "/a/b/c"
                                        message {
                                          fields {
                                            key: "d"
                                            value { string_value: "e" }
                                          }
                                        })pb"),
                    _))
      .WillOnce(
          DoAll(SetArgPointee<2>(mock_response), Return(grpc::Status::OK)));

  grpc::ClientContext context;
  redfish::v1::Request request;
  request.set_url("/a/b/c");
  (*request.mutable_message()->mutable_fields())["d"].set_string_value("e");
  redfish::v1::Response response;
  EXPECT_THAT(AsAbslStatus(stub->Patch(&context, request, &response)), IsOk());
  EXPECT_THAT(response, EqualsProto(mock_response));
}

TEST_F(GrpcProxyTest, PutTest) {
  StartProxy();
  auto stub = MakeProxyStub();

  redfish::v1::Response mock_response =
      ParseTextProtoOrDie(R"pb(message {
                                 fields {
                                   key: "f"
                                   value { number_value: 1.5 }
                                 }
                               }
                               code: 200)pb");
  EXPECT_CALL(mock_stub_,
              Put(_, EqualsProto(R"pb(url: "/a/b/c"
                                      message {
                                        fields {
                                          key: "d"
                                          value { string_value: "e" }
                                        }
                                      })pb"),
                  _))
      .WillOnce(
          DoAll(SetArgPointee<2>(mock_response), Return(grpc::Status::OK)));

  grpc::ClientContext context;
  redfish::v1::Request request;
  request.set_url("/a/b/c");
  (*request.mutable_message()->mutable_fields())["d"].set_string_value("e");
  redfish::v1::Response response;
  EXPECT_THAT(AsAbslStatus(stub->Put(&context, request, &response)), IsOk());
  EXPECT_THAT(response, EqualsProto(mock_response));
}

TEST_F(GrpcProxyTest, DeleteTest) {
  StartProxy();
  auto stub = MakeProxyStub();

  redfish::v1::Response mock_response =
      ParseTextProtoOrDie(R"pb(message {
                                 fields {
                                   key: "f"
                                   value { number_value: 1.5 }
                                 }
                               }
                               code: 200)pb");
  EXPECT_CALL(mock_stub_,
              Delete(_, EqualsProto(R"pb(url: "/a/b/c"
                                         message {
                                           fields {
                                             key: "d"
                                             value { string_value: "e" }
                                           }
                                         })pb"),
                     _))
      .WillOnce(
          DoAll(SetArgPointee<2>(mock_response), Return(grpc::Status::OK)));

  grpc::ClientContext context;
  redfish::v1::Request request;
  request.set_url("/a/b/c");
  (*request.mutable_message()->mutable_fields())["d"].set_string_value("e");
  redfish::v1::Response response;
  EXPECT_THAT(AsAbslStatus(stub->Delete(&context, request, &response)), IsOk());
  EXPECT_THAT(response, EqualsProto(mock_response));
}

}  // namespace
}  // namespace ecclesia
