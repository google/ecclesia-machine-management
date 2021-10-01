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

#include "absl/functional/bind_front.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/http/client.h"
#include "ecclesia/lib/status/macros.h"

namespace ecclesia {
namespace {

// Generic helper function for all REST operations.
// RestOp is a functor which invokes the relevant REST operation.
template <typename RestOp>
absl::StatusOr<RedfishTransport::Result> RestHelper(absl::string_view uri,
                                                    absl::string_view data,
                                                    RestOp rest_func) {
  auto request = absl::make_unique<HttpClient::HttpRequest>();
  request->uri = uri;
  request->body = data;
  ECCLESIA_ASSIGN_OR_RETURN(HttpClient::HttpResponse resp,
                            rest_func(std::move(request)));
  RedfishTransport::Result result;
  result.code = resp.code;
  result.body = resp.GetBodyJson();
  return result;
}

}  // namespace

HttpRedfishTransport::HttpRedfishTransport(std::unique_ptr<HttpClient> client,
                                           std::string endpoint)
    : client_(std::move(client)), endpoint_(endpoint) {}

absl::StatusOr<RedfishTransport::Result> HttpRedfishTransport::Get(
    absl::string_view path) {
  return RestHelper(absl::StrCat(endpoint_, path), "",
                    absl::bind_front(&HttpClient::Get, client_.get()));
}

absl::StatusOr<RedfishTransport::Result> HttpRedfishTransport::Post(
    absl::string_view path, absl::string_view data) {
  return RestHelper(absl::StrCat(endpoint_, path), data,
                    absl::bind_front(&HttpClient::Post, client_.get()));
}

absl::StatusOr<RedfishTransport::Result> HttpRedfishTransport::Patch(
    absl::string_view path, absl::string_view data) {
  return RestHelper(absl::StrCat(endpoint_, path), data,
                    absl::bind_front(&HttpClient::Patch, client_.get()));
}

absl::StatusOr<RedfishTransport::Result> HttpRedfishTransport::Delete(
    absl::string_view path, absl::string_view data) {
  return RestHelper(absl::StrCat(endpoint_, path), data,
                    absl::bind_front(&HttpClient::Delete, client_.get()));
}

}  // namespace ecclesia
