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
#include <string>
#include <utility>
#include <variant>

#include "absl/functional/bind_front.h"
#include "absl/memory/memory.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "ecclesia/lib/http/client.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/status/macros.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {
namespace {

// Use a dummy endpoint so that the URI stays valid, as required by cURL:
// https://github.com/curl/curl/issues/936
constexpr absl::string_view kUdsDummyEndpoint = "dummy";

// Generic helper function for all REST operations.
// RestOp is a functor which invokes the relevant REST operation.
template <typename RestOp>
absl::StatusOr<RedfishTransport::Result> RestHelper(
    std::unique_ptr<HttpClient::HttpRequest> request, RestOp rest_func) {
  ECCLESIA_ASSIGN_OR_RETURN(HttpClient::HttpResponse resp,
                            rest_func(std::move(request)));
  RedfishTransport::Result result;
  result.code = resp.code;
  result.body = resp.GetBodyJson();
  return result;
}

}  // namespace

std::unique_ptr<HttpClient::HttpRequest> HttpRedfishTransport::MakeRequest(
    TcpTarget target, absl::string_view path, absl::string_view data) {
  auto request = absl::make_unique<HttpClient::HttpRequest>();
  request->uri = absl::StrCat(target.endpoint, path);
  request->body = data;
  return request;
}

std::unique_ptr<HttpClient::HttpRequest> HttpRedfishTransport::MakeRequest(
    UdsTarget target, absl::string_view path, absl::string_view data) {
  auto request = absl::make_unique<HttpClient::HttpRequest>();
  request->uri = absl::StrCat(kUdsDummyEndpoint, path);
  request->body = data;
  request->unix_socket_path = target.path;
  return request;
}

HttpRedfishTransport::HttpRedfishTransport(
    std::unique_ptr<HttpClient> client,
    std::variant<TcpTarget, UdsTarget> target)
    : client_(std::move(client)), target_(std::move(target)) {}

std::unique_ptr<RedfishTransport> HttpRedfishTransport::MakeNetwork(
    std::unique_ptr<HttpClient> client, std::string endpoint) {
  return absl::WrapUnique(new HttpRedfishTransport(
      std::move(client), TcpTarget{std::move(endpoint)}));
}

std::unique_ptr<RedfishTransport> HttpRedfishTransport::MakeUds(
    std::unique_ptr<HttpClient> client, std::string unix_domain_socket) {
  return absl::WrapUnique(new HttpRedfishTransport(
      std::move(client), UdsTarget{std::string(unix_domain_socket)}));
}

void HttpRedfishTransport::UpdateToNetworkEndpoint(
    absl::string_view tcp_endpoint) {
  absl::WriterMutexLock mu(&mutex_);
  target_ = TcpTarget{std::string(tcp_endpoint)};
}

void HttpRedfishTransport::UpdateToUdsEndpoint(
    absl::string_view unix_domain_socket) {
  absl::WriterMutexLock mu(&mutex_);
  target_ = UdsTarget{std::string(unix_domain_socket)};
}

absl::StatusOr<RedfishTransport::Result> HttpRedfishTransport::Get(
    absl::string_view path) {
  absl::ReaderMutexLock mu(&mutex_);
  return RestHelper(
      std::visit([&](const auto &t) { return MakeRequest(t, path, ""); },
                 target_),
      absl::bind_front(&HttpClient::Get, client_.get()));
}

absl::StatusOr<RedfishTransport::Result> HttpRedfishTransport::Post(
    absl::string_view path, absl::string_view data) {
  absl::ReaderMutexLock mu(&mutex_);
  return RestHelper(
      std::visit([&](const auto &t) { return MakeRequest(t, path, data); },
                 target_),
      absl::bind_front(&HttpClient::Post, client_.get()));
}

absl::StatusOr<RedfishTransport::Result> HttpRedfishTransport::Patch(
    absl::string_view path, absl::string_view data) {
  absl::ReaderMutexLock mu(&mutex_);
  return RestHelper(
      std::visit([&](const auto &t) { return MakeRequest(t, path, data); },
                 target_),
      absl::bind_front(&HttpClient::Patch, client_.get()));
}

absl::StatusOr<RedfishTransport::Result> HttpRedfishTransport::Delete(
    absl::string_view path, absl::string_view data) {
  absl::ReaderMutexLock mu(&mutex_);
  return RestHelper(
      std::visit([&](const auto &t) { return MakeRequest(t, path, data); },
                 target_),
      absl::bind_front(&HttpClient::Delete, client_.get()));
}

}  // namespace ecclesia
