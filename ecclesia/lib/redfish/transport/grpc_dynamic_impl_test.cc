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

#include "ecclesia/lib/redfish/transport/grpc_dynamic_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "google/protobuf/struct.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.pb.h"
#include "ecclesia/lib/redfish/transport/grpc_dynamic_fake_server.h"
#include "ecclesia/lib/redfish/transport/grpc_dynamic_options.h"
#include "ecclesia/lib/testing/status.h"
#include "grpcpp/server_context.h"

namespace ecclesia {
namespace {

using ::google::protobuf::Struct;
using ::google::protobuf::Value;
using ::libredfish::RedfishInterface;
using ::libredfish::RedfishObject;
using ::libredfish::RedfishVariant;
using ::redfish::v1::Request;
using ::testing::Eq;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Optional;
using ::testing::Test;

class GrpcDynamicImplTest : public Test {
 public:
  GrpcDynamicImplTest() {
    GrpcDynamicImplOptions options;
    options.SetToInsecure();
    options.SetTimeout(absl::Seconds(5));
    client_ = absl::make_unique<GrpcDynamicImpl>(
        GrpcDynamicImpl::Target{.fqdn = "[::1]", .port = server_.GetPort()},
        RedfishInterface::TrustedEndpoint::kUntrusted, options);
  }

 protected:
  GrpcDynamicFakeServer server_;
  std::unique_ptr<RedfishInterface> client_;
};

TEST_F(GrpcDynamicImplTest, IsTrusted) { EXPECT_FALSE(client_->IsTrusted()); }

TEST_F(GrpcDynamicImplTest, UpdateEndpoint) {
  GrpcDynamicFakeServer another_server_;
  EXPECT_DEATH(
      client_->UpdateEndpoint(another_server_.GetHostPort(),
                              RedfishInterface::TrustedEndpoint::kTrusted),
      "Create a new instance instead");
}

TEST_F(GrpcDynamicImplTest, StatusNotOk) {
  server_.SetCallback([](grpc::ServerContext*, const Request* request,
                         Struct*) { return grpc::Status::CANCELLED; });
  {
    SCOPED_TRACE("GetRoot");
    RedfishVariant variant = client_->GetRoot();
    EXPECT_THAT(variant.AsObject(), IsNull());
    EXPECT_THAT(variant.AsIterable(), IsNull());
    EXPECT_THAT(variant.status(), IsStatusCancelled());
  }
  {
    SCOPED_TRACE("GetUri");
    RedfishVariant variant = client_->GetUri("");
    EXPECT_THAT(variant.AsObject(), IsNull());
    EXPECT_THAT(variant.AsIterable(), IsNull());
    EXPECT_THAT(variant.status(), IsStatusCancelled());
  }
  {
    SCOPED_TRACE("PostUri with span");
    RedfishVariant variant = client_->PostUri("", {{"", 0}});
    EXPECT_THAT(variant.AsObject(), IsNull());
    EXPECT_THAT(variant.AsIterable(), IsNull());
    EXPECT_THAT(variant.status(), IsStatusCancelled());
  }
  {
    SCOPED_TRACE("PatchUri with span");
    RedfishVariant variant = client_->PatchUri("", {{"", 0}});
    EXPECT_THAT(variant.AsObject(), IsNull());
    EXPECT_THAT(variant.AsIterable(), IsNull());
    EXPECT_THAT(variant.status(), IsStatusCancelled());
  }
}

TEST_F(GrpcDynamicImplTest, GetRootOk) {
  Struct expected_response;
  Value string_val;
  string_val.set_string_value("World");
  expected_response.mutable_fields()->insert({"Hello", string_val});
  server_.SetCallback([&expected_response](grpc::ServerContext*,
                                           const Request* request,
                                           Struct* response) {
    EXPECT_EQ(request->url(), "/redfish/v1");
    *response = expected_response;
    return grpc::Status::OK;
  });
  std::unique_ptr<RedfishObject> object = client_->GetRoot().AsObject();
  ASSERT_THAT(object, NotNull());
  EXPECT_THAT(object->GetNodeValue<std::string>("Hello"),
              Optional(Eq("World")));
}

TEST_F(GrpcDynamicImplTest, GetUriOk) {
  Struct expected_response;
  Value string_val;
  string_val.set_string_value("World");
  expected_response.mutable_fields()->insert({"Hello", string_val});
  server_.SetCallback([&expected_response](grpc::ServerContext*,
                                           const Request* request,
                                           Struct* response) {
    EXPECT_EQ(request->url(), "/hello");
    *response = expected_response;
    return grpc::Status::OK;
  });
  std::unique_ptr<RedfishObject> object = client_->GetUri("/hello").AsObject();
  ASSERT_THAT(object, NotNull());
  EXPECT_THAT(object->GetNodeValue<std::string>("Hello"),
              Optional(Eq("World")));
}

TEST_F(GrpcDynamicImplTest, PostUriAndPatchUriOk) {
  absl::string_view const_char_start = "const_char_star";
  absl::flat_hash_map<std::string, RedfishInterface::ValueVariant> payloads = {
      {"string", std::string("string")},
      {"const_char_star", const_char_start.data()},
      {"bool", true},
      {"int", 123},
      {"double", 1.0}};
  server_.SetCallback([&payloads](grpc::ServerContext*, const Request* request,
                                  Struct* response) {
    EXPECT_EQ(request->url(), "/magic");
    EXPECT_EQ(request->message().fields().at("string").string_value(),
              std::get<std::string>(payloads.at("string")));
    EXPECT_EQ(request->message().fields().at("const_char_star").string_value(),
              std::get<const char*>(payloads.at("const_char_star")));
    EXPECT_EQ(request->message().fields().at("bool").bool_value(),
              std::get<bool>(payloads.at("bool")));
    EXPECT_EQ(request->message().fields().at("int").number_value(),
              std::get<int>(payloads.at("int")));
    EXPECT_EQ(request->message().fields().at("double").number_value(),
              std::get<double>(payloads.at("double")));
    return grpc::Status::OK;
  });
  {
    SCOPED_TRACE("PostUri");
    libredfish::RedfishVariant variant = client_->PostUri(
        "/magic",
        std::vector<std::pair<std::string, RedfishInterface::ValueVariant>>(
            payloads.begin(), payloads.end()));
    EXPECT_TRUE(variant.status().ok()) << variant.status().message();
  }
  {
    SCOPED_TRACE("PatchUri");
    libredfish::RedfishVariant variant = client_->PatchUri(
        "/magic",
        std::vector<std::pair<std::string, RedfishInterface::ValueVariant>>(
            payloads.begin(), payloads.end()));
    EXPECT_TRUE(variant.status().ok()) << variant.status().message();
  }
}

using GrpcDynamicImplDeathTest = GrpcDynamicImplTest;

TEST_F(GrpcDynamicImplDeathTest, PostUriWithStringView) {
  server_.SetCallback([](grpc::ServerContext*, const Request* request,
                         Struct*) { return grpc::Status::CANCELLED; });
  EXPECT_DEATH(client_->PostUri("", ""), "Use the kv_span version instead");
}

TEST_F(GrpcDynamicImplDeathTest, PatchUriWithStringView) {
  server_.SetCallback([](grpc::ServerContext*, const Request* request,
                         Struct*) { return grpc::Status::CANCELLED; });
  EXPECT_DEATH(client_->PatchUri("", ""), "Use the kv_span version instead");
}

}  // namespace
}  // namespace ecclesia
