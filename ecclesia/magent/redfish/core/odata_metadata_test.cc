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

#include "ecclesia/magent/redfish/core/odata_metadata.h"

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
#include "ecclesia/magent/lib/thread_pool/thread_pool.h"
#include "ecclesia/magent/redfish/core/redfish_keywords.h"
#include "tensorflow_serving/util/net_http/client/public/httpclient.h"
#include "tensorflow_serving/util/net_http/client/public/httpclient_interface.h"
#include "tensorflow_serving/util/net_http/server/public/httpserver.h"
#include "tensorflow_serving/util/net_http/server/public/httpserver_interface.h"
#include "tensorflow_serving/util/net_http/server/public/response_code_enum.h"

namespace ecclesia {
namespace {

constexpr absl::string_view kFileName = "magent/redfish/metadata/index.xml";

class RequestExecutor : public tensorflow::serving::net_http::EventExecutor {
 public:
  explicit RequestExecutor(int num_threads) : thread_pool_(num_threads) {}
  void Schedule(std::function<void()> fn) override {
    thread_pool_.Schedule(fn);
  }

 private:
  ThreadPool thread_pool_;
};

class ODataMetadataTest : public ::testing::Test {
 public:
  ODataMetadataTest() {
    InitServer();
    odata_metadata_ =
        absl::make_unique<ODataMetadata>(GetTestDataDependencyPath(kFileName));
    odata_metadata_->RegisterRequestHandler(server_.get());
  }

  ~ODataMetadataTest() {
    if (!server_->is_terminating()) {
      server_->Terminate();
      server_->WaitForTermination();
    }
  }

 protected:
  std::unique_ptr<tensorflow::serving::net_http::HTTPServerInterface> server_;
  std::unique_ptr<ODataMetadata> odata_metadata_;
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

TEST_F(ODataMetadataTest, QueryODataMetadata) {
  // Read the expected XML object
  absl::string_view expected;
  ApifsFile apifs_file((std::string(GetTestDataDependencyPath(kFileName))));
  absl::StatusOr<std::string> maybe_contents = apifs_file.Read();
  ASSERT_TRUE(maybe_contents.ok());
  expected = *maybe_contents;

  ASSERT_TRUE(server_->StartAcceptingRequests());

  EXPECT_EQ(expected, odata_metadata_->GetMetadata());

  // Exercise the RequestHandler and compare to the expected metadata
  std::unique_ptr<tensorflow::serving::net_http::HTTPClientInterface>
      connection = tensorflow::serving::net_http::CreateEvHTTPConnection(
          "localhost", port_);

  ASSERT_TRUE(connection != nullptr);

  tensorflow::serving::net_http::ClientRequest request = {
      kODataMetadataUri, "GET", {}, ""};
  tensorflow::serving::net_http::ClientResponse response = {};

  EXPECT_TRUE(connection->BlockingSendRequest(request, &response));
  EXPECT_EQ(response.status, tensorflow::serving::net_http::HTTPStatusCode::OK);

  absl::string_view actual = response.body;
  EXPECT_EQ(expected, actual);
}

}  // namespace
}  // namespace ecclesia
