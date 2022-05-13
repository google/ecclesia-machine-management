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

#include "ecclesia/magent/redfish/common/service_root.h"

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
#include "ecclesia/lib/http/client.h"
#include "ecclesia/lib/http/cred.pb.h"
#include "ecclesia/lib/http/curl_client.h"
#include "ecclesia/lib/network/testing.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/transport/cache.h"
#include "ecclesia/lib/redfish/transport/http.h"
#include "ecclesia/lib/redfish/transport/http_redfish_intf.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/magent/lib/thread_pool/thread_pool.h"
#include "ecclesia/magent/redfish/core/redfish_keywords.h"
#include "single_include/nlohmann/json.hpp"
#include "tensorflow_serving/util/net_http/server/public/httpserver.h"
#include "tensorflow_serving/util/net_http/server/public/httpserver_interface.h"

namespace ecclesia {
namespace {

using ::tensorflow::serving::net_http::EventExecutor;
using ::tensorflow::serving::net_http::HTTPServerInterface;
using ::tensorflow::serving::net_http::ServerOptions;

constexpr absl::string_view kFileName =
    "magent/redfish/common/test_data/service_root.json";

void ReadJsonFromFile(const std::string &filename, nlohmann::json *value) {
  std::string file_contents;
  std::string line;
  std::ifstream file(filename);
  ASSERT_TRUE(file.is_open());
  while (getline(file, line)) {
    file_contents.append(line);
  }
  file.close();

  *value = nlohmann::json::parse(file_contents, nullptr, false);
  ASSERT_FALSE(value->is_discarded());
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
    service_root_ = std::make_unique<ServiceRoot>();
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
    auto options = std::make_unique<ServerOptions>();
    options->AddPort(port_);
    options->SetExecutor(std::make_unique<RequestExecutor>(5));
    server_ = CreateEvHTTPServer(std::move(options));
  }
};

TEST_F(ServiceRootTest, QueryServiceRoot) {
  // Read the expected json object
  nlohmann::json expected;
  ReadJsonFromFile(GetTestDataDependencyPath(kFileName), &expected);

  ASSERT_TRUE(server_->StartAcceptingRequests());
  auto curl_http_client = std::make_unique<CurlHttpClient>(
      LibCurlProxy::CreateInstance(), HttpCredential());
  auto transport = HttpRedfishTransport::MakeNetwork(
      std::move(curl_http_client), absl::StrCat("localhost:", port_));
  auto cache = std::make_unique<NullCache>(transport.get());
  auto redfish_intf = NewHttpInterface(std::move(transport), std::move(cache),
                                       RedfishInterface::kTrusted);

  // Perform an http get request on the service root resource.
  RedfishVariant response = redfish_intf->UncachedGetUri(kServiceRootUri);

  // Parse the raw contents and compare it to the expected service root.
  nlohmann::json actual =
      nlohmann::json::parse(response.DebugString(), nullptr, false);
  ASSERT_FALSE(actual.is_discarded());
  EXPECT_EQ(expected, actual);
}

TEST_F(ServiceRootTest, QueryServiceRootEquivalentUri) {
  // Read the expected json object
  nlohmann::json expected;
  ReadJsonFromFile(GetTestDataDependencyPath(kFileName), &expected);

  ASSERT_TRUE(server_->StartAcceptingRequests());
  auto curl_http_client = std::make_unique<CurlHttpClient>(
      LibCurlProxy::CreateInstance(), HttpCredential());
  auto transport = HttpRedfishTransport::MakeNetwork(
      std::move(curl_http_client), absl::StrCat("localhost:", port_));
  auto cache = std::make_unique<NullCache>(transport.get());
  auto redfish_intf = NewHttpInterface(std::move(transport), std::move(cache),
                                       RedfishInterface::kTrusted);

  // Perform an http get request on the service root resource without the
  // trailing forward slash and expect the URI to be treated as equivalent.
  RedfishVariant response =
      redfish_intf->UncachedGetUri(absl::StripSuffix(kServiceRootUri, "/"));

  // Parse the raw contents and compare it to the expected service root.
  nlohmann::json actual =
      nlohmann::json::parse(response.DebugString(), nullptr, false);
  ASSERT_FALSE(actual.is_discarded());
  EXPECT_EQ(expected, actual);
}

}  // namespace
}  // namespace ecclesia
