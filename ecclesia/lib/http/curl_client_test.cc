/*
 * Copyright 2020 Google LLC
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

#include "ecclesia/lib/http/curl_client.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "curl/curl.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/http/client.h"
#include "ecclesia/lib/http/cred.pb.h"
#include "ecclesia/lib/redfish/testing/fake_redfish_server.h"
#include "single_include/nlohmann/json.hpp"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"

namespace ecclesia {
namespace {

using testing::Eq;
using testing::Field;
using testing::Gt;
using testing::Invoke;
using testing::IsEmpty;
using testing::Not;
using testing::Return;

HttpCredential GetSimpleCredential() {
  auto cred = HttpCredential();
  cred.set_username("user");
  cred.set_password("password");
  return cred;
}

class CurlHttpClientTest : public ::testing::Test {
 protected:
  CurlHttpClientTest()
      : server_("barebones_session_auth/mockup.shar"),
        endpoint_(absl::StrFormat("%s:%d", server_.GetConfig().hostname,
                                  server_.GetConfig().port)),
        curl_http_client_(LibCurlProxy::CreateInstance(), HttpCredential()) {}

  FakeRedfishServer server_;
  const std::string endpoint_;
  CurlHttpClient curl_http_client_;
};

TEST_F(CurlHttpClientTest, CanGet) {
  auto req = std::make_unique<HttpClient::HttpRequest>();
  req->uri = absl::StrFormat("%s/redfish/v1", endpoint_);
  auto result = curl_http_client_.Get(std::move(req));
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
  EXPECT_THAT(result->GetBodyJson(), Eq(expected));
}

TEST_F(CurlHttpClientTest, GetInvalidJson) {
  bool called = false;
  server_.AddHttpGetHandler(
      "/redfish/v1",
      [&](::tensorflow::serving::net_http::ServerRequestInterface *req) {
        called = true;
        // Construct the success message.
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        // Return some invalid JSON.
        req->WriteResponseString("{");
        req->Reply();
      });

  auto req = std::make_unique<HttpClient::HttpRequest>();
  req->uri = absl::StrFormat("%s/redfish/v1", endpoint_);
  auto result = curl_http_client_.Get(std::move(req));
  ASSERT_TRUE(called);
  EXPECT_TRUE(result.ok()) << result.status().message();
  EXPECT_THAT(result->code, Eq(200));
  EXPECT_TRUE(result->GetBodyJson().is_discarded());
}

TEST_F(CurlHttpClientTest, GetError) {
  auto result_json = nlohmann::json::parse(R"json({
  "error": {
    "code": "Base.1.0.GeneralError",
    "message": "A general error has occurred.  See Resolution for information on how to resolve the error, or @Message.ExtendedInfo if Resolution is not provided."
  }
})json");

  bool called = false;
  server_.AddHttpGetHandler(
      "/redfish/v1",
      [&](::tensorflow::serving::net_http::ServerRequestInterface *req) {
        called = true;
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->WriteResponseString(result_json.dump());
        req->ReplyWithStatus(
            ::tensorflow::serving::net_http::HTTPStatusCode::ERROR);
      });

  auto req = std::make_unique<HttpClient::HttpRequest>();
  req->uri = absl::StrFormat("%s/redfish/v1", endpoint_);
  auto result = curl_http_client_.Get(std::move(req));
  ASSERT_TRUE(called);
  EXPECT_TRUE(result.ok()) << result.status().message();
  EXPECT_THAT(result->code, Eq(500));
  EXPECT_THAT(result->GetBodyJson(), Eq(result_json));
}

TEST_F(CurlHttpClientTest, CanPost) {
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
  server_.AddHttpPostHandler(
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

  auto req = std::make_unique<HttpClient::HttpRequest>();
  req->uri = absl::StrFormat("%s/redfish/v1/Chassis/1/Actions/Chassis.Reset",
                             endpoint_);
  req->body = request_json.dump();
  auto result = curl_http_client_.Post(std::move(req));
  ASSERT_TRUE(called);
  ASSERT_TRUE(result.ok()) << result.status().message();
  EXPECT_THAT(result->code, Eq(200));
  EXPECT_THAT(result->GetBodyJson(), Eq(result_json));
}

TEST_F(CurlHttpClientTest, PostInvalidJson) {
  auto request_json = nlohmann::json::parse(R"json({
  "ResetType": "PowerCycle"
})json");
  bool called = false;
  server_.AddHttpPostHandler(
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

  auto req = std::make_unique<HttpClient::HttpRequest>();
  req->uri = absl::StrFormat("%s/redfish/v1/Chassis/1/Actions/Chassis.Reset",
                             endpoint_);
  req->body = request_json.dump();
  auto result = curl_http_client_.Post(std::move(req));
  ASSERT_TRUE(called);
  EXPECT_TRUE(result.ok()) << result.status().message();
  EXPECT_THAT(result->code, Eq(200));
  EXPECT_TRUE(result->GetBodyJson().is_discarded());
}

TEST_F(CurlHttpClientTest, PostError) {
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
  server_.AddHttpPostHandler(
      "/redfish/v1/Chassis/1/Actions/Chassis.Reset",
      [&](::tensorflow::serving::net_http::ServerRequestInterface *req) {
        called = true;
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->WriteResponseString(result_json.dump());
        req->ReplyWithStatus(
            ::tensorflow::serving::net_http::HTTPStatusCode::ERROR);
      });

  auto req = std::make_unique<HttpClient::HttpRequest>();
  req->uri = absl::StrFormat("%s/redfish/v1/Chassis/1/Actions/Chassis.Reset",
                             endpoint_);
  req->body = request_json.dump();
  auto result = curl_http_client_.Post(std::move(req));
  ASSERT_TRUE(called);
  EXPECT_TRUE(result.ok()) << result.status().message();
  EXPECT_THAT(result->code, Eq(500));
  EXPECT_THAT(result->GetBodyJson(), Eq(result_json));
}

TEST_F(CurlHttpClientTest, CanPatch) {
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
  server_.AddHttpPatchHandler(
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

  auto req = std::make_unique<HttpClient::HttpRequest>();
  req->uri = absl::StrFormat("%s/redfish/v1/Chassis/1", endpoint_);
  req->body = request_json.dump();
  auto result = curl_http_client_.Patch(std::move(req));
  ASSERT_TRUE(called);
  ASSERT_TRUE(result.ok()) << result.status().message();
  EXPECT_THAT(result->code, Eq(200));
  EXPECT_THAT(result->GetBodyJson(), Eq(result_json));
}

TEST_F(CurlHttpClientTest, PatchInvalidJson) {
  auto request_json = nlohmann::json::parse(R"json({
  "Name": "TestName"
})json");
  bool called = false;
  server_.AddHttpPatchHandler(
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

  auto req = std::make_unique<HttpClient::HttpRequest>();
  req->uri = absl::StrFormat("%s/redfish/v1/Chassis/1", endpoint_);
  req->body = request_json.dump();
  auto result = curl_http_client_.Patch(std::move(req));
  ASSERT_TRUE(called);
  EXPECT_TRUE(result.ok()) << result.status().message();
  EXPECT_THAT(result->code, Eq(200));
  EXPECT_TRUE(result->GetBodyJson().is_discarded());
}

TEST_F(CurlHttpClientTest, PatchError) {
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
  server_.AddHttpPatchHandler(
      "/redfish/v1/Chassis/1",
      [&](::tensorflow::serving::net_http::ServerRequestInterface *req) {
        called = true;
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->WriteResponseString(result_json.dump());
        req->ReplyWithStatus(
            ::tensorflow::serving::net_http::HTTPStatusCode::ERROR);
      });

  auto req = std::make_unique<HttpClient::HttpRequest>();
  req->uri = absl::StrFormat("%s/redfish/v1/Chassis/1", endpoint_);
  req->body = request_json.dump();
  auto result = curl_http_client_.Patch(std::move(req));
  ASSERT_TRUE(called);
  EXPECT_TRUE(result.ok()) << result.status().message();
  EXPECT_THAT(result->code, Eq(500));
  EXPECT_THAT(result->GetBodyJson(), Eq(result_json));
}

TEST_F(CurlHttpClientTest, CanDelete) {
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
  server_.AddHttpDeleteHandler(
      "/redfish/v1/Chassis/1",
      [&](::tensorflow::serving::net_http::ServerRequestInterface *req) {
        called = true;
        // Construct the success message.
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->WriteResponseString(result_json.dump());
        req->Reply();
      });

  auto req = std::make_unique<HttpClient::HttpRequest>();
  req->uri = absl::StrFormat("%s/redfish/v1/Chassis/1", endpoint_);
  auto result = curl_http_client_.Delete(std::move(req));
  ASSERT_TRUE(called);
  ASSERT_TRUE(result.ok()) << result.status().message();
  EXPECT_THAT(result->code, Eq(200));
  EXPECT_THAT(result->GetBodyJson(), Eq(result_json));
}

TEST_F(CurlHttpClientTest, DeleteError) {
  auto result_json = nlohmann::json::parse(R"json({
  "error": {
    "code": "Base.1.0.GeneralError",
    "message": "A general error has occurred.  See Resolution for information on how to resolve the error, or @Message.ExtendedInfo if Resolution is not provided."
  }
})json");

  bool called = false;
  server_.AddHttpDeleteHandler(
      "/redfish/v1/Chassis/1",
      [&](::tensorflow::serving::net_http::ServerRequestInterface *req) {
        called = true;
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->WriteResponseString(result_json.dump());
        req->ReplyWithStatus(
            ::tensorflow::serving::net_http::HTTPStatusCode::ERROR);
      });

  auto req = std::make_unique<HttpClient::HttpRequest>();
  req->uri = absl::StrFormat("%s/redfish/v1/Chassis/1", endpoint_);
  auto result = curl_http_client_.Delete(std::move(req));
  ASSERT_TRUE(called);
  EXPECT_TRUE(result.ok()) << result.status().message();
  EXPECT_THAT(result->code, Eq(500));
  EXPECT_THAT(result->GetBodyJson(), Eq(result_json));
}

class MockIncrementalResponseHandler
    : public HttpClient::IncrementalResponseHandler {
 public:
  MOCK_METHOD(absl::Status, OnResponseHeaders,
              (const HttpClient::HttpResponse &), (override));
  MOCK_METHOD(absl::Status, OnBodyData, (absl::string_view), (override));
  MOCK_METHOD(bool, IsCancelled, (), (const, override));
};

TEST_F(CurlHttpClientTest, CanGetIncremental) {
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

  auto req = std::make_unique<HttpClient::HttpRequest>();
  req->uri = absl::StrFormat("%s/redfish/v1", endpoint_);
  std::string body;
  MockIncrementalResponseHandler mock_handler;
  EXPECT_CALL(mock_handler, IsCancelled()).WillRepeatedly(Return(false));

  testing::InSequence seq;
  EXPECT_CALL(mock_handler,
              OnResponseHeaders(Field(&HttpClient::HttpResponse::code, 200)))
      .WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(mock_handler, OnBodyData(Not(IsEmpty())))
      .WillRepeatedly(Invoke([&body](absl::string_view part) {
        body.append(part);
        return absl::OkStatus();
      }));

  auto result = curl_http_client_.GetIncremental(std::move(req), &mock_handler);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(nlohmann::json::parse(body, /*cb=*/nullptr, false), expected);
}

TEST_F(CurlHttpClientTest, GetIncrementalError) {
  MockIncrementalResponseHandler mock_handler;
  EXPECT_CALL(mock_handler, IsCancelled()).WillRepeatedly(Return(false));
  EXPECT_CALL(mock_handler,
              OnResponseHeaders(Field(&HttpClient::HttpResponse::code, 200)))
      .WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(mock_handler, OnBodyData(Not(IsEmpty())))
      .WillOnce(Return(absl::InternalError("test error")));
  auto req = std::make_unique<HttpClient::HttpRequest>();
  req->uri = absl::StrFormat("%s/redfish/v1/Chassis", endpoint_);
  auto result = curl_http_client_.GetIncremental(std::move(req), &mock_handler);
  EXPECT_EQ(result.code(), absl::StatusCode::kInternal);
  EXPECT_EQ(result.message(), "test error");
}

TEST_F(CurlHttpClientTest, GetIncrementalCancelled) {
  MockIncrementalResponseHandler mock_handler;
  EXPECT_CALL(mock_handler, OnResponseHeaders).Times(0);
  EXPECT_CALL(mock_handler, OnBodyData).Times(0);
  EXPECT_CALL(mock_handler, IsCancelled()).WillRepeatedly(Return(true));
  auto req = std::make_unique<HttpClient::HttpRequest>();
  req->uri = absl::StrFormat("%s/redfish/v1/Chassis", endpoint_);
  auto result = curl_http_client_.GetIncremental(std::move(req), &mock_handler);
  EXPECT_EQ(result.code(), absl::StatusCode::kCancelled);
}

TEST_F(CurlHttpClientTest, CanPostIncremental) {
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
  server_.AddHttpPostHandler(
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

  auto req = std::make_unique<HttpClient::HttpRequest>();
  req->uri = absl::StrFormat("%s/redfish/v1/Chassis/1/Actions/Chassis.Reset",
                             endpoint_);
  req->body = request_json.dump();
  std::string body;
  MockIncrementalResponseHandler mock_handler;
  ON_CALL(mock_handler, IsCancelled()).WillByDefault(Return(false));

  testing::InSequence seq;
  EXPECT_CALL(mock_handler,
              OnResponseHeaders(Field(&HttpClient::HttpResponse::code, 200)))
      .WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(mock_handler, OnBodyData(Not(IsEmpty())))
      .WillRepeatedly(Invoke([&body](absl::string_view part) {
        body.append(part);
        return absl::OkStatus();
      }));

  auto result =
      curl_http_client_.PostIncremental(std::move(req), &mock_handler);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(nlohmann::json::parse(body, /*cb=*/nullptr, false), result_json);
}

TEST_F(CurlHttpClientTest, CanPatchIncremental) {
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
  server_.AddHttpPatchHandler(
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

  auto req = std::make_unique<HttpClient::HttpRequest>();
  req->uri = absl::StrFormat("%s/redfish/v1/Chassis/1", endpoint_);
  req->body = request_json.dump();
  std::string body;
  MockIncrementalResponseHandler mock_handler;
  ON_CALL(mock_handler, IsCancelled()).WillByDefault(Return(false));

  testing::InSequence seq;
  EXPECT_CALL(mock_handler,
              OnResponseHeaders(Field(&HttpClient::HttpResponse::code, 200)))
      .WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(mock_handler, OnBodyData(Not(IsEmpty())))
      .WillRepeatedly(Invoke([&body](absl::string_view part) {
        body.append(part);
        return absl::OkStatus();
      }));

  auto result =
      curl_http_client_.PatchIncremental(std::move(req), &mock_handler);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(nlohmann::json::parse(body, /*cb=*/nullptr, false), result_json);
}

TEST_F(CurlHttpClientTest, CanDeleteIncremental) {
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
  server_.AddHttpDeleteHandler(
      "/redfish/v1/Chassis/1",
      [&](::tensorflow::serving::net_http::ServerRequestInterface *req) {
        called = true;
        // Construct the success message.
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->WriteResponseString(result_json.dump());
        req->Reply();
      });

  auto req = std::make_unique<HttpClient::HttpRequest>();
  req->uri = absl::StrFormat("%s/redfish/v1/Chassis/1", endpoint_);
  std::string body;
  MockIncrementalResponseHandler mock_handler;
  EXPECT_CALL(mock_handler, IsCancelled()).WillRepeatedly(Return(false));

  testing::InSequence seq;
  EXPECT_CALL(mock_handler,
              OnResponseHeaders(Field(&HttpClient::HttpResponse::code, 200)))
      .WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(mock_handler, OnBodyData(Not(IsEmpty())))
      .WillRepeatedly(Invoke([&body](absl::string_view part) {
        body.append(part);
        return absl::OkStatus();
      }));

  auto result =
      curl_http_client_.DeleteIncremental(std::move(req), &mock_handler);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(nlohmann::json::parse(body, /*cb=*/nullptr, false), result_json);
}

}  // namespace
}  // namespace ecclesia
