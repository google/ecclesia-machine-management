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

#include "ecclesia/magent/redfish/common/session_service.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/apifs/apifs.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/network/testing.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/raw.h"
#include "ecclesia/magent/lib/thread_pool/thread_pool.h"
#include "ecclesia/magent/redfish/core/redfish_keywords.h"
#include "json/reader.h"
#include "json/value.h"
#include "tensorflow_serving/util/net_http/client/public/httpclient.h"
#include "tensorflow_serving/util/net_http/client/public/httpclient_interface.h"
#include "tensorflow_serving/util/net_http/server/public/httpserver.h"
#include "tensorflow_serving/util/net_http/server/public/httpserver_interface.h"
#include "tensorflow_serving/util/net_http/server/public/response_code_enum.h"

namespace ecclesia {
namespace {

constexpr absl::string_view kFileName =
    "magent/redfish/common/test_data/session_service.json";

void ReadJsonFromFile(const std::string &filename, Json::Value *value) {
  std::string expected;
  ApifsFile apifs_file((std::string(GetTestDataDependencyPath(kFileName))));
  absl::StatusOr<std::string> maybe_contents = apifs_file.Read();
  ASSERT_TRUE(maybe_contents.ok());
  expected = *maybe_contents;

  Json::Reader reader;
  ASSERT_TRUE(reader.parse(expected, *value));
}

class RequestExecutor : public tensorflow::serving::net_http::EventExecutor {
 public:
  explicit RequestExecutor(int num_threads) : thread_pool_(num_threads) {}
  void Schedule(std::function<void()> fn) override {
    thread_pool_.Schedule(fn);
  }

 private:
  ThreadPool thread_pool_;
};

class SessionServiceTest : public ::testing::Test {
 public:
  SessionServiceTest() {
    InitServer();
    session_service_.RegisterRequestHandler(server_.get());
  }

  ~SessionServiceTest() {
    if (!server_->is_terminating()) {
      server_->Terminate();
      server_->WaitForTermination();
    }
  }

 protected:
  std::unique_ptr<tensorflow::serving::net_http::HTTPServerInterface> server_;
  SessionService session_service_;
  int port_;

 private:
  void InitServer() {
    port_ = FindUnusedPortOrDie();
    auto options =
        absl::make_unique<tensorflow::serving::net_http::ServerOptions>();
    options->AddPort(port_);
    options->SetExecutor(absl::make_unique<RequestExecutor>(4));
    server_ =
        tensorflow::serving::net_http::CreateEvHTTPServer(std::move(options));
  }
};

// Compare the resource properties to the expected properties
TEST_F(SessionServiceTest, QuerySessionService) {
  // Read the expected json object
  Json::Value expected;
  ReadJsonFromFile(GetTestDataDependencyPath(kFileName), &expected);

  ASSERT_TRUE(server_->StartAcceptingRequests());

  auto redfish_intf =
      libredfish::NewRawInterface(absl::StrCat("localhost:", port_),
                                  libredfish::RedfishInterface::kTrusted);

  // Perform an http get request on the session service resource.
  libredfish::RedfishVariant response =
      redfish_intf->GetUri(kSessionServiceUri);

  // Parse the raw contents and compare it to the expected session service.
  Json::Reader reader;
  Json::Value actual;
  ASSERT_TRUE(reader.parse(response.DebugString(), actual));
  EXPECT_EQ(expected, actual);
}

// Check that POST requests are unauthorized
TEST_F(SessionServiceTest, PostToSessionServiceUnauthorized) {
  ASSERT_TRUE(server_->StartAcceptingRequests());

  // Exercise the RequestHandler
  std::unique_ptr<tensorflow::serving::net_http::HTTPClientInterface>
      connection = tensorflow::serving::net_http::CreateEvHTTPConnection(
          "localhost", port_);

  ASSERT_TRUE(connection != nullptr);

  tensorflow::serving::net_http::ClientRequest request = {
      kSessionServiceUri, "POST", {}, ""};
  tensorflow::serving::net_http::ClientResponse response = {};

  EXPECT_TRUE(connection->BlockingSendRequest(request, &response));
  EXPECT_EQ(response.status,
            tensorflow::serving::net_http::HTTPStatusCode::UNAUTHORIZED);
}

}  // namespace
}  // namespace ecclesia
