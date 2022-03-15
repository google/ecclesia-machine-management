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

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/functional/bind_front.h"
#include "absl/memory/memory.h"
#include "absl/meta/type_traits.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "ecclesia/lib/http/client.h"
#include "ecclesia/lib/redfish/property_definitions.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/status/macros.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {
namespace {

// Use a dummy endpoint so that the URI stays valid, as required by cURL:
// https://github.com/curl/curl/issues/936
constexpr absl::string_view kUdsDummyEndpoint = "dummy";

// Headers used for session authentication, as defined in the Redfish spec.
constexpr absl::string_view kXAuthToken = "X-Auth-Token";
constexpr absl::string_view kLocation = "Location";

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
  result.headers = std::move(resp.headers);
  return result;
}

// Helper function for retrieving the POST target to the SessionService.
// The Redfish Spec suggests 2 options:
// 1. via the @odata.id reference in Links.Sessions
// 2. via SessionService.Sessions
absl::StatusOr<std::string> GetSessionServicePostTarget(nlohmann::json root) {
  // Try reading Links.Session
  auto links_itr = root.find(kRfPropertyLinks);
  if (links_itr != root.end()) {
    auto session_itr = links_itr->find(kRfPropertySessions);
    if (session_itr != links_itr->end()) {
      auto odata_id = session_itr->find(PropertyOdataId::Name);
      if (odata_id != session_itr->end() && odata_id->is_string()) {
        return odata_id->get<std::string>();
      }
    }
  }

  // Try reading SessionService Collection.
  auto service_itr = root.find(kRfPropertySessionService);
  if (service_itr != root.end()) {
    auto odata_id = service_itr->find(PropertyOdataId::Name);
    if (odata_id != service_itr->end() && odata_id->is_string()) {
      return absl::StrCat(odata_id->get<std::string>(), "/",
                          kRfPropertySessions);
    }
  }

  return absl::NotFoundError("No Session URI target found.");
}

}  // namespace

HttpRedfishTransport::~HttpRedfishTransport() {
  absl::MutexLock mu(&mutex_);
  EndCurrentSession();
}

void HttpRedfishTransport::EndCurrentSession() {
  if (!session_auth_uri_.empty()) {
    LockedDelete(session_auth_uri_, "").IgnoreError();
  }
  session_auth_uri_.clear();
  x_auth_token_.clear();
}

absl::Status HttpRedfishTransport::DoSessionAuth(std::string username,
                                                 std::string password) {
  absl::MutexLock mu(&mutex_);
  EndCurrentSession();
  session_username_ = std::move(username);
  session_password_ = std::move(password);
  return LockedDoSessionAuth();
}

absl::Status HttpRedfishTransport::LockedDoSessionAuth() {
  if (session_username_.empty() && session_password_.empty()) {
    return absl::OkStatus();
  }
  ECCLESIA_ASSIGN_OR_RETURN(Result root, LockedGet(GetRootUri()));
  ECCLESIA_ASSIGN_OR_RETURN(std::string post_uri,
                            GetSessionServicePostTarget(root.body));

  nlohmann::json session_post_payload;
  session_post_payload[PropertyUserName::Name] = session_username_;
  session_post_payload[PropertyPassword::Name] = session_password_;
  absl::StatusOr<Result> session_post =
      LockedPost(post_uri, session_post_payload.dump());
  if (!session_post.ok()) {
    return absl::InternalError(absl::StrCat("Could not establish session: ",
                                            session_post.status().message()));
  }
  auto token = session_post->headers.find(kXAuthToken);
  if (token == session_post->headers.end()) {
    return absl::InternalError("No X-Auth-Token returned for POST.");
  }
  auto session_uri = session_post->headers.find(kLocation);
  if (session_uri == session_post->headers.end()) {
    return absl::InternalError("No session URI returned for POST.");
  }
  session_auth_uri_ = std::move(session_uri->second);
  x_auth_token_ = std::move(token->second);

  return absl::OkStatus();
}

std::unique_ptr<HttpClient::HttpRequest> HttpRedfishTransport::MakeRequest(
    TcpTarget target, absl::string_view path, absl::string_view data) {
  auto request = absl::make_unique<HttpClient::HttpRequest>();
  request->uri = absl::StrCat(target.endpoint, path);
  request->body = std::string(data);
  if (!x_auth_token_.empty()) {
    request->headers[kXAuthToken] = x_auth_token_;
  }
  return request;
}

std::unique_ptr<HttpClient::HttpRequest> HttpRedfishTransport::MakeRequest(
    UdsTarget target, absl::string_view path, absl::string_view data) {
  auto request = absl::make_unique<HttpClient::HttpRequest>();
  request->uri = absl::StrCat(kUdsDummyEndpoint, path);
  request->body = std::string(data);
  request->unix_socket_path = target.path;
  if (!x_auth_token_.empty()) {
    request->headers[kXAuthToken] = x_auth_token_;
  }
  return request;
}

HttpRedfishTransport::HttpRedfishTransport(
    std::unique_ptr<HttpClient> client,
    std::variant<TcpTarget, UdsTarget> target, ServiceRootUri service_root)
    : client_(std::move(client)),
      target_(std::move(target)),
      service_root_(service_root) {}

std::unique_ptr<HttpRedfishTransport> HttpRedfishTransport::MakeNetwork(
    std::unique_ptr<HttpClient> client, std::string endpoint,
    ServiceRootUri service_root) {
  return absl::WrapUnique(new HttpRedfishTransport(
      std::move(client), TcpTarget{std::move(endpoint)}, service_root));
}

std::unique_ptr<HttpRedfishTransport> HttpRedfishTransport::MakeUds(
    std::unique_ptr<HttpClient> client, std::string unix_domain_socket,
    ServiceRootUri service_root) {
  return absl::WrapUnique(new HttpRedfishTransport(
      std::move(client), UdsTarget{std::string(unix_domain_socket)},
      service_root));
}

absl::string_view HttpRedfishTransport::GetRootUri() {
  return RedfishInterface::ServiceRootToUri(service_root_);
}

absl::StatusOr<RedfishTransport::Result> HttpRedfishTransport::Get(
    absl::string_view path) {
  absl::MutexLock mu(&mutex_);
  return LockedGet(path);
}

absl::StatusOr<RedfishTransport::Result> HttpRedfishTransport::LockedGet(
    absl::string_view path) {
  return RestHelper(std::visit([&](const auto &t) ABSL_SHARED_LOCKS_REQUIRED(
                                   mutex_) { return MakeRequest(t, path, ""); },
                               target_),
                    absl::bind_front(&HttpClient::Get, client_.get()));
}

absl::StatusOr<RedfishTransport::Result> HttpRedfishTransport::Post(
    absl::string_view path, absl::string_view data) {
  absl::MutexLock mu(&mutex_);
  return LockedPost(path, data);
}

absl::StatusOr<RedfishTransport::Result> HttpRedfishTransport::LockedPost(
    absl::string_view path, absl::string_view data) {
  return RestHelper(
      std::visit([&](const auto &t) ABSL_SHARED_LOCKS_REQUIRED(
                     mutex_) { return MakeRequest(t, path, data); },
                 target_),
      absl::bind_front(&HttpClient::Post, client_.get()));
}

absl::StatusOr<RedfishTransport::Result> HttpRedfishTransport::Patch(
    absl::string_view path, absl::string_view data) {
  absl::MutexLock mu(&mutex_);
  return LockedPatch(path, data);
}

absl::StatusOr<RedfishTransport::Result> HttpRedfishTransport::LockedPatch(
    absl::string_view path, absl::string_view data) {
  return RestHelper(
      std::visit([&](const auto &t) ABSL_SHARED_LOCKS_REQUIRED(
                     mutex_) { return MakeRequest(t, path, data); },
                 target_),
      absl::bind_front(&HttpClient::Patch, client_.get()));
}

absl::StatusOr<RedfishTransport::Result> HttpRedfishTransport::Delete(
    absl::string_view path, absl::string_view data) {
  absl::MutexLock mu(&mutex_);
  return LockedDelete(path, data);
}

absl::StatusOr<RedfishTransport::Result> HttpRedfishTransport::LockedDelete(
    absl::string_view path, absl::string_view data) {
  return RestHelper(
      std::visit([&](const auto &t) ABSL_SHARED_LOCKS_REQUIRED(
                     mutex_) { return MakeRequest(t, path, data); },
                 target_),
      absl::bind_front(&HttpClient::Delete, client_.get()));
}

}  // namespace ecclesia
