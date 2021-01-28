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

#include "ecclesia/magent/redfish/indus/service_root.h"

#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
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

using tensorflow::serving::net_http::EventExecutor;
using tensorflow::serving::net_http::HTTPServerInterface;
using tensorflow::serving::net_http::ServerOptions;

constexpr absl::string_view kFileName =
    "magent/redfish/indus/test_data/service_root.json";

void ReadJsonFromFile(const std::string &filename, Json::Value *value) {
  std::string file_contents;
  std::string line;
  std::ifstream file(filename);
  ASSERT_TRUE(file.is_open());
  while (getline(file, line)) {
    file_contents.append(line);
  }
  file.close();
  Json::Reader reader;
  ASSERT_TRUE(reader.parse(file_contents, *value));
}

class RequestExecutor : public EventExecutor {
 public:
  explicit RequestExecutor(int num_threads) : thread_pool_(num_threads) {}
  void Schedule(std::function<void()> fn) override {
    thread_pool_.Schedule(fn);
  }

 private:
  ThreadPool thread_pool_;
};

class ServiceRootTest : public ::testing::Test {
 public:
  ServiceRootTest() {
    InitServer();
    service_root_ = absl::make_unique<ServiceRoot>();
    service_root_->RegisterRequestHandler(server_.get());
  }

  ~ServiceRootTest() {
    if (!server_->is_terminating()) {
      server_->Terminate();
      server_->WaitForTermination();
    }
  }

 protected:
  std::unique_ptr<HTTPServerInterface> server_;
  std::unique_ptr<ServiceRoot> service_root_;
  int port_;

 private:
  void InitServer() {
    port_ = FindUnusedPortOrDie();
    auto options = absl::make_unique<ServerOptions>();
    options->AddPort(port_);
    options->SetExecutor(absl::make_unique<RequestExecutor>(5));
    server_ = CreateEvHTTPServer(std::move(options));
  }
};

TEST_F(ServiceRootTest, QueryServiceRoot) {
  // Read the expected json object
  Json::Value expected;
  ReadJsonFromFile(GetTestDataDependencyPath(kFileName), &expected);

  ASSERT_TRUE(server_->StartAcceptingRequests());
  auto redfish_intf =
      libredfish::NewRawInterface(absl::StrCat("localhost:", port_),
                                  libredfish::RedfishInterface::kTrusted);

  // Perform an http get request on the service root resource.
  libredfish::RedfishVariant response = redfish_intf->GetUri(kServiceRootUri);

  // Parse the raw contents and compare it to the expected service root.
  Json::Reader reader;
  Json::Value actual;
  ASSERT_TRUE(reader.parse(response.DebugString(), actual));
  EXPECT_EQ(expected, actual);
}

TEST_F(ServiceRootTest, QueryServiceRootEquivalentUri) {
  // Read the expected json object
  Json::Value expected;
  ReadJsonFromFile(GetTestDataDependencyPath(kFileName), &expected);

  ASSERT_TRUE(server_->StartAcceptingRequests());
  auto redfish_intf =
      libredfish::NewRawInterface(absl::StrCat("localhost:", port_),
                                  libredfish::RedfishInterface::kTrusted);

  // Perform an http get request on the service root resource without the
  // trailing forward slash and expect the URI to be treated as equivalent.
  libredfish::RedfishVariant response =
      redfish_intf->GetUri(absl::StripSuffix(kServiceRootUri, "/"));

  // Parse the raw contents and compare it to the expected service root.
  Json::Reader reader;
  Json::Value actual;
  ASSERT_TRUE(reader.parse(response.DebugString(), actual));
  EXPECT_EQ(expected, actual);
}

}  // namespace
}  // namespace ecclesia
