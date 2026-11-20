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

#include "ecclesia/lib/redfish/transport/http.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <variant>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/http/client.h"
#include "ecclesia/lib/http/cred.pb.h"
#include "ecclesia/lib/http/curl_client.h"
#include "ecclesia/lib/redfish/testing/fake_redfish_server.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/testing/status.h"
#include "single_include/nlohmann/json.hpp"
#include "tensorflow_serving/util/net_http/public/response_code_enum.h"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"

namespace ecclesia {
namespace {

using ::testing::Contains;
using ::testing::Eq;
using ::testing::Gt;
using ::testing::Pair;

class HttpRedfishTransportTest : public ::testing::Test {
 protected:
  void SetUp() override {
    server_ = std::make_unique<FakeRedfishServer>(
        "barebones_session_auth/mockup.shar");
    auto config = server_->GetConfig();
    HttpCredential creds;
    auto curl_http_client =
        std::make_unique<CurlHttpClient>(LibCurlProxy::CreateInstance(), creds);
    network_endpoint_ = absl::StrFormat("%s:%d", config.hostname, config.port);
    transport_ = HttpRedfishTransport::MakeNetwork(std::move(curl_http_client),
                                                   network_endpoint_);
  }

  std::unique_ptr<FakeRedfishServer> server_;
  std::string network_endpoint_;
  std::unique_ptr<HttpRedfishTransport> transport_;
};

TEST_F(HttpRedfishTransportTest, CanGet) {
  auto result = transport_->Get("/redfish/v1");
  ASSERT_TRUE(result.ok()) << result.status().message();
  EXPECT_THAT(result->code, Eq(200));
  EXPECT_THAT(result->headers, Contains(Pair("OData-Version", "4.0")));
  nlohmann::json expected =
      nlohmann::json::parse(R"json({
  "@odata.context": "/redfish/v1/$metadata#ServiceRoot.ServiceRoot",
  "@odata.id": "/redfish/v1",
  "@odata.type": "#ServiceRoot.v1_5_0.ServiceRoot",
  "Chassis": {
      "@odata.id": "/redfish/v1/Chassis"
  },
  "EventService": {
      "@odata.id": "/redfish/v1/EventService"
  },
  "Id": "RootService",
  "Links": {
      "Sessions": {
          "@odata.id": "/redfish/v1/SessionService/Sessions"
      }
  },
  "Name": "Root Service",
  "RedfishVersion": "1.6.1"
})json",
                            nullptr, /*allow_exceptions=*/false);
  ASSERT_TRUE(std::holds_alternative<nlohmann::json>(result->body));
  const auto& json = std::get<nlohmann::json>(result->body);
  EXPECT_THAT(json, Eq(expected))
      << "Diff: " << nlohmann::json::diff(json, expected);
}

TEST_F(HttpRedfishTransportTest, GetInvalidJson) {
  bool called = false;
  server_->AddHttpGetHandler(
      "/redfish/v1/Chassis/1/Actions/Chassis.Reset",
      [&](::tensorflow::serving::net_http::ServerRequestInterface* req) {
        called = true;
        // Construct the success message.
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        // Return some invalid JSON.
        req->WriteResponseString("{");
        req->Reply();
      });

  auto result = transport_->Get("/redfish/v1/Chassis/1/Actions/Chassis.Reset");
  ASSERT_TRUE(called);
  EXPECT_TRUE(result.ok()) << result.status().message();
  EXPECT_THAT(result->code, Eq(200));
  EXPECT_THAT(result->headers, Contains(Pair("OData-Version", "4.0")));
  ASSERT_TRUE(std::holds_alternative<nlohmann::json>(result->body));
  EXPECT_TRUE(std::get<nlohmann::json>(result->body).is_discarded());
}

TEST_F(HttpRedfishTransportTest, GetError) {
  auto result_json = nlohmann::json::parse(R"json({
  "error": {
    "code": "Base.1.0.GeneralError",
    "message": "A general error has occurred.  See Resolution for information on how to resolve the error, or @Message.ExtendedInfo if Resolution is not provided."
  }
})json");

  bool called = false;
  server_->AddHttpGetHandler(
      "/redfish/v1/Chassis/1/Actions/Chassis.Reset",
      [&](::tensorflow::serving::net_http::ServerRequestInterface* req) {
        called = true;
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        req->WriteResponseString(result_json.dump());
        req->ReplyWithStatus(
            ::tensorflow::serving::net_http::HTTPStatusCode::ERROR);
      });

  auto result = transport_->Get("/redfish/v1/Chassis/1/Actions/Chassis.Reset");
  ASSERT_TRUE(called);
  EXPECT_TRUE(result.ok()) << result.status().message();
  EXPECT_THAT(result->code, Eq(500));
  EXPECT_THAT(result->headers, Contains(Pair("OData-Version", "4.0")));
  ASSERT_TRUE(std::holds_alternative<nlohmann::json>(result->body));
  EXPECT_THAT(std::get<nlohmann::json>(result->body), Eq(result_json));
}

TEST_F(HttpRedfishTransportTest, CanPost) {
  auto request_json = nlohmann::json::parse(R"json({
  "ResetType": "PowerCycle"
})json");
  auto result_json = nlohmann::json::parse(R"json({
  "error": {
    "code": "Base.1.0.Success",
    "message": "Successfully Completed Request",
    "@Message.ExtendedInfo": [
      {
      "@odata.type": "#Message.v1_0_0.Message",
      "MessageId": "Base.1.0.Success",
      "Message": "Successfully Completed Request",
      "Severity": "OK",
      "Resolution": "None"
      }
    ]
  }
})json");

  bool called = false;
  server_->AddHttpPostHandler(
      "/redfish/v1/Chassis/1/Actions/Chassis.Reset",
      [&](::tensorflow::serving::net_http::ServerRequestInterface* req) {
        int64_t size;
        auto buf = req->ReadRequestBytes(&size);
        ASSERT_THAT(size, Gt(0));
        auto read_request = nlohmann::json::parse(
            absl::string_view(buf.get(), size), /*cb=*/nullptr,
            /*allow_exceptions=*/false);
        ASSERT_FALSE(read_request.is_discarded());
        EXPECT_THAT(read_request, Eq(request_json));
        called = true;
        // Construct the success message.
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        req->WriteResponseString(result_json.dump());
        req->Reply();
      });

  auto result = transport_->Post("/redfish/v1/Chassis/1/Actions/Chassis.Reset",
                                 request_json.dump());
  ASSERT_TRUE(called);
  ASSERT_TRUE(result.ok()) << result.status().message();
  EXPECT_THAT(result->code, Eq(200));
  EXPECT_THAT(result->headers, Contains(Pair("OData-Version", "4.0")));
  ASSERT_TRUE(std::holds_alternative<nlohmann::json>(result->body));
  const auto& json = std::get<nlohmann::json>(result->body);
  EXPECT_THAT(json, Eq(result_json))
      << "Diff: " << nlohmann::json::diff(json, result_json);
}

TEST_F(HttpRedfishTransportTest, PostInvalidJson) {
  auto request_json = nlohmann::json::parse(R"json({
  "ResetType": "PowerCycle"
})json");
  bool called = false;
  server_->AddHttpPostHandler(
      "/redfish/v1/Chassis/1/Actions/Chassis.Reset",
      [&](::tensorflow::serving::net_http::ServerRequestInterface* req) {
        called = true;
        // Construct the success message.
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        // Return some invalid JSON.
        req->WriteResponseString("{");
        req->Reply();
      });

  auto result = transport_->Post("/redfish/v1/Chassis/1/Actions/Chassis.Reset",
                                 request_json.dump());
  ASSERT_TRUE(called);
  EXPECT_TRUE(result.ok()) << result.status().message();
  EXPECT_THAT(result->code, Eq(200));
  EXPECT_THAT(result->headers, Contains(Pair("OData-Version", "4.0")));
  ASSERT_TRUE(std::holds_alternative<nlohmann::json>(result->body));
  EXPECT_TRUE(std::get<nlohmann::json>(result->body).is_discarded());
}

TEST_F(HttpRedfishTransportTest, PostError) {
  auto request_json = nlohmann::json::parse(R"json({
  "ResetType": "PowerCycle"
})json");
  auto result_json = nlohmann::json::parse(R"json({
  "error": {
    "code": "Base.1.0.GeneralError",
    "message": "A general error has occurred.  See Resolution for information on how to resolve the error, or @Message.ExtendedInfo if Resolution is not provided."
  }
})json");

  bool called = false;
  server_->AddHttpPostHandler(
      "/redfish/v1/Chassis/1/Actions/Chassis.Reset",
      [&](::tensorflow::serving::net_http::ServerRequestInterface* req) {
        called = true;
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        req->WriteResponseString(result_json.dump());
        req->ReplyWithStatus(
            ::tensorflow::serving::net_http::HTTPStatusCode::ERROR);
      });

  auto result = transport_->Post("/redfish/v1/Chassis/1/Actions/Chassis.Reset",
                                 request_json.dump());
  ASSERT_TRUE(called);
  EXPECT_TRUE(result.ok()) << result.status().message();
  EXPECT_THAT(result->code, Eq(500));
  EXPECT_THAT(result->headers, Contains(Pair("OData-Version", "4.0")));
  ASSERT_TRUE(std::holds_alternative<nlohmann::json>(result->body));
  EXPECT_THAT(std::get<nlohmann::json>(result->body), Eq(result_json));
}

TEST_F(HttpRedfishTransportTest, CanPatch) {
  auto request_json = nlohmann::json::parse(R"json({
  "Name": "TestName"
})json");
  auto result_json = nlohmann::json::parse(R"json({
    "@odata.context": "/redfish/v1/$metadata#Chassis.Chassis",
    "@odata.id": "/redfish/v1/Chassis/1",
    "@odata.type": "#Chassis.v1_10_0.Chassis",
    "Id": "chassis",
    "Name": "TestName",
    "Status": {
        "State": "StandbyOffline"
    }
})json");

  bool called = false;
  server_->AddHttpPatchHandler(
      "/redfish/v1/Chassis/1",
      [&](::tensorflow::serving::net_http::ServerRequestInterface* req) {
        int64_t size;
        auto buf = req->ReadRequestBytes(&size);
        ASSERT_THAT(size, Gt(0));
        auto read_request = nlohmann::json::parse(
            absl::string_view(buf.get(), size), /*cb=*/nullptr,
            /*allow_exceptions=*/false);
        ASSERT_FALSE(read_request.is_discarded());
        EXPECT_THAT(read_request, Eq(request_json));
        called = true;
        // Construct the success message.
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        req->WriteResponseString(result_json.dump());
        req->Reply();
      });

  auto result = transport_->Patch("/redfish/v1/Chassis/1", request_json.dump());
  ASSERT_TRUE(called);
  ASSERT_TRUE(result.ok()) << result.status().message();
  EXPECT_THAT(result->code, Eq(200));
  EXPECT_THAT(result->headers, Contains(Pair("OData-Version", "4.0")));
  ASSERT_TRUE(std::holds_alternative<nlohmann::json>(result->body));
  const auto& json = std::get<nlohmann::json>(result->body);
  EXPECT_THAT(json, Eq(result_json))
      << "Diff: " << nlohmann::json::diff(json, result_json);
}

TEST_F(HttpRedfishTransportTest, PatchInvalidJson) {
  auto request_json = nlohmann::json::parse(R"json({
  "Name": "TestName"
})json");
  bool called = false;
  server_->AddHttpPatchHandler(
      "/redfish/v1/Chassis/1",
      [&](::tensorflow::serving::net_http::ServerRequestInterface* req) {
        called = true;
        // Construct the success message.
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        // Return some invalid JSON.
        req->WriteResponseString("{");
        req->Reply();
      });

  auto result = transport_->Patch("/redfish/v1/Chassis/1", request_json.dump());
  ASSERT_TRUE(called);
  EXPECT_TRUE(result.ok()) << result.status().message();
  EXPECT_THAT(result->code, Eq(200));
  EXPECT_THAT(result->headers, Contains(Pair("OData-Version", "4.0")));
  ASSERT_TRUE(std::holds_alternative<nlohmann::json>(result->body));
  EXPECT_TRUE(std::get<nlohmann::json>(result->body).is_discarded());
}

TEST_F(HttpRedfishTransportTest, PatchError) {
  auto request_json = nlohmann::json::parse(R"json({
  "Name": "TestName"
})json");
  auto result_json = nlohmann::json::parse(R"json({
  "error": {
    "code": "Base.1.0.GeneralError",
    "message": "A general error has occurred.  See Resolution for information on how to resolve the error, or @Message.ExtendedInfo if Resolution is not provided."
  }
})json");

  bool called = false;
  server_->AddHttpPatchHandler(
      "/redfish/v1/Chassis/1",
      [&](::tensorflow::serving::net_http::ServerRequestInterface* req) {
        called = true;
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        req->WriteResponseString(result_json.dump());
        req->ReplyWithStatus(
            ::tensorflow::serving::net_http::HTTPStatusCode::ERROR);
      });

  auto result = transport_->Patch("/redfish/v1/Chassis/1", request_json.dump());
  ASSERT_TRUE(called);
  EXPECT_TRUE(result.ok()) << result.status().message();
  EXPECT_THAT(result->code, Eq(500));
  EXPECT_THAT(result->headers, Contains(Pair("OData-Version", "4.0")));
  ASSERT_TRUE(std::holds_alternative<nlohmann::json>(result->body));
  EXPECT_THAT(std::get<nlohmann::json>(result->body), Eq(result_json));
}

TEST_F(HttpRedfishTransportTest, CanDelete) {
  auto result_json = nlohmann::json::parse(R"json({
    "@odata.context": "/redfish/v1/$metadata#Chassis.Chassis",
    "@odata.id": "/redfish/v1/Chassis/1",
    "@odata.type": "#Chassis.v1_10_0.Chassis",
    "Id": "chassis",
    "Name": "Name",
    "Status": {
        "State": "StandbyOffline"
    }
})json");

  bool called = false;
  server_->AddHttpDeleteHandler(
      "/redfish/v1/Chassis/1",
      [&](::tensorflow::serving::net_http::ServerRequestInterface* req) {
        called = true;
        // Construct the success message.
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        req->WriteResponseString(result_json.dump());
        req->Reply();
      });

  auto result = transport_->Delete("/redfish/v1/Chassis/1", "");
  ASSERT_TRUE(called);
  ASSERT_TRUE(result.ok()) << result.status().message();
  EXPECT_THAT(result->code, Eq(200));
  EXPECT_THAT(result->headers, Contains(Pair("OData-Version", "4.0")));
  ASSERT_TRUE(std::holds_alternative<nlohmann::json>(result->body));
  const auto& json = std::get<nlohmann::json>(result->body);
  EXPECT_THAT(json, Eq(result_json))
      << "Diff: " << nlohmann::json::diff(json, result_json);
}

TEST_F(HttpRedfishTransportTest, DeleteError) {
  auto result_json = nlohmann::json::parse(R"json({
  "error": {
    "code": "Base.1.0.GeneralError",
    "message": "A general error has occurred.  See Resolution for information on how to resolve the error, or @Message.ExtendedInfo if Resolution is not provided."
  }
})json");

  bool called = false;
  server_->AddHttpDeleteHandler(
      "/redfish/v1/Chassis/1",
      [&](::tensorflow::serving::net_http::ServerRequestInterface* req) {
        called = true;
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        req->WriteResponseString(result_json.dump());
        req->ReplyWithStatus(
            ::tensorflow::serving::net_http::HTTPStatusCode::ERROR);
      });

  auto result = transport_->Delete("/redfish/v1/Chassis/1", "");
  ASSERT_TRUE(called);
  EXPECT_TRUE(result.ok()) << result.status().message();
  EXPECT_THAT(result->code, Eq(500));
  ASSERT_TRUE(std::holds_alternative<nlohmann::json>(result->body));
  EXPECT_THAT(std::get<nlohmann::json>(result->body), Eq(result_json));
}

TEST_F(HttpRedfishTransportTest, CanEstablishSessionRootSessionCollection) {
  auto request_json = nlohmann::json::parse(R"json({
  "UserName": "test_username",
  "Password": "test_password"
})json");
  auto result_json = nlohmann::json::parse(R"json({
  "@odata.id": "/redfish/v1/Session/Sessions/1",
  "@odata.type": "#Session.v1_0_0.Session",
  "Id": "1",
  "Name": "User Session",
  "Description": "User Session",
  "UserName": "test_username",
  "Password": null
})json");
  auto root_json = nlohmann::json::parse(R"json({
  "@odata.context": "/redfish/v1/$metadata#ServiceRoot.ServiceRoot",
  "@odata.id": "/redfish/v1",
  "@odata.type": "#ServiceRoot.v1_5_0.ServiceRoot",
  "SessionService": {
    "@odata.id": "/redfish/v1/SessionService"
  }
})json");

  auto get_handler =
      [&](::tensorflow::serving::net_http::ServerRequestInterface* req) {
        // Construct the success message.
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        req->WriteResponseString(root_json.dump());
        req->Reply();
      };

  server_->AddHttpGetHandler("/redfish/v1", get_handler);
  server_->AddHttpGetHandler("/redfish/v1/", get_handler);

  bool called = false;
  server_->AddHttpPostHandler(
      "/redfish/v1/SessionService/Sessions",
      [&](::tensorflow::serving::net_http::ServerRequestInterface* req) {
        int64_t size;
        auto buf = req->ReadRequestBytes(&size);
        ASSERT_THAT(size, Gt(0));
        auto read_request = nlohmann::json::parse(
            absl::string_view(buf.get(), size), /*cb=*/nullptr,
            /*allow_exceptions=*/false);
        ASSERT_FALSE(read_request.is_discarded());
        EXPECT_THAT(read_request, Eq(request_json));
        called = true;
        // Construct the success message.
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->WriteResponseString(result_json.dump());
        req->AppendResponseHeader("X-Auth-Token", "a1b2c3");
        req->AppendResponseHeader("Location", "/redfish/v1/Session/Sessions/1");
        req->Reply();
      });

  auto result = transport_->DoSessionAuth("test_username", "test_password");
  EXPECT_TRUE(called);
  EXPECT_TRUE(result.ok()) << result.message();

  // Send a GET and check that we are sending the session token in the header.
  bool req_get_called = false;
  server_->AddHttpGetHandler(
      "/redfish/v1/test/uri",
      [&](::tensorflow::serving::net_http::ServerRequestInterface* req) {
        req_get_called = true;
        EXPECT_THAT(req->GetRequestHeader("X-Auth-Token"), Eq("a1b2c3"));
        // Construct the success message.
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->WriteResponseString("{}");
        req->Reply();
      });
  auto get_result = transport_->Get("/redfish/v1/test/uri");
  EXPECT_TRUE(req_get_called);
  EXPECT_TRUE(get_result.ok()) << get_result.status().message();

  // Handle the delete when the transport is destroyed.
  bool delete_called = false;
  server_->AddHttpDeleteHandler(
      "/redfish/v1/Session/Sessions/1",
      [&](::tensorflow::serving::net_http::ServerRequestInterface* req) {
        delete_called = true;
        // Construct the success message.
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->ReplyWithStatus(
            tensorflow::serving::net_http::HTTPStatusCode::NO_CONTENT);
      });
  transport_.reset();
  EXPECT_TRUE(delete_called);
}

TEST_F(HttpRedfishTransportTest, CanEstablishSessionServiceRootLinks) {
  auto request_json = nlohmann::json::parse(R"json({
  "UserName": "test_username",
  "Password": "test_password"
})json");
  auto result_json = nlohmann::json::parse(R"json({
  "@odata.id": "/redfish/v1/Session/Sessions/1",
  "@odata.type": "#Session.v1_0_0.Session",
  "Id": "1",
  "Name": "User Session",
  "Description": "User Session",
  "UserName": "test_username",
  "Password": null
})json");
  auto root_json = nlohmann::json::parse(R"json({
  "@odata.context": "/redfish/v1/$metadata#ServiceRoot.ServiceRoot",
  "@odata.id": "/redfish/v1",
  "@odata.type": "#ServiceRoot.v1_5_0.ServiceRoot",
  "Links": {
      "Sessions": {
          "@odata.id": "/redfish/v1/SessionService/Sessions"
      }
  }
})json");

  bool root_called = false;
  auto get_handler =
      [&](::tensorflow::serving::net_http::ServerRequestInterface* req) {
        root_called = true;
        // Construct the success message.
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        req->WriteResponseString(root_json.dump());
        req->Reply();
      };

  server_->AddHttpGetHandler("/redfish/v1", get_handler);
  server_->AddHttpGetHandler("/redfish/v1/", get_handler);

  bool called = false;
  server_->AddHttpPostHandler(
      "/redfish/v1/SessionService/Sessions",
      [&](::tensorflow::serving::net_http::ServerRequestInterface* req) {
        int64_t size;
        auto buf = req->ReadRequestBytes(&size);
        ASSERT_THAT(size, Gt(0));
        auto read_request = nlohmann::json::parse(
            absl::string_view(buf.get(), size), /*cb=*/nullptr,
            /*allow_exceptions=*/false);
        ASSERT_FALSE(read_request.is_discarded());
        EXPECT_THAT(read_request, Eq(request_json));
        called = true;
        // Construct the success message.
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->WriteResponseString(result_json.dump());
        req->AppendResponseHeader("X-Auth-Token", "a1b2c3");
        req->AppendResponseHeader("Location", "/redfish/v1/Session/Sessions/1");
        req->Reply();
      });

  auto result = transport_->DoSessionAuth("test_username", "test_password");
  EXPECT_TRUE(called);
  EXPECT_TRUE(result.ok()) << result.message();

  // Send a GET and check that we are sending the session token in the header.
  bool req_get_called = false;
  server_->AddHttpGetHandler(
      "/redfish/v1/test/uri",
      [&](::tensorflow::serving::net_http::ServerRequestInterface* req) {
        req_get_called = true;
        EXPECT_THAT(req->GetRequestHeader("X-Auth-Token"), Eq("a1b2c3"));
        // Construct the success message.
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        req->WriteResponseString("{}");
        req->Reply();
      });
  auto get_result = transport_->Get("/redfish/v1/test/uri");
  EXPECT_TRUE(req_get_called);
  EXPECT_TRUE(get_result.ok()) << get_result.status().message();

  // Handle the delete when the transport is destroyed.
  bool delete_called = false;
  server_->AddHttpDeleteHandler(
      "/redfish/v1/Session/Sessions/1",
      [&](::tensorflow::serving::net_http::ServerRequestInterface* req) {
        delete_called = true;
        // Construct the success message.
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->ReplyWithStatus(
            tensorflow::serving::net_http::HTTPStatusCode::NO_CONTENT);
      });
  transport_.reset();
  EXPECT_TRUE(delete_called);
}

TEST_F(HttpRedfishTransportTest, PostOperationsAfterAuthIncludeHeader) {
  auto result_json = nlohmann::json::parse(R"json({
  "@odata.id": "/redfish/v1/Session/Sessions/1",
  "@odata.type": "#Session.v1_0_0.Session",
  "Id": "1",
  "Name": "User Session",
  "Description": "User Session",
  "UserName": "test_username",
  "Password": null
})json");

  bool called = false;
  server_->AddHttpPostHandler(
      "/redfish/v1/SessionService/Sessions",
      [&](::tensorflow::serving::net_http::ServerRequestInterface* req) {
        int64_t size;
        auto buf = req->ReadRequestBytes(&size);
        ASSERT_THAT(size, Gt(0));
        auto read_request = nlohmann::json::parse(
            absl::string_view(buf.get(), size), /*cb=*/nullptr,
            /*allow_exceptions=*/false);
        ASSERT_FALSE(read_request.is_discarded());
        called = true;
        // Construct the success message.
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->WriteResponseString(result_json.dump());
        req->AppendResponseHeader("X-Auth-Token", "a1b2c3");
        req->AppendResponseHeader("Location", "/redfish/v1/Session/Sessions/1");
        req->Reply();
      });

  auto result = transport_->DoSessionAuth("test_username", "test_password");
  EXPECT_TRUE(called);
  EXPECT_TRUE(result.ok()) << result.message();

  // Send a POST and check that we are sending the session token in the header.
  bool req_get_called = false;
  server_->AddHttpPostHandler(
      "/redfish/v1/test/uri",
      [&](::tensorflow::serving::net_http::ServerRequestInterface* req) {
        req_get_called = true;
        EXPECT_THAT(req->GetRequestHeader("X-Auth-Token"), Eq("a1b2c3"));
        // Construct the success message.
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->WriteResponseString("{}");
        req->Reply();
      });
  auto get_result = transport_->Post("/redfish/v1/test/uri", "");
  EXPECT_TRUE(req_get_called);
  EXPECT_TRUE(get_result.ok()) << get_result.status().message();

  // Handle the delete when the transport is destroyed.
  bool delete_called = false;
  server_->AddHttpDeleteHandler(
      "/redfish/v1/Session/Sessions/1",
      [&](::tensorflow::serving::net_http::ServerRequestInterface* req) {
        delete_called = true;
        // Construct the success message.
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->ReplyWithStatus(
            tensorflow::serving::net_http::HTTPStatusCode::NO_CONTENT);
      });
  transport_.reset();
  EXPECT_TRUE(delete_called);
}

TEST_F(HttpRedfishTransportTest, FailEstablishSessionNoSessionSupported) {
  auto root_json = nlohmann::json::parse(R"json({
  "@odata.context": "/redfish/v1/$metadata#ServiceRoot.ServiceRoot",
  "@odata.id": "/redfish/v1",
  "@odata.type": "#ServiceRoot.v1_5_0.ServiceRoot"
})json");

  bool root_called = false;
  auto get_handler =
      [&](::tensorflow::serving::net_http::ServerRequestInterface* req) {
        root_called = true;
        // Construct the success message.
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        req->WriteResponseString(root_json.dump());
        req->Reply();
      };

  server_->AddHttpGetHandler("/redfish/v1", get_handler);
  server_->AddHttpGetHandler("/redfish/v1/", get_handler);

  auto result = transport_->DoSessionAuth("test_username", "test_password");
  EXPECT_THAT(result, IsStatusNotFound()) << result.message();
}

TEST_F(HttpRedfishTransportTest, FailEstablishSessionNoAuthToken) {
  auto result_json = nlohmann::json::parse(R"json({
  "@odata.id": "/redfish/v1/Session/Sessions/1",
  "@odata.type": "#Session.v1_0_0.Session",
  "Id": "1",
  "Name": "User Session",
  "Description": "User Session",
  "UserName": "test_username",
  "Password": null
})json");

  bool called = false;
  server_->AddHttpPostHandler(
      "/redfish/v1/SessionService/Sessions",
      [&](::tensorflow::serving::net_http::ServerRequestInterface* req) {
        int64_t size;
        auto buf = req->ReadRequestBytes(&size);
        ASSERT_THAT(size, Gt(0));
        auto read_request = nlohmann::json::parse(
            absl::string_view(buf.get(), size), /*cb=*/nullptr,
            /*allow_exceptions=*/false);
        ASSERT_FALSE(read_request.is_discarded());
        called = true;
        // Construct the success message.
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->WriteResponseString(result_json.dump());
        req->AppendResponseHeader("Location", "/redfish/v1/Session/Sessions/1");
        req->Reply();
      });

  auto result = transport_->DoSessionAuth("test_username", "test_password");
  EXPECT_TRUE(called);
  EXPECT_THAT(result, IsStatusInternal()) << result.message();
}

TEST_F(HttpRedfishTransportTest, FailEstablishSessionNoLocation) {
  auto result_json = nlohmann::json::parse(R"json({
  "@odata.id": "/redfish/v1/Session/Sessions/1",
  "@odata.type": "#Session.v1_0_0.Session",
  "Id": "1",
  "Name": "User Session",
  "Description": "User Session",
  "UserName": "test_username",
  "Password": null
})json");

  bool called = false;
  server_->AddHttpPostHandler(
      "/redfish/v1/SessionService/Sessions",
      [&](::tensorflow::serving::net_http::ServerRequestInterface* req) {
        int64_t size;
        auto buf = req->ReadRequestBytes(&size);
        ASSERT_THAT(size, Gt(0));
        auto read_request = nlohmann::json::parse(
            absl::string_view(buf.get(), size), /*cb=*/nullptr,
            /*allow_exceptions=*/false);
        ASSERT_FALSE(read_request.is_discarded());
        called = true;
        // Construct the success message.
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->WriteResponseString(result_json.dump());
        req->AppendResponseHeader("X-Auth-Token", "a1b2c3");
        req->Reply();
      });

  auto result = transport_->DoSessionAuth("test_username", "test_password");
  EXPECT_TRUE(called);
  EXPECT_THAT(result, IsStatusInternal()) << result.message();
}

TEST_F(HttpRedfishTransportTest, FailEstablishSessionHttpError) {
  auto result_json = nlohmann::json::parse(R"json({
  "@odata.id": "/redfish/v1/Session/Sessions/1",
  "@odata.type": "#Session.v1_0_0.Session",
  "Id": "1",
  "Name": "User Session",
  "Description": "User Session",
  "UserName": "test_username",
  "Password": null
})json");

  bool called = false;
  server_->AddHttpPostHandler(
      "/redfish/v1/SessionService/Sessions",
      [&](::tensorflow::serving::net_http::ServerRequestInterface* req) {
        int64_t size;
        auto buf = req->ReadRequestBytes(&size);
        ASSERT_THAT(size, Gt(0));
        auto read_request = nlohmann::json::parse(
            absl::string_view(buf.get(), size), /*cb=*/nullptr,
            /*allow_exceptions=*/false);
        ASSERT_FALSE(read_request.is_discarded());
        called = true;
        // Construct the success message.
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->ReplyWithStatus(
            tensorflow::serving::net_http::HTTPStatusCode::NOT_FOUND);
      });

  auto result = transport_->DoSessionAuth("test_username", "test_password");
  EXPECT_TRUE(called);
  EXPECT_THAT(result, IsStatusInternal()) << result.message();
}

TEST_F(HttpRedfishTransportTest, CanDoSessionAuthTwice) {
  auto request_json = nlohmann::json::parse(R"json({
  "UserName": "test_username",
  "Password": "test_password"
})json");
  auto request_json2 = nlohmann::json::parse(R"json({
  "UserName": "test_username2",
  "Password": "test_password2"
})json");
  auto result_json = nlohmann::json::parse(R"json({
  "@odata.id": "/redfish/v1/Session/Sessions/1",
  "@odata.type": "#Session.v1_0_0.Session",
  "Id": "1",
  "Name": "User Session",
  "Description": "User Session",
  "UserName": "test_username",
  "Password": null
})json");

  bool called = false;
  server_->AddHttpPostHandler(
      "/redfish/v1/SessionService/Sessions",
      [&](::tensorflow::serving::net_http::ServerRequestInterface* req) {
        int64_t size;
        auto buf = req->ReadRequestBytes(&size);
        ASSERT_THAT(size, Gt(0));
        auto read_request = nlohmann::json::parse(
            absl::string_view(buf.get(), size), /*cb=*/nullptr,
            /*allow_exceptions=*/false);
        ASSERT_FALSE(read_request.is_discarded());
        EXPECT_THAT(read_request, Eq(request_json));
        called = true;
        // Construct the success message.
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->WriteResponseString(result_json.dump());
        req->AppendResponseHeader("X-Auth-Token", "a1b2c3");
        req->AppendResponseHeader("Location", "/redfish/v1/Session/Sessions/1");
        req->Reply();
      });

  auto result = transport_->DoSessionAuth("test_username", "test_password");
  EXPECT_TRUE(called);
  EXPECT_TRUE(result.ok()) << result.message();

  // Send a GET and check that we are sending the session token in the header.
  bool req_get_called = false;
  server_->AddHttpGetHandler(
      "/redfish/v1/test/uri",
      [&](::tensorflow::serving::net_http::ServerRequestInterface* req) {
        req_get_called = true;
        EXPECT_THAT(req->GetRequestHeader("X-Auth-Token"), Eq("a1b2c3"));
        // Construct the success message.
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->WriteResponseString("{}");
        req->Reply();
      });
  auto get_result = transport_->Get("/redfish/v1/test/uri");
  EXPECT_TRUE(req_get_called);
  EXPECT_TRUE(get_result.ok()) << get_result.status().message();

  // Handle DELETE and second POST.
  int delete_count = 0;
  server_->AddHttpDeleteHandler(
      "/redfish/v1/Session/Sessions/1",
      [&](::tensorflow::serving::net_http::ServerRequestInterface* req) {
        ++delete_count;
        // Construct the success message.
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->ReplyWithStatus(
            tensorflow::serving::net_http::HTTPStatusCode::NO_CONTENT);
      });
  called = false;
  server_->AddHttpPostHandler(
      "/redfish/v1/SessionService/Sessions",
      [&](::tensorflow::serving::net_http::ServerRequestInterface* req) {
        int64_t size;
        auto buf = req->ReadRequestBytes(&size);
        ASSERT_THAT(size, Gt(0));
        auto read_request = nlohmann::json::parse(
            absl::string_view(buf.get(), size), /*cb=*/nullptr,
            /*allow_exceptions=*/false);
        ASSERT_FALSE(read_request.is_discarded());
        EXPECT_THAT(read_request, Eq(request_json2));
        called = true;
        // Construct the success message.
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->WriteResponseString(result_json.dump());
        req->AppendResponseHeader("X-Auth-Token", "d4e5f6");
        req->AppendResponseHeader("Location", "/redfish/v1/Session/Sessions/1");
        req->Reply();
      });
  auto result2 = transport_->DoSessionAuth("test_username2", "test_password2");
  EXPECT_THAT(delete_count, Eq(1));
  EXPECT_TRUE(result2.ok()) << result2.message();

  // Send a GET and check that we are sending the new session token.
  req_get_called = false;
  server_->AddHttpGetHandler(
      "/redfish/v1/test/uri",
      [&](::tensorflow::serving::net_http::ServerRequestInterface* req) {
        req_get_called = true;
        EXPECT_THAT(req->GetRequestHeader("X-Auth-Token"), Eq("d4e5f6"));
        // Construct the success message.
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->WriteResponseString("{}");
        req->Reply();
      });
  get_result = transport_->Get("/redfish/v1/test/uri");
  EXPECT_TRUE(req_get_called);
  EXPECT_TRUE(get_result.ok()) << get_result.status().message();

  // Clean up transport_ before the test ends so that our DeleteHandler doesn't
  // fall out of scope.
  transport_.reset();
  EXPECT_THAT(delete_count, Eq(2));
}

TEST_F(HttpRedfishTransportTest, GetWithTimeoutUnimplemented) {
  ASSERT_THAT(transport_->Get("test_uri", absl::Seconds(1)),
              IsStatusUnimplemented());
}

class MockHttpClient : public HttpClient {
 public:
  MOCK_METHOD(absl::StatusOr<HttpResponse>, Get, (std::unique_ptr<HttpRequest>),
              (override));
  MOCK_METHOD(absl::StatusOr<HttpResponse>, Post,
              (std::unique_ptr<HttpRequest>), (override));
  MOCK_METHOD(absl::StatusOr<HttpResponse>, Delete,
              (std::unique_ptr<HttpRequest>), (override));
  MOCK_METHOD(absl::StatusOr<HttpResponse>, Patch,
              (std::unique_ptr<HttpRequest>), (override));
  MOCK_METHOD(absl::StatusOr<HttpResponse>, Put, (std::unique_ptr<HttpRequest>),
              (override));
};

constexpr absl::string_view kRejectedRequestTargets[] = {
    "",
    "#fragment",
    "redfish/v1",
    "@127.0.0.1:8080/unexpected",
    "\\@127.0.0.1:8080/unexpected",
    "7/redfish/v1",
    ".other.example/redfish/v1",
    ":8080/redfish/v1",
    "http://127.0.0.1:8080/redfish/v1",
    "https://127.0.0.1:8080/redfish/v1",
    "//",
    "//#fragment",
    "//127.0.0.1:8080/redfish/v1",
    "/\\127.0.0.1:8080/redfish/v1",
    "/redfish/v1/\\sub",
    "/redfish/v1/with space",
    "/redfish/v1/with space#fragment",
    "/redfish/v1/\t",
    "/redfish/v1/\x1f",
    "/redfish/v1\r\nX-Header: true",
    "/redfish/v1\x7f",
    "/.",
    "/..",
    "/redfish/v1/..#fragment",
    "/redfish/v1/../Managers",
    "/redfish/v1/./Systems",
    "/redfish/v1/%2e/Systems",
    "/redfish/v1/%2e%2e/Managers",
    "/redfish/v1/%2E./Managers",
    "/redfish/v1/.%2E/Managers",
    "/redfish/v1/..?a=1",
};

void ExpectRejectedOnAllRestMethods(RedfishTransport& transport,
                                    absl::string_view path) {
  EXPECT_THAT(transport.Get(path), IsStatusInvalidArgument()) << path;
  EXPECT_THAT(transport.Post(path, "{}"), IsStatusInvalidArgument()) << path;
  EXPECT_THAT(transport.Patch(path, "{}"), IsStatusInvalidArgument()) << path;
  EXPECT_THAT(transport.Delete(path, ""), IsStatusInvalidArgument()) << path;
  EXPECT_THAT(transport.Put(path, "{}"), IsStatusInvalidArgument()) << path;
}

TEST_F(HttpRedfishTransportTest, RejectsNonOriginTargetsAcrossRestMethods) {
  auto mock_client = std::make_unique<MockHttpClient>();
  EXPECT_CALL(*mock_client, Get).Times(0);
  EXPECT_CALL(*mock_client, Post).Times(0);
  EXPECT_CALL(*mock_client, Delete).Times(0);
  EXPECT_CALL(*mock_client, Patch).Times(0);
  EXPECT_CALL(*mock_client, Put).Times(0);
  std::unique_ptr<HttpRedfishTransport> mock_network_transport =
      HttpRedfishTransport::MakeNetwork(std::move(mock_client),
                                        "http://localhost:8000");

  for (absl::string_view path : kRejectedRequestTargets) {
    ExpectRejectedOnAllRestMethods(*transport_, path);
    ExpectRejectedOnAllRestMethods(*mock_network_transport, path);
  }

  constexpr absl::string_view kEmbeddedNullPath("/redfish/v1\0/sub", 16);
  ExpectRejectedOnAllRestMethods(*transport_, kEmbeddedNullPath);
  ExpectRejectedOnAllRestMethods(*mock_network_transport, kEmbeddedNullPath);
}

TEST(HttpRedfishTransportUdsTest, ValidatesPathsOnUdsTransport) {
  {
    auto mock_client = std::make_unique<MockHttpClient>();
    EXPECT_CALL(*mock_client, Get).Times(0);
    EXPECT_CALL(*mock_client, Post).Times(0);
    EXPECT_CALL(*mock_client, Delete).Times(0);
    EXPECT_CALL(*mock_client, Patch).Times(0);
    EXPECT_CALL(*mock_client, Put).Times(0);
    std::unique_ptr<HttpRedfishTransport> uds_transport =
        HttpRedfishTransport::MakeUds(std::move(mock_client),
                                      "/tmp/test.socket");
    for (absl::string_view path : kRejectedRequestTargets) {
      ExpectRejectedOnAllRestMethods(*uds_transport, path);
    }
    constexpr absl::string_view kEmbeddedNullPath("/redfish/v1\0/sub", 16);
    ExpectRejectedOnAllRestMethods(*uds_transport, kEmbeddedNullPath);
  }

  auto mock_client = std::make_unique<MockHttpClient>();
  EXPECT_CALL(*mock_client, Get)
      .WillOnce([](std::unique_ptr<HttpClient::HttpRequest> req)
                    -> absl::StatusOr<HttpClient::HttpResponse> {
        EXPECT_THAT(req->uri, Eq("dummy/redfish/v1"));
        EXPECT_THAT(req->unix_socket_path, Eq("/tmp/test.socket"));
        HttpClient::HttpResponse resp;
        resp.code = 200;
        resp.body = "{}";
        resp.headers["Content-Type"] = "application/json";
        return resp;
      })
      .WillOnce([](std::unique_ptr<HttpClient::HttpRequest> req)
                    -> absl::StatusOr<HttpClient::HttpResponse> {
        EXPECT_THAT(req->uri, Eq("dummy/redfish/v1/Chassis/1/Assembly"));
        EXPECT_THAT(req->unix_socket_path, Eq("/tmp/test.socket"));
        HttpClient::HttpResponse resp;
        resp.code = 200;
        resp.body = "{}";
        resp.headers["Content-Type"] = "application/json";
        return resp;
      });

  std::unique_ptr<HttpRedfishTransport> uds_transport =
      HttpRedfishTransport::MakeUds(std::move(mock_client), "/tmp/test.socket");
  absl::StatusOr<RedfishTransport::Result> root_result =
      uds_transport->Get("/redfish/v1");
  ASSERT_TRUE(root_result.ok()) << root_result.status().message();
  EXPECT_THAT(root_result->code, Eq(200));

  absl::StatusOr<RedfishTransport::Result> fragment_result = uds_transport->Get(
      "/redfish/v1/Chassis/1/Assembly#/Members/PCIe Device rack1/0");
  ASSERT_TRUE(fragment_result.ok()) << fragment_result.status().message();
  EXPECT_THAT(fragment_result->code, Eq(200));
}

TEST_F(HttpRedfishTransportTest,
       AllowsValidRelativePathsQueriesAndStripsFragments) {
  struct ValidTargetCase {
    absl::string_view input_path;
    absl::string_view expected_wire_target;
  };
  static constexpr ValidTargetCase kValidCases[] = {
      {"/", "/"},
      {"/redfish/v1/AccountService/Accounts/user@example.com",
       "/redfish/v1/AccountService/Accounts/user@example.com"},
      {"/redfish/v1/Chassis/1/Assembly#/Members/PCIe Device rack1/0",
       "/redfish/v1/Chassis/1/Assembly"},
      {"/redfish/v1/Registries/%23SUTReg.v2_5_0.SUTReg",
       "/redfish/v1/Registries/%23SUTReg.v2_5_0.SUTReg"},
      {"/redfish/v1/Chassis/1/Sensors/DIMMA~CTemp",
       "/redfish/v1/Chassis/1/Sensors/DIMMA~CTemp"},
      {"/redfish/v1/$metadata", "/redfish/v1/$metadata"},
      {"/redfish/v1//Systems", "/redfish/v1//Systems"},
      {"/redfish/v1/Managers/iDRAC.Embedded.1",
       "/redfish/v1/Managers/iDRAC.Embedded.1"},
      {"/redfish/v1/.well-known", "/redfish/v1/.well-known"},
      {"/redfish/v1/Systems/...", "/redfish/v1/Systems/..."},
      {"/redfish/v1/Chassis?$expand=~($levels=1)",
       "/redfish/v1/Chassis?$expand=~($levels=1)"},
      {"/redfish/v1/Chassis/1/Sensors?$filter=Reading%20gt%2040",
       "/redfish/v1/Chassis/1/Sensors?$filter=Reading%20gt%2040"},
      {"/redfish/v1/Chassis/1/Sensors?$filter=Name%20eq%20'a..b'",
       "/redfish/v1/Chassis/1/Sensors?$filter=Name%20eq%20'a..b'"},
  };

  bool called = false;
  std::string observed_wire_uri;
  for (const ValidTargetCase& test_case : kValidCases) {
    called = false;
    observed_wire_uri.clear();
    server_->AddHttpGetHandler(
        std::string(test_case.expected_wire_target),
        [&](::tensorflow::serving::net_http::ServerRequestInterface* req) {
          called = true;
          observed_wire_uri = std::string(req->uri_path());
          ::tensorflow::serving::net_http::SetContentType(req,
                                                          "application/json");
          req->OverwriteResponseHeader("OData-Version", "4.0");
          req->WriteResponseString("{}");
          req->Reply();
        });

    absl::StatusOr<RedfishTransport::Result> result =
        transport_->Get(test_case.input_path);
    ASSERT_TRUE(result.ok())
        << test_case.input_path << ": " << result.status().message();
    EXPECT_TRUE(called) << test_case.input_path;
    EXPECT_THAT(observed_wire_uri, Eq(test_case.expected_wire_target));
    EXPECT_THAT(result->code, Eq(200));
  }

  // Non-ASCII bytes (>= 0x80) remain permitted in request targets.
  auto mock_client = std::make_unique<MockHttpClient>();
  EXPECT_CALL(*mock_client, Get)
      .WillOnce([](std::unique_ptr<HttpClient::HttpRequest> req)
                    -> absl::StatusOr<HttpClient::HttpResponse> {
        EXPECT_THAT(req->uri,
                    Eq("http://localhost:8000/redfish/v1/Systems/\xc3\xa9"));
        HttpClient::HttpResponse resp;
        resp.code = 200;
        resp.body = "{}";
        resp.headers["Content-Type"] = "application/json";
        return resp;
      });
  std::unique_ptr<HttpRedfishTransport> mock_transport =
      HttpRedfishTransport::MakeNetwork(std::move(mock_client),
                                        "http://localhost:8000");
  absl::StatusOr<RedfishTransport::Result> non_ascii_result =
      mock_transport->Get("/redfish/v1/Systems/\xc3\xa9");
  ASSERT_TRUE(non_ascii_result.ok()) << non_ascii_result.status().message();
  EXPECT_THAT(non_ascii_result->code, Eq(200));
}

std::unique_ptr<HttpRedfishTransport> MakeSchemeNetworkTransport(
    const FakeRedfishServer::Config& cfg) {
  HttpCredential creds;
  auto client =
      std::make_unique<CurlHttpClient>(LibCurlProxy::CreateInstance(), creds);
  return HttpRedfishTransport::MakeNetwork(
      std::move(client),
      absl::StrFormat("http://%s:%d", cfg.hostname, cfg.port));
}

TEST_F(HttpRedfishTransportTest, RejectsInvalidSessionDiscoveryTargets) {
  FakeRedfishServer secondary_server(
      "barebones_session_auth/mockup.shar",
      absl::StrCat(GetTestTempUdsDirectory(), "/secondary_discovery.socket"));
  const FakeRedfishServer::Config secondary_cfg = secondary_server.GetConfig();

  std::atomic<int> secondary_hits{0};
  auto record_secondary_hit =
      [&](::tensorflow::serving::net_http::ServerRequestInterface* req) {
        ++secondary_hits;
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->WriteResponseString("{}");
        req->ReplyWithStatus(
            ::tensorflow::serving::net_http::HTTPStatusCode::CREATED);
      };
  secondary_server.AddHttpPostHandler("/unexpected", record_secondary_hit);
  secondary_server.AddHttpPostHandler("/unexpected/Sessions",
                                      record_secondary_hit);

  const FakeRedfishServer::Config primary_cfg = server_->GetConfig();

  // Invalid Links.Sessions.@odata.id in service root fails closed even when
  // SessionService.@odata.id is also present.
  const std::string invalid_links_targets[] = {
      absl::StrFormat("@%s:%d/unexpected", secondary_cfg.hostname,
                      secondary_cfg.port),
      absl::StrFormat("//%s:%d/unexpected", secondary_cfg.hostname,
                      secondary_cfg.port),
      "/redfish/v1/../unexpected",
  };
  for (const std::string& bad_target : invalid_links_targets) {
    std::string invalid_links_root = absl::StrFormat(
        R"({"@odata.id":"/redfish/v1","Links":{"Sessions":{"@odata.id":"%s"}},"SessionService":{"@odata.id":"/redfish/v1/SessionService"}})",
        bad_target);
    server_->AddHttpGetHandlerWithOwnedData("/redfish/v1", invalid_links_root);
    std::unique_ptr<HttpRedfishTransport> transport =
        MakeSchemeNetworkTransport(primary_cfg);
    EXPECT_THAT(transport->DoSessionAuth("test_username", "test_password"),
                IsStatusInvalidArgument())
        << bad_target;
    EXPECT_THAT(secondary_hits.load(), Eq(0));
  }

  // Invalid SessionService.@odata.id in service root (including '?query' and
  // '#fragment') fails closed.
  const std::string invalid_service_targets[] = {
      absl::StrFormat("@%s:%d/unexpected", secondary_cfg.hostname,
                      secondary_cfg.port),
      "/redfish/v1/../SessionService",
      "/redfish/v1/SessionService?filter=1",
      "/redfish/v1/SessionService#fragment",
  };
  for (const std::string& bad_service : invalid_service_targets) {
    std::string invalid_service_root = absl::StrFormat(
        R"({"@odata.id":"/redfish/v1","SessionService":{"@odata.id":"%s"}})",
        bad_service);
    server_->AddHttpGetHandlerWithOwnedData("/redfish/v1",
                                            invalid_service_root);
    std::unique_ptr<HttpRedfishTransport> transport =
        MakeSchemeNetworkTransport(primary_cfg);
    EXPECT_THAT(transport->DoSessionAuth("test_username", "test_password"),
                IsStatusInvalidArgument())
        << bad_service;
    EXPECT_THAT(secondary_hits.load(), Eq(0));
  }
}

TEST_F(HttpRedfishTransportTest,
       NormalizesTrailingSlashSessionServiceAndOriginLocation) {
  std::atomic<int> primary_delete_hits{0};
  server_->AddHttpGetHandlerWithOwnedData(
      "/redfish/v1",
      R"({"@odata.id":"/redfish/v1","SessionService":{"@odata.id":"/redfish/v1/SessionService/"}})");
  server_->AddHttpPostHandler(
      "/redfish/v1/SessionService/Sessions",
      [](::tensorflow::serving::net_http::ServerRequestInterface* req) {
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->WriteResponseString("{}");
        req->AppendResponseHeader("X-Auth-Token", "token123");
        req->AppendResponseHeader(
            "Location", "/redfish/v1/SessionService/Sessions/1#Session");
        req->Reply();
      });
  server_->AddHttpDeleteHandler(
      "/redfish/v1/SessionService/Sessions/1",
      [&](::tensorflow::serving::net_http::ServerRequestInterface* req) {
        ++primary_delete_hits;
        EXPECT_THAT(req->GetRequestHeader("X-Auth-Token"), Eq("token123"));
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->ReplyWithStatus(
            ::tensorflow::serving::net_http::HTTPStatusCode::NO_CONTENT);
      });

  std::unique_ptr<HttpRedfishTransport> transport =
      MakeSchemeNetworkTransport(server_->GetConfig());
  EXPECT_THAT(transport->DoSessionAuth("test_username", "test_password"),
              IsOk());
  transport.reset();
  EXPECT_THAT(primary_delete_hits.load(), Eq(1));
}

TEST_F(HttpRedfishTransportTest, RejectsInvalidSessionLocationHeaders) {
  FakeRedfishServer secondary_server(
      "barebones_session_auth/mockup.shar",
      absl::StrCat(GetTestTempUdsDirectory(), "/secondary_bad_loc.socket"));
  const FakeRedfishServer::Config secondary_cfg = secondary_server.GetConfig();

  std::atomic<int> secondary_hits{0};
  auto record_secondary_delete =
      [&](::tensorflow::serving::net_http::ServerRequestInterface* req) {
        ++secondary_hits;
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->ReplyWithStatus(
            ::tensorflow::serving::net_http::HTTPStatusCode::NO_CONTENT);
      };
  secondary_server.AddHttpDeleteHandler("/unexpected", record_secondary_delete);
  secondary_server.AddHttpDeleteHandler("/redfish/v1/SessionService/Sessions/1",
                                        record_secondary_delete);

  std::atomic<int> primary_delete_hits{0};
  server_->AddHttpDeleteHandler(
      "/redfish/v1/SessionService/Sessions/1",
      [&](::tensorflow::serving::net_http::ServerRequestInterface* req) {
        ++primary_delete_hits;
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->ReplyWithStatus(
            ::tensorflow::serving::net_http::HTTPStatusCode::NO_CONTENT);
      });

  const FakeRedfishServer::Config primary_cfg = server_->GetConfig();
  const std::string invalid_locations[] = {
      "",
      "redfish/v1/SessionService/Sessions/1",
      absl::StrFormat("http://%s:%d/redfish/v1/SessionService/Sessions/1",
                      primary_cfg.hostname, primary_cfg.port),
      absl::StrFormat("https://%s:%d/redfish/v1/SessionService/Sessions/1",
                      primary_cfg.hostname, primary_cfg.port),
      absl::StrFormat("http://%s:%d/redfish/v1/SessionService/Sessions/1",
                      secondary_cfg.hostname, secondary_cfg.port),
      absl::StrFormat("https://%s:%d/redfish/v1/SessionService/Sessions/1",
                      secondary_cfg.hostname, secondary_cfg.port),
      absl::StrFormat("@%s:%d/unexpected", secondary_cfg.hostname,
                      secondary_cfg.port),
      absl::StrFormat("//%s:%d/unexpected", secondary_cfg.hostname,
                      secondary_cfg.port),
      "/redfish/v1/../Sessions/1",
  };
  server_->AddHttpGetHandlerWithOwnedData(
      "/redfish/v1",
      R"({"@odata.id":"/redfish/v1","Links":{"Sessions":{"@odata.id":"/redfish/v1/SessionService/Sessions"}}})");
  for (const std::string& bad_location : invalid_locations) {
    server_->AddHttpPostHandler(
        "/redfish/v1/SessionService/Sessions",
        [bad_location](
            ::tensorflow::serving::net_http::ServerRequestInterface* req) {
          ::tensorflow::serving::net_http::SetContentType(req,
                                                          "application/json");
          req->WriteResponseString("{}");
          req->AppendResponseHeader("X-Auth-Token", "token123");
          req->AppendResponseHeader("Location", bad_location);
          req->Reply();
        });
    std::unique_ptr<HttpRedfishTransport> transport =
        MakeSchemeNetworkTransport(primary_cfg);
    EXPECT_THAT(transport->DoSessionAuth("test_username", "test_password"),
                IsStatusInternal())
        << bad_location;
    transport.reset();
    EXPECT_THAT(primary_delete_hits.load(), Eq(0));
    EXPECT_THAT(secondary_hits.load(), Eq(0));
  }
}

TEST_F(HttpRedfishTransportTest, DoesNotFollowRedirectResponses) {
  FakeRedfishServer secondary_server(
      "barebones_session_auth/mockup.shar",
      absl::StrCat(GetTestTempUdsDirectory(), "/secondary_redirect.socket"));
  const FakeRedfishServer::Config secondary_cfg = secondary_server.GetConfig();

  std::atomic<int> secondary_hits{0};
  secondary_server.AddHttpGetHandler(
      "/unexpected",
      [&](::tensorflow::serving::net_http::ServerRequestInterface* req) {
        ++secondary_hits;
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->WriteResponseString("{}");
        req->Reply();
      });

  const std::string redirect_location = absl::StrFormat(
      "http://%s:%d/unexpected", secondary_cfg.hostname, secondary_cfg.port);
  server_->AddHttpGetHandler(
      "/redfish/v1/redirect",
      [redirect_location](
          ::tensorflow::serving::net_http::ServerRequestInterface* req) {
        req->AppendResponseHeader("Location", redirect_location);
        req->ReplyWithStatus(
            ::tensorflow::serving::net_http::HTTPStatusCode::MOVED_TEMP);
      });
  std::unique_ptr<HttpRedfishTransport> transport =
      MakeSchemeNetworkTransport(server_->GetConfig());
  absl::StatusOr<RedfishTransport::Result> redirect_result =
      transport->Get("/redfish/v1/redirect");
  ASSERT_TRUE(redirect_result.ok()) << redirect_result.status().message();
  EXPECT_THAT(redirect_result->code, Eq(302));
  EXPECT_THAT(secondary_hits.load(), Eq(0));
}
}  // namespace
}  // namespace ecclesia
