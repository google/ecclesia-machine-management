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

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/meta/type_traits.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ecclesia/lib/http/client.h"
#include "ecclesia/lib/http/cred.pb.h"
#include "ecclesia/lib/http/curl_client.h"
#include "ecclesia/lib/http/server.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/test_mockup.h"
#include "ecclesia/lib/redfish/transport/cache.h"
#include "ecclesia/lib/redfish/transport/http.h"
#include "ecclesia/lib/redfish/transport/http_redfish_intf.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "single_include/nlohmann/json.hpp"
#include "tensorflow_serving/util/net_http/public/response_code_enum.h"
#include "tensorflow_serving/util/net_http/server/public/httpserver_interface.h"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"

namespace ecclesia {
namespace {

// Binding to port 0 will bind to any available port.
constexpr int kDefaultPort = 0;
constexpr int kNumThreads = 5;

// Create HTTP Transport
std::unique_ptr<RedfishTransport> MakeHttpRedfishTransport(
    ::tensorflow::serving::net_http::HTTPServerInterface *proxy_server) {
  std::string endpoint =
      absl::StrCat("http://localhost:", proxy_server->listen_port());

  HttpCredential creds;
  auto curl_http_client = std::make_unique<CurlHttpClient>(
      LibCurlProxy::CreateInstance(), std::move(creds));
  auto transport =
      HttpRedfishTransport::MakeNetwork(std::move(curl_http_client), endpoint);
  return transport;
}

std::unique_ptr<RedfishInterface> MakeHttpRedfishInterface(
    std::unique_ptr<RedfishTransport> transport) {
  auto cache = std::make_unique<NullCache>(transport.get());
  auto intf = NewHttpInterface(std::move(transport), std::move(cache),
                               RedfishInterface::kTrusted);
  CHECK(intf != nullptr) << "can connect to the redfish proxy server";
  return intf;
}

}  // namespace

void FakeRedfishServer::ClearHandlers() {
  absl::MutexLock mu(&patch_lock_);
  http_get_handlers_.clear();
  http_patch_handlers_.clear();
  http_post_handlers_.clear();
  http_delete_handlers_.clear();
}

void FakeRedfishServer::AddHttpGetHandlerWithData(std::string uri,
                                                  absl::Span<const char> data) {
  AddHttpGetHandler(
      std::move(uri),
      [&, data](::tensorflow::serving::net_http::ServerRequestInterface *req) {
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        req->WriteResponseBytes(data.data(), data.length());
        req->Reply();
      });
}

void FakeRedfishServer::AddHttpGetHandlerWithOwnedData(std::string uri,
                                                       std::string data) {
  AddHttpGetHandler(
      std::move(uri),
      [&, data = std::move(data)](
          ::tensorflow::serving::net_http::ServerRequestInterface *req) {
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        req->WriteResponseString(data);
        req->Reply();
      });
}

void FakeRedfishServer::AddHttpGetHandlerWithStatus(
    std::string uri, absl::Span<const char> data,
    tensorflow::serving::net_http::HTTPStatusCode status) {
  AddHttpGetHandler(
      std::move(uri),
      [&, data,
       status](::tensorflow::serving::net_http::ServerRequestInterface *req) {
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        req->WriteResponseBytes(data.data(),
                                static_cast<int64_t>(data.length()));
        req->ReplyWithStatus(status);
      });
}

void FakeRedfishServer::HandleHttpGet(
    ::tensorflow::serving::net_http::ServerRequestInterface *req) {
  absl::MutexLock mu(&patch_lock_);
  auto itr = http_get_handlers_.find(req->uri_path());
  if (itr == http_get_handlers_.end()) {
    ::tensorflow::serving::net_http::SetContentType(req, "application/json");
    req->OverwriteResponseHeader("OData-Version", "4.0");
    auto response = redfish_intf_->UncachedGetUri(req->uri_path());
    req->WriteResponseString(response.DebugString());
    if (response.httpcode().has_value()) {
      req->ReplyWithStatus(::tensorflow::serving::net_http::HTTPStatusCode(
          response.httpcode().value()));
    } else {
      req->Reply();
    }
    return;
  }
  itr->second(req);
}

void FakeRedfishServer::HandleHttpPatch(
    ::tensorflow::serving::net_http::ServerRequestInterface *req) {
  absl::MutexLock mu(&patch_lock_);
  auto itr = http_patch_handlers_.find(req->uri_path());
  if (itr == http_patch_handlers_.end()) {
    LOG(FATAL) << "Unhandled PATCH to URI: " << req->uri_path();
    return;
  }
  itr->second(req);
}

void FakeRedfishServer::HandleHttpPost(
    ::tensorflow::serving::net_http::ServerRequestInterface *req) {
  absl::MutexLock mu(&patch_lock_);
  auto itr = http_post_handlers_.find(req->uri_path());
  if (itr == http_post_handlers_.end()) {
    LOG(FATAL) << "Unhandled POST to URI: " << req->uri_path();
    return;
  }
  itr->second(req);
}

void FakeRedfishServer::HandleHttpDelete(
    ::tensorflow::serving::net_http::ServerRequestInterface *req) {
  absl::MutexLock mu(&patch_lock_);
  auto itr = http_delete_handlers_.find(req->uri_path());
  if (itr == http_delete_handlers_.end()) {
    LOG(FATAL) << "Unhandled DELETE to URI: " << req->uri_path();
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

void FakeRedfishServer::AddHttpPostHandlerWithData(absl::string_view uri,
                                                   absl::string_view data,
                                                   HandlerFunc handler) {
  absl::MutexLock mu(&patch_lock_);
  http_post_handlers_[uri] = std::move(handler);
}

void FakeRedfishServer::AddHttpDeleteHandler(std::string uri,
                                             HandlerFunc handler) {
  absl::MutexLock mu(&patch_lock_);
  http_delete_handlers_[uri] = std::move(handler);
}

std::unique_ptr<RedfishInterface> FakeRedfishServer::RedfishClientInterface() {
  std::unique_ptr<RedfishTransport> transport =
      MakeHttpRedfishTransport(proxy_server_.get());
  return MakeHttpRedfishInterface(std::move(transport));
}

std::unique_ptr<RedfishTransport> FakeRedfishServer::RedfishClientTransport() {
  return MakeHttpRedfishTransport(proxy_server_.get());
}

FakeRedfishServer::FakeRedfishServer(absl::string_view mockup_shar,
                                     absl::string_view mockup_uds_path)
    : proxy_server_(ecclesia::CreateServer(kDefaultPort, kNumThreads)),
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
        } else if (req->http_method() == "DELETE") {
          this->HandleHttpDelete(req);
        }
      };
  proxy_server_->RegisterRequestDispatcher(
      [handler = std::move(handler)](
          ::tensorflow::serving::net_http::ServerRequestInterface *req)
          -> ::tensorflow::serving::net_http::RequestHandler {
        return handler;
      },
      ::tensorflow::serving::net_http::RequestHandlerOptions());
  CHECK(proxy_server_->StartAcceptingRequests())
      << "can start the proxy server";
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
  return FakeRedfishServer::Config{.hostname = "localhost",
                                   .port = proxy_server_->listen_port()};
}

void FakeRedfishServer::EnableExpandGetHandler(ExpandQuery expand_query) {
  RedfishVariant redfish_var = redfish_intf_->UncachedGetUri("/redfish/v1");
  nlohmann::json json_res;
  std::unique_ptr<RedfishObject> obj_res = redfish_var.AsObject();
  if (obj_res != nullptr) {
    json_res = obj_res->GetContentAsJson();
  }
  nlohmann::json root_response_for_expand;
  root_response_for_expand["ExpandQuery"]["ExpandALL"] = expand_query.ExpandAll;
  root_response_for_expand["ExpandQuery"]["Levels"] = expand_query.Levels;
  root_response_for_expand["ExpandQuery"]["Links"] = expand_query.Links;
  root_response_for_expand["ExpandQuery"]["MaxLevels"] = expand_query.MaxLevels;
  root_response_for_expand["ExpandQuery"]["NoLinks"] = expand_query.NoLinks;
  json_res["ProtocolFeaturesSupported"] = root_response_for_expand;
  std::string result = json_res.dump();
  AddHttpGetHandler(
      "/redfish/v1",
      [&, result](tensorflow::serving::net_http::ServerRequestInterface *req) {
        SetContentType(req, "application/json");
        req->OverwriteResponseHeader("OData-Version", "4.0");
        req->WriteResponseString(result);
        req->Reply();
      });
}

}  // namespace ecclesia
