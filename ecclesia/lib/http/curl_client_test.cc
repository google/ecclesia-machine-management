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
using testing::Gt;

HttpCredential GetSimpleCredential() {
  auto cred = HttpCredential();
  cred.set_username("user");
  cred.set_password("password");
  return cred;
}

class FakeLibCurl : public LibCurl {
 public:
  FakeLibCurl(absl::string_view response_content, uint64_t response_code)
      : response_content_(response_content), response_code_(response_code) {}

  FakeLibCurl(absl::string_view response_content, uint64_t response_code,
              const std::vector<std::string> &response_headers)
      : response_content_(response_content),
        response_code_(response_code),
        response_headers_(response_headers) {}

  CURL *curl_easy_init() override {
    is_initialized_ = true;
    return reinterpret_cast<CURL *>(this);
  }

  CURLcode curl_easy_setopt(CURL *curl, CURLoption option,
                            uint64_t param) override {
    switch (option) {
      case CURLOPT_POST:
        is_post_ = param;
        break;
      case CURLOPT_PUT:
        is_put_ = param;
        break;
      default:
        break;
    }
    return CURLE_OK;
  }
  CURLcode curl_easy_setopt(CURL *curl, CURLoption option,
                            const char *param) override {
    return curl_easy_setopt(
        curl, option, reinterpret_cast<void *>(const_cast<char *>(param)));
  }
  CURLcode curl_easy_setopt(CURL *curl, CURLoption option,
                            void *param) override {
    switch (option) {
      case CURLOPT_URL:
        url_ = reinterpret_cast<char *>(param);
        break;
      case CURLOPT_RANGE:
        range_ = reinterpret_cast<char *>(param);
        break;
      case CURLOPT_CUSTOMREQUEST:
        custom_request_ = reinterpret_cast<char *>(param);
        break;
      case CURLOPT_HTTPHEADER:
        headers_ = reinterpret_cast<std::vector<std::string> *>(param);
        break;
      case CURLOPT_ERRORBUFFER:
        error_buffer_ = reinterpret_cast<char *>(param);
        break;
      case CURLOPT_CAINFO:
        ca_info_ = reinterpret_cast<char *>(param);
        break;
      case CURLOPT_WRITEDATA:
        write_data_ = reinterpret_cast<FILE *>(param);
        break;
      case CURLOPT_HEADERDATA:
        header_data_ = reinterpret_cast<FILE *>(param);
        break;
      case CURLOPT_READDATA:
        read_data_ = reinterpret_cast<FILE *>(param);
        break;
      case CURLOPT_XFERINFODATA:
        progress_data_ = param;
        break;
      default:
        break;
    }
    return CURLE_OK;
  }
  CURLcode curl_easy_setopt(CURL *curl, CURLoption option,
                            size_t (*param)(void *, size_t, size_t,
                                            FILE *)) override {
    read_callback_ = param;
    return CURLE_OK;
  }

  CURLcode curl_easy_setopt(CURL *curl, CURLoption option,
                            size_t (*param)(const void *, size_t, size_t,
                                            void *)) override {
    switch (option) {
      case CURLOPT_WRITEFUNCTION:
        write_callback_ = param;
        break;
      case CURLOPT_HEADERFUNCTION:
        header_callback_ = param;
        break;
      default:
        break;
    }
    return CURLE_OK;
  }

  CURLcode curl_easy_setopt(CURL *curl, CURLoption option,
                            int (*param)(void *clientp, curl_off_t dltotal,
                                         curl_off_t dlnow, curl_off_t ultotal,
                                         curl_off_t ulnow)) override {
    progress_callback_ = param;
    return CURLE_OK;
  }
  CURLcode curl_easy_perform(CURL *curl) override {
    if (is_post_ || is_put_) {
      char buffer[3];
      int bytes_read;
      posted_content_ = "";
      do {
        bytes_read = read_callback_(buffer, 1, sizeof(buffer), read_data_);
        posted_content_ = absl::StrCat(posted_content_,
                                       absl::string_view(buffer, bytes_read));
      } while (bytes_read > 0);
    }
    if (write_data_ || write_callback_) {
      size_t bytes_handled = write_callback_(
          response_content_.c_str(), 1, response_content_.size(), write_data_);
      if (bytes_handled != response_content_.size()) {
        curl_easy_perform_result_ = CURLE_WRITE_ERROR;
      }
    }
    for (const auto &header : response_headers_) {
      header_callback_(header.c_str(), 1, header.size(), header_data_);
    }
    if (error_buffer_) {
      strncpy(error_buffer_, curl_easy_perform_error_message_.c_str(),
              curl_easy_perform_error_message_.size() + 1);
    }
    return curl_easy_perform_result_;
  }

  CURLcode curl_easy_getinfo(CURL *curl, CURLINFO info,
                             uint64_t *value) override {
    switch (info) {
      case CURLINFO_RESPONSE_CODE:
        *value = response_code_;
        break;
      default:
        break;
    }
    return CURLE_OK;
  }

  CURLcode curl_easy_getinfo(CURL *curl, CURLINFO info,
                             double *value) override {
    switch (info) {
      case CURLINFO_SIZE_DOWNLOAD:
        *value = response_content_.size();
        break;
      default:
        break;
    }
    return CURLE_OK;
  }

  void curl_easy_cleanup(CURL *curl) override { is_cleaned_up_ = true; }
  void curl_free(void *p) override { free(p); }

  std::string response_content_;
  uint64_t response_code_;
  std::vector<std::string> response_headers_;

  // Internal variables to store the libcurl state.
  std::string url_;
  std::string range_;
  std::string custom_request_;
  std::string ca_info_;
  char *error_buffer_ = nullptr;
  bool is_initialized_ = false;
  bool is_cleaned_up_ = false;
  std::vector<std::string> *headers_ = nullptr;
  bool is_post_ = false;
  bool is_put_ = false;
  void *write_data_ = nullptr;
  size_t (*write_callback_)(const void *ptr, size_t size, size_t nmemb,
                            void *userdata) = nullptr;
  void *header_data_ = nullptr;
  size_t (*header_callback_)(const void *ptr, size_t size, size_t nmemb,
                             void *userdata) = nullptr;
  FILE *read_data_ = nullptr;
  size_t (*read_callback_)(void *ptr, size_t size, size_t nmemb,
                           FILE *userdata) = &fread;
  int (*progress_callback_)(void *clientp, curl_off_t dltotal, curl_off_t dlnow,
                            curl_off_t ultotal, curl_off_t ulnow) = nullptr;
  void *progress_data_ = nullptr;
  // Outcome of performing the request.
  std::string posted_content_;
  CURLcode curl_easy_perform_result_ = CURLE_OK;
  std::string curl_easy_perform_error_message_;
};

TEST(CurlHttpClient, TestDefaultConfig) {
  CurlHttpClient client(LibCurlProxy::CreateInstance(), GetSimpleCredential());
  const auto config = client.GetConfig();
  EXPECT_FALSE(config.raw);
  EXPECT_FALSE(config.verbose);
  EXPECT_EQ(config.verbose_cb, nullptr);
  EXPECT_EQ(config.proxy, "");
  EXPECT_EQ(config.request_timeout, 5);
  EXPECT_EQ(config.connect_timeout, 5);
  EXPECT_EQ(config.dns_timeout, 60);
  EXPECT_FALSE(config.follow_redirect);
  EXPECT_EQ(config.max_recv_speed, -1);
}

class CurlHttpClientTest : public ::testing::Test {
 protected:
  CurlHttpClientTest()
      : server_("barebones_session_auth/mockup.shar",
                absl::StrCat(GetTestTempUdsDirectory(), "/mockup.socket")),
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

}  // namespace
}  // namespace ecclesia
