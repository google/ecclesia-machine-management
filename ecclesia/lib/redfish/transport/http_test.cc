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

#include <memory>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/http/cred.pb.h"
#include "ecclesia/lib/http/curl_client.h"
#include "ecclesia/lib/redfish/testing/fake_redfish_server.h"
#include "tensorflow_serving/util/net_http/server/public/response_code_enum.h"

namespace ecclesia {
namespace {

using ::testing::Eq;
using ::testing::Gt;

class HttpRedfishTransportTest : public ::testing::Test {
 protected:
  HttpRedfishTransportTest() {
    server_ = std::make_unique<FakeRedfishServer>(
        "barebones_session_auth/mockup.shar",
        absl::StrCat(GetTestTempUdsDirectory(), "/mockup.socket"));
    auto config = server_->GetConfig();
    HttpCredential creds;
    auto curl_http_client =
        std::make_unique<CurlHttpClient>(LibCurlProxy::CreateInstance(), creds);
    transport_ = HttpRedfishTransport::MakeNetwork(
        std::move(curl_http_client),
        absl::StrFormat("%s:%d", config.hostname, config.port));
  }

  std::unique_ptr<FakeRedfishServer> server_;
  std::unique_ptr<RedfishTransport> transport_;
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

}  // namespace
}  // namespace ecclesia
