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
#include "ecclesia/lib/http/codes.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.pb.h"
#include "ecclesia/lib/redfish/transport/grpc_dynamic_fake_server.h"
#include "ecclesia/lib/redfish/transport/grpc_tls_options.h"
#include "ecclesia/lib/testing/proto.h"
#include "ecclesia/lib/testing/status.h"
#include "grpcpp/server_context.h"

namespace ecclesia {
namespace {

using ::google::protobuf::Struct;
using ::google::protobuf::Value;
using ::redfish::v1::Request;
using ::redfish::v1::Response;
using ::testing::Eq;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Optional;
using ::testing::Test;

class GrpcDynamicImplTest : public Test {
 public:
  GrpcDynamicImplTest() {
    StaticBufferBasedTlsOptions options;
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

TEST_F(GrpcDynamicImplTest, UpdateTransport) {
  EXPECT_DEATH(client_->UpdateTransport(
                   nullptr, RedfishInterface::TrustedEndpoint::kTrusted),
               "Create a new instance instead");
}

TEST_F(GrpcDynamicImplTest, StatusNotOk) {
  server_.SetCallback([](grpc::ServerContext*, const Request* request,
                         Response*) { return grpc::Status::CANCELLED; });
  {
    SCOPED_TRACE("GetRoot");
    RedfishVariant variant = client_->GetRoot();
    EXPECT_THAT(variant.AsObject(), IsNull());
    EXPECT_THAT(variant.AsIterable(), IsNull());
    EXPECT_THAT(variant.status(), IsStatusCancelled());
  }
  {
    SCOPED_TRACE("GetUri");
    RedfishVariant variant = client_->UncachedGetUri("");
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
                                           Response* response) {
    EXPECT_EQ(request->url(), "/redfish/v1");
    *response->mutable_message() = expected_response;
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
                                           Response* response) {
    EXPECT_EQ(request->url(), "/hello");
    *response->mutable_message() = expected_response;
    return grpc::Status::OK;
  });
  std::unique_ptr<RedfishObject> object =
      client_->UncachedGetUri("/hello").AsObject();
  ASSERT_THAT(object, NotNull());
  EXPECT_THAT(object->GetNodeValue<std::string>("Hello"),
              Optional(Eq("World")));
}

TEST_F(GrpcDynamicImplTest, PostUriAndPatchUriOk) {
  constexpr char kHi[] = "hi";
  constexpr char kBye[] = "bye";
  absl::flat_hash_map<std::string, RedfishInterface::ValueVariant> payloads = {
      {"int", 1},
      {"string", "hello"},
      {"char", kHi},
      {"bool", true},
      {"double", 3.14},
      {"list",
       RedfishInterface::ListValue{
           .items = {1, "string",
                     RedfishInterface::ListValue{.items = {"nested", "list"}},
                     RedfishInterface::ObjectValue{
                         .items = {{"nested", "obj"}}}}}},
      {"obj", RedfishInterface::ObjectValue{
                  .items = {{"obj_int", 2},
                            {"obj_string", "goodbye"},
                            {"obj_char", kBye},
                            {"obj_bool", false},
                            {"obj_double", 6.28},
                            {"obj_list",
                             RedfishInterface::ListValue{
                                 .items = {2, "string",
                                           RedfishInterface::ListValue{
                                               .items = {"nested", "list"}},
                                           RedfishInterface::ObjectValue{
                                               .items = {{"nested", "obj"}}}}}},
                            {"obj_obj", RedfishInterface::ObjectValue{
                                            .items = {{"nested", 3}}}}}}}};
  server_.SetCallback([&payloads](grpc::ServerContext*, const Request* request,
                                  Response* response) {
    EXPECT_EQ(request->url(), "/magic");
    EXPECT_THAT(request->message(), EqualsProto(R"pb(
                  fields {
                    key: "int"
                    value { number_value: 1 }
                  }
                  fields {
                    key: "string"
                    value { string_value: "hello" }
                  }
                  fields {
                    key: "char"
                    value { string_value: "hi" }
                  }
                  fields {
                    key: "bool"
                    value { bool_value: true }
                  }
                  fields {
                    key: "double"
                    value { number_value: 3.14 }
                  }
                  fields {
                    key: "list"
                    value {
                      list_value {
                        values { number_value: 1 }
                        values { string_value: "string" }
                        values {
                          list_value {
                            values { string_value: "nested" }
                            values { string_value: "list" }
                          }
                        }
                        values {
                          struct_value {
                            fields {
                              key: "nested"
                              value { string_value: "obj" }
                            }
                          }
                        }
                      }
                    }
                  }
                  fields {
                    key: "obj"
                    value {
                      struct_value {
                        fields {
                          key: "obj_int"
                          value { number_value: 2 }
                        }
                        fields {
                          key: "obj_string"
                          value { string_value: "goodbye" }
                        }
                        fields {
                          key: "obj_char"
                          value { string_value: "bye" }
                        }
                        fields {
                          key: "obj_bool"
                          value { bool_value: false }
                        }
                        fields {
                          key: "obj_double"
                          value { number_value: 6.28 }
                        }
                        fields {
                          key: "obj_list"
                          value {
                            list_value {
                              values { number_value: 2 }
                              values { string_value: "string" }
                              values {
                                list_value {
                                  values { string_value: "nested" }
                                  values { string_value: "list" }
                                }
                              }
                              values {
                                struct_value {
                                  fields {
                                    key: "nested"
                                    value { string_value: "obj" }
                                  }
                                }
                              }
                            }
                          }
                        }
                        fields {
                          key: "obj_obj"
                          value {
                            struct_value {
                              fields {
                                key: "nested"
                                value: { number_value: 3 }
                              }
                            }
                          }
                        }
                      }
                    }
                  }
                )pb"));
    return grpc::Status::OK;
  });
  {
    SCOPED_TRACE("PostUri");
    RedfishVariant variant = client_->PostUri(
        "/magic",
        std::vector<std::pair<std::string, RedfishInterface::ValueVariant>>(
            payloads.begin(), payloads.end()));
    EXPECT_TRUE(variant.status().ok()) << variant.status().message();
  }
  {
    SCOPED_TRACE("PatchUri");
    RedfishVariant variant = client_->PatchUri(
        "/magic",
        std::vector<std::pair<std::string, RedfishInterface::ValueVariant>>(
            payloads.begin(), payloads.end()));
    EXPECT_TRUE(variant.status().ok()) << variant.status().message();
  }
}

TEST_F(GrpcDynamicImplTest, HttpResponseCodeOk) {
  constexpr HttpResponseCode expected_code =
      HttpResponseCode::HTTP_CODE_NO_CONTENT;
  server_.SetCallback(
      [](grpc::ServerContext*, const Request*, Response* response) {
        response->set_code(expected_code);
        return grpc::Status::OK;
      });
  RedfishVariant variant = client_->UncachedGetUri("/magic");
  EXPECT_EQ(variant.httpcode(), expected_code);
  EXPECT_TRUE(variant.status().ok());
}

TEST_F(GrpcDynamicImplTest, HttpResponseCodeError) {
  constexpr HttpResponseCode expected_code =
      HttpResponseCode::HTTP_CODE_NOT_FOUND;
  server_.SetCallback(
      [](grpc::ServerContext*, const Request*, Response* response) {
        response->set_code(expected_code);
        return grpc::Status::OK;
      });
  RedfishVariant variant = client_->UncachedGetUri("/magic");
  EXPECT_EQ(variant.httpcode(), expected_code);
  EXPECT_FALSE(variant.status().ok());
}

using GrpcDynamicImplDeathTest = GrpcDynamicImplTest;

TEST_F(GrpcDynamicImplDeathTest, PostUriWithStringView) {
  server_.SetCallback([](grpc::ServerContext*, const Request* request,
                         Response*) { return grpc::Status::CANCELLED; });
  EXPECT_DEATH(client_->PostUri("", ""), "Use the kv_span version instead");
}

TEST_F(GrpcDynamicImplDeathTest, PatchUriWithStringView) {
  server_.SetCallback([](grpc::ServerContext*, const Request* request,
                         Response*) { return grpc::Status::CANCELLED; });
  EXPECT_DEATH(client_->PatchUri("", ""), "Use the kv_span version instead");
}

}  // namespace
}  // namespace ecclesia
