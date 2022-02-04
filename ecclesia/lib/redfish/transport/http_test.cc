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

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/http/client.h"
#include "ecclesia/lib/http/cred.pb.h"
#include "ecclesia/lib/http/curl_client.h"
#include "ecclesia/lib/redfish/testing/fake_redfish_server.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/testing/status.h"
#include "single_include/nlohmann/json.hpp"
#include "tensorflow_serving/util/net_http/server/public/response_code_enum.h"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"

namespace ecclesia {
namespace {

using ::testing::Eq;
using ::testing::Gt;

class HttpRedfishTransportTest : public ::testing::Test {
 protected:
  HttpRedfishTransportTest() {
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
  nlohmann::json expected =
      nlohmann::json::parse(R"json({
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
})json",
                            nullptr, /*allow_exceptions=*/false);
  EXPECT_THAT(result->body, Eq(expected))
      << "Diff: " << nlohmann::json::diff(result->body, expected);
}

TEST_F(HttpRedfishTransportTest, GetInvalidJson) {
  bool called = false;
  server_->AddHttpGetHandler(
      "/redfish/v1/Chassis/1/Actions/Chassis.Reset",
      [&](::tensorflow::serving::net_http::ServerRequestInterface *req) {
        called = true;
        // Construct the success message.
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        // Return some invalid JSON.
        req->WriteResponseString("{");
        req->Reply();
      });

  auto result = transport_->Get("/redfish/v1/Chassis/1/Actions/Chassis.Reset");
  ASSERT_TRUE(called);
  EXPECT_TRUE(result.ok()) << result.status().message();
  EXPECT_THAT(result->code, Eq(200));
  EXPECT_TRUE(result->body.is_discarded());
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
      [&](::tensorflow::serving::net_http::ServerRequestInterface *req) {
        called = true;
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->WriteResponseString(result_json.dump());
        req->ReplyWithStatus(
            ::tensorflow::serving::net_http::HTTPStatusCode::ERROR);
      });

  auto result = transport_->Get("/redfish/v1/Chassis/1/Actions/Chassis.Reset");
  ASSERT_TRUE(called);
  EXPECT_TRUE(result.ok()) << result.status().message();
  EXPECT_THAT(result->code, Eq(500));
  EXPECT_THAT(result->body, Eq(result_json));
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
      [&](::tensorflow::serving::net_http::ServerRequestInterface *req) {
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
        req->Reply();
      });

  auto result = transport_->Post("/redfish/v1/Chassis/1/Actions/Chassis.Reset",
                                 request_json.dump());
  ASSERT_TRUE(called);
  ASSERT_TRUE(result.ok()) << result.status().message();
  EXPECT_THAT(result->code, Eq(200));
  EXPECT_THAT(result->body, Eq(result_json))
      << "Diff: " << nlohmann::json::diff(result->body, result_json);
}

TEST_F(HttpRedfishTransportTest, PostInvalidJson) {
  auto request_json = nlohmann::json::parse(R"json({
  "ResetType": "PowerCycle"
})json");
  bool called = false;
  server_->AddHttpPostHandler(
      "/redfish/v1/Chassis/1/Actions/Chassis.Reset",
      [&](::tensorflow::serving::net_http::ServerRequestInterface *req) {
        called = true;
        // Construct the success message.
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        // Return some invalid JSON.
        req->WriteResponseString("{");
        req->Reply();
      });

  auto result = transport_->Post("/redfish/v1/Chassis/1/Actions/Chassis.Reset",
                                 request_json.dump());
  ASSERT_TRUE(called);
  EXPECT_TRUE(result.ok()) << result.status().message();
  EXPECT_THAT(result->code, Eq(200));
  EXPECT_TRUE(result->body.is_discarded());
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
      [&](::tensorflow::serving::net_http::ServerRequestInterface *req) {
        called = true;
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->WriteResponseString(result_json.dump());
        req->ReplyWithStatus(
            ::tensorflow::serving::net_http::HTTPStatusCode::ERROR);
      });

  auto result = transport_->Post("/redfish/v1/Chassis/1/Actions/Chassis.Reset",
                                 request_json.dump());
  ASSERT_TRUE(called);
  EXPECT_TRUE(result.ok()) << result.status().message();
  EXPECT_THAT(result->code, Eq(500));
  EXPECT_THAT(result->body, Eq(result_json));
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
      [&](::tensorflow::serving::net_http::ServerRequestInterface *req) {
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
        req->Reply();
      });

  auto result = transport_->Patch("/redfish/v1/Chassis/1", request_json.dump());
  ASSERT_TRUE(called);
  ASSERT_TRUE(result.ok()) << result.status().message();
  EXPECT_THAT(result->code, Eq(200));
  EXPECT_THAT(result->body, Eq(result_json))
      << "Diff: " << nlohmann::json::diff(result->body, result_json);
}

TEST_F(HttpRedfishTransportTest, PatchInvalidJson) {
  auto request_json = nlohmann::json::parse(R"json({
  "Name": "TestName"
})json");
  bool called = false;
  server_->AddHttpPatchHandler(
      "/redfish/v1/Chassis/1",
      [&](::tensorflow::serving::net_http::ServerRequestInterface *req) {
        called = true;
        // Construct the success message.
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        // Return some invalid JSON.
        req->WriteResponseString("{");
        req->Reply();
      });

  auto result = transport_->Patch("/redfish/v1/Chassis/1", request_json.dump());
  ASSERT_TRUE(called);
  EXPECT_TRUE(result.ok()) << result.status().message();
  EXPECT_THAT(result->code, Eq(200));
  EXPECT_TRUE(result->body.is_discarded());
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
      [&](::tensorflow::serving::net_http::ServerRequestInterface *req) {
        called = true;
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->WriteResponseString(result_json.dump());
        req->ReplyWithStatus(
            ::tensorflow::serving::net_http::HTTPStatusCode::ERROR);
      });

  auto result = transport_->Patch("/redfish/v1/Chassis/1", request_json.dump());
  ASSERT_TRUE(called);
  EXPECT_TRUE(result.ok()) << result.status().message();
  EXPECT_THAT(result->code, Eq(500));
  EXPECT_THAT(result->body, Eq(result_json));
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
      [&](::tensorflow::serving::net_http::ServerRequestInterface *req) {
        called = true;
        // Construct the success message.
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->WriteResponseString(result_json.dump());
        req->Reply();
      });

  auto result = transport_->Delete("/redfish/v1/Chassis/1", "");
  ASSERT_TRUE(called);
  ASSERT_TRUE(result.ok()) << result.status().message();
  EXPECT_THAT(result->code, Eq(200));
  EXPECT_THAT(result->body, Eq(result_json))
      << "Diff: " << nlohmann::json::diff(result->body, result_json);
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
      [&](::tensorflow::serving::net_http::ServerRequestInterface *req) {
        called = true;
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->WriteResponseString(result_json.dump());
        req->ReplyWithStatus(
            ::tensorflow::serving::net_http::HTTPStatusCode::ERROR);
      });

  auto result = transport_->Delete("/redfish/v1/Chassis/1", "");
  ASSERT_TRUE(called);
  EXPECT_TRUE(result.ok()) << result.status().message();
  EXPECT_THAT(result->code, Eq(500));
  EXPECT_THAT(result->body, Eq(result_json));
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
      [&](::tensorflow::serving::net_http::ServerRequestInterface *req) {
        // Construct the success message.
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->WriteResponseString(root_json.dump());
        req->Reply();
      };

  server_->AddHttpGetHandler("/redfish/v1", get_handler);
  server_->AddHttpGetHandler("/redfish/v1/", get_handler);

  bool called = false;
  server_->AddHttpPostHandler(
      "/redfish/v1/SessionService/Sessions",
      [&](::tensorflow::serving::net_http::ServerRequestInterface *req) {
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
      [&](::tensorflow::serving::net_http::ServerRequestInterface *req) {
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
      [&](::tensorflow::serving::net_http::ServerRequestInterface *req) {
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
      [&](::tensorflow::serving::net_http::ServerRequestInterface *req) {
        root_called = true;
        // Construct the success message.
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->WriteResponseString(root_json.dump());
        req->Reply();
      };

  server_->AddHttpGetHandler("/redfish/v1", get_handler);
  server_->AddHttpGetHandler("/redfish/v1/", get_handler);

  bool called = false;
  server_->AddHttpPostHandler(
      "/redfish/v1/SessionService/Sessions",
      [&](::tensorflow::serving::net_http::ServerRequestInterface *req) {
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
      [&](::tensorflow::serving::net_http::ServerRequestInterface *req) {
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
      [&](::tensorflow::serving::net_http::ServerRequestInterface *req) {
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
      [&](::tensorflow::serving::net_http::ServerRequestInterface *req) {
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
      [&](::tensorflow::serving::net_http::ServerRequestInterface *req) {
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
      [&](::tensorflow::serving::net_http::ServerRequestInterface *req) {
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
      [&](::tensorflow::serving::net_http::ServerRequestInterface *req) {
        root_called = true;
        // Construct the success message.
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
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
      [&](::tensorflow::serving::net_http::ServerRequestInterface *req) {
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
      [&](::tensorflow::serving::net_http::ServerRequestInterface *req) {
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
      [&](::tensorflow::serving::net_http::ServerRequestInterface *req) {
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
      [&](::tensorflow::serving::net_http::ServerRequestInterface *req) {
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
      [&](::tensorflow::serving::net_http::ServerRequestInterface *req) {
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
      [&](::tensorflow::serving::net_http::ServerRequestInterface *req) {
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
      [&](::tensorflow::serving::net_http::ServerRequestInterface *req) {
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
      [&](::tensorflow::serving::net_http::ServerRequestInterface *req) {
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

}  // namespace
}  // namespace ecclesia
