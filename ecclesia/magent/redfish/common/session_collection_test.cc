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

#include "ecclesia/magent/redfish/common/session_collection.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/apifs/apifs.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/network/testing.h"
#include "ecclesia/lib/redfish/raw.h"
#include "ecclesia/magent/lib/thread_pool/thread_pool.h"
#include "ecclesia/magent/redfish/core/redfish_keywords.h"
#include "single_include/nlohmann/json.hpp"
#include "tensorflow_serving/util/net_http/client/public/httpclient.h"
#include "tensorflow_serving/util/net_http/client/public/httpclient_interface.h"
#include "tensorflow_serving/util/net_http/server/public/httpserver.h"
#include "tensorflow_serving/util/net_http/server/public/httpserver_interface.h"
#include "tensorflow_serving/util/net_http/server/public/response_code_enum.h"

namespace ecclesia {
namespace {

constexpr absl::string_view kFileName =
    "magent/redfish/common/test_data/session_collection.json";

void ReadJsonFromFile(const std::string &filename, nlohmann::json *value) {
  ApifsFile apifs_file((std::string(GetTestDataDependencyPath(kFileName))));
  absl::StatusOr<std::string> maybe_contents = apifs_file.Read();
  ASSERT_TRUE(maybe_contents.ok());
  std::string expected = *maybe_contents;

  *value = nlohmann::json::parse(expected, nullptr, false);
  ASSERT_FALSE(value->is_discarded());
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

class SessionCollectionTest : public ::testing::Test {
 public:
  SessionCollectionTest() {
    InitServer();
    session_collection_.RegisterRequestHandler(server_.get());
  }

  ~SessionCollectionTest() {
    if (!server_->is_terminating()) {
      server_->Terminate();
      server_->WaitForTermination();
    }
  }

 protected:
  std::unique_ptr<tensorflow::serving::net_http::HTTPServerInterface> server_;
  SessionCollection session_collection_;
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

// Compare the resource properties to the expected properties.
TEST_F(SessionCollectionTest, QuerySessionCollection) {
  // Read the expected json object
  nlohmann::json expected;
  ReadJsonFromFile(GetTestDataDependencyPath(kFileName), &expected);

  ASSERT_TRUE(server_->StartAcceptingRequests());

  auto redfish_intf =
      libredfish::NewRawInterface(absl::StrCat("localhost:", port_),
                                  libredfish::RedfishInterface::kTrusted);

  // Perform an http get request on the session collection resource.
  libredfish::RedfishVariant response = redfish_intf->GetUri(kSessionsUri);

  // Parse the raw contents and compare it to the expected session collection.
  nlohmann::json actual = nlohmann::json::parse(response.DebugString(), nullptr,
                                                false);
  ASSERT_FALSE(actual.is_discarded());
  EXPECT_EQ(expected, actual);
}

// Check that POST requests are unauthorized.
TEST_F(SessionCollectionTest, PostToSessionCollectionUnauthorized) {
  ASSERT_TRUE(server_->StartAcceptingRequests());

  // Exercise the RequestHandler.
  std::unique_ptr<tensorflow::serving::net_http::HTTPClientInterface>
      connection = tensorflow::serving::net_http::CreateEvHTTPConnection(
          "localhost", port_);

  ASSERT_TRUE(connection != nullptr);

  tensorflow::serving::net_http::ClientRequest request = {
      kSessionsUri, "POST", {}, ""};
  tensorflow::serving::net_http::ClientResponse response = {};

  EXPECT_TRUE(connection->BlockingSendRequest(request, &response));
  EXPECT_EQ(response.status,
            tensorflow::serving::net_http::HTTPStatusCode::UNAUTHORIZED);
}

}  // namespace
}  // namespace ecclesia
