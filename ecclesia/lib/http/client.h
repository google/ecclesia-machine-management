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

#ifndef ECCLESIA_LIB_HTTP_CLIENT_H_
#define ECCLESIA_LIB_HTTP_CLIENT_H_

#include <memory>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {

// The supported Http request methods
enum class Protocol {
  kGet,
  kPost,
  kDelete,
  kPatch,
};

// Return the HTTP method name for protocol.
std::string GetHttpMethodName(ecclesia::Protocol protocol);

// Http Client interface
class HttpClient {
 public:
  using HttpHeaders = absl::flat_hash_map<std::string, std::string>;

  enum class Resolver { kIPAny = 0, kIPv4Only, kIPv6Only };

  // Get/Post requests
  struct HttpRequest {
    // The full URI, i.e., "http://host/redfish/v1", not "/redfish/v1".
    std::string uri;

    // Unix domain socket path, if applicable. Empty if not used.
    std::string unix_socket_path = "";

    // Only used by Post requests.
    std::string body;

    HttpHeaders headers;
  };

  // Http response that contains http status code, header and body
  struct HttpResponse {
    nlohmann::json GetBodyJson() const;

    int code = 0;
    std::string body;
    HttpHeaders headers;
  };

  class IncrementalResponseHandler {
   public:
    virtual ~IncrementalResponseHandler() = default;
    virtual absl::Status OnResponseHeaders(const HttpResponse& response) = 0;
    virtual absl::Status OnBodyData(absl::string_view data) = 0;
    virtual bool IsCancelled() const { return false; }
  };

  HttpClient() {}
  virtual ~HttpClient() {}

  // Following HTTP method functions are stateless.

  // Execute a GET request and return HttpResponse or absl::Status.
  virtual absl::StatusOr<HttpResponse> Get(
      std::unique_ptr<HttpRequest> request) = 0;
  // Sub class is responsible for generating the post string_view.
  virtual absl::StatusOr<HttpResponse> Post(
      std::unique_ptr<HttpRequest> request) = 0;
  // Execute a DELETE request and return HttpResponse or absl::Status.
  virtual absl::StatusOr<HttpResponse> Delete(
      std::unique_ptr<HttpRequest> request) {
    return absl::UnimplementedError("Delete not implemented");
  }
  // Execute a PATCH request and return HttpResponse or absl::Status.
  virtual absl::StatusOr<HttpResponse> Patch(
      std::unique_ptr<HttpRequest> request) {
    return absl::UnimplementedError("Patch not implemented");
  }

  // These methods pass the response code and headers, followed the response
  // body in chunks to a handler. This is useful for long running connections
  // such as SSE.
  virtual absl::Status GetIncremental(std::unique_ptr<HttpRequest> request,
                                      IncrementalResponseHandler* handler) {
    return absl::UnimplementedError("GetIncremental not implemented");
  }
  virtual absl::Status PostIncremental(std::unique_ptr<HttpRequest> request,
                                       IncrementalResponseHandler* handler) {
    return absl::UnimplementedError("PostIncremental not implemented");
  }
  virtual absl::Status DeleteIncremental(std::unique_ptr<HttpRequest> request,
                                         IncrementalResponseHandler* handler) {
    return absl::UnimplementedError("DeleteIncremental not implemented");
  }
  virtual absl::Status PatchIncremental(std::unique_ptr<HttpRequest> request,
                                        IncrementalResponseHandler* handler) {
    return absl::UnimplementedError("PatchIncremental not implemented");
  }
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_HTTP_CLIENT_H_
