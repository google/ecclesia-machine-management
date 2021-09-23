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

#include "ecclesia/lib/redfish/testing/fake_redfish_server.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/meta/type_traits.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/raw.h"
#include "ecclesia/lib/redfish/test_mockup.h"
#include "ecclesia/magent/daemons/common.h"
#include "tensorflow_serving/util/net_http/server/public/httpserver_interface.h"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"

namespace ecclesia {

void FakeRedfishServer::ClearHandlers() {
  absl::MutexLock mu(&patch_lock_);
  http_get_handlers_.clear();
  http_patch_handlers_.clear();
  http_post_handlers_.clear();
}

void FakeRedfishServer::AddHttpGetHandlerWithData(std::string uri,
                                                  absl::Span<const char> data) {
  AddHttpGetHandler(
      uri,
      [&, data](::tensorflow::serving::net_http::ServerRequestInterface *req) {
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->WriteResponseBytes(data.data(), data.length());
        req->Reply();
      });
}

void FakeRedfishServer::HandleHttpGet(
    ::tensorflow::serving::net_http::ServerRequestInterface *req) {
  absl::MutexLock mu(&patch_lock_);
  auto itr = http_get_handlers_.find(req->uri_path());
  if (itr == http_get_handlers_.end()) {
    ::tensorflow::serving::net_http::SetContentType(req, "application/json");
    req->WriteResponseString(
        redfish_intf_->GetUri(req->uri_path()).DebugString());
    req->Reply();
    return;
  }
  itr->second(req);
}

void FakeRedfishServer::HandleHttpPatch(
    ::tensorflow::serving::net_http::ServerRequestInterface *req) {
  absl::MutexLock mu(&patch_lock_);
  auto itr = http_patch_handlers_.find(req->uri_path());
  if (itr == http_patch_handlers_.end()) {
    FatalLog() << "Unhandled PATCH to URI: " << req->uri_path();
    return;
  }
  itr->second(req);
}

void FakeRedfishServer::HandleHttpPost(
    ::tensorflow::serving::net_http::ServerRequestInterface *req) {
  absl::MutexLock mu(&patch_lock_);
  auto itr = http_post_handlers_.find(req->uri_path());
  if (itr == http_post_handlers_.end()) {
    FatalLog() << "Unhandled POST to URI: " << req->uri_path();
    return;
  }
  itr->second(req);
}

void FakeRedfishServer::AddHttpGetHandler(std::string uri,
                                          HandlerFunc handler) {
  absl::MutexLock mu(&patch_lock_);
  http_get_handlers_[uri] = std::move(handler);
}

void FakeRedfishServer::AddHttpPatchHandler(std::string uri,
                                            HandlerFunc handler) {
  absl::MutexLock mu(&patch_lock_);
  http_patch_handlers_[uri] = std::move(handler);
}

void FakeRedfishServer::AddHttpPostHandler(std::string uri,
                                           HandlerFunc handler) {
  absl::MutexLock mu(&patch_lock_);
  http_post_handlers_[uri] = std::move(handler);
}

std::unique_ptr<libredfish::RedfishInterface>
FakeRedfishServer::RedfishClientInterface() {
  std::string endpoint =
      absl::StrCat("http://localhost:", proxy_server_->listen_port());
  auto intf = libredfish::NewRawInterface(
      endpoint, libredfish::RedfishInterface::kTrusted);
  ecclesia::Check(intf != nullptr, "can connect to the redfish proxy server");
  return intf;
}

FakeRedfishServer::FakeRedfishServer(absl::string_view mockup_shar,
                                     absl::string_view mockup_uds_path)
    : proxy_server_(ecclesia::CreateServer(0)),
      mockup_server_(mockup_shar, mockup_uds_path),
      redfish_intf_(mockup_server_.RedfishClientInterface()) {
  auto handler =
      [this](::tensorflow::serving::net_http::ServerRequestInterface *req) {
        if (req->http_method() == "GET") {
          this->HandleHttpGet(req);
        } else if (req->http_method() == "POST") {
          this->HandleHttpPost(req);
        } else if (req->http_method() == "PATCH") {
          this->HandleHttpPatch(req);
        }
      };
  proxy_server_->RegisterRequestDispatcher(
      [handler = std::move(handler)](
          ::tensorflow::serving::net_http::ServerRequestInterface *req)
          -> ::tensorflow::serving::net_http::RequestHandler {
        return handler;
      },
      ::tensorflow::serving::net_http::RequestHandlerOptions());
  ecclesia::Check(proxy_server_->StartAcceptingRequests(),
                  "can start the proxy server");
}

FakeRedfishServer::~FakeRedfishServer() {
  if (proxy_server_) {
    proxy_server_->Terminate();
    // As this server is used for testing, just arbitrarily choose 10 seconds
    // to be the upper limit for termination.
    proxy_server_->WaitForTerminationWithTimeout(absl::Seconds(10));
  }
}

FakeRedfishServer::Config FakeRedfishServer::GetConfig() const {
  return FakeRedfishServer::Config{.hostname = "[::1]",
                                   .port = proxy_server_->listen_port()};
}

}  // namespace ecclesia
