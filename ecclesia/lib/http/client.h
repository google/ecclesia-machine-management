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

#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "json/value.h"

namespace ecclesia {

// The supported Http request methods
enum class Protocol {
  kError,
  kGet,
  kPost,
  kHead,
  kPut,
  kDelete,
  kCopy,
  kMove,
  kLock,
  kUnlock,
  kTrace,
  kOptions,
  kConnect
};

// A struct we use to store protocol with its Method string.
struct ProtocolInfo {
  constexpr ProtocolInfo(Protocol p_enum, const char p_name[])
      : protocol(p_enum), name(p_name) {}
  Protocol protocol;
  absl::string_view name;
};

// Protocols we currently support
static constexpr ProtocolInfo kAllProtocols[] = {
    {Protocol::kGet, "GET"},
    {Protocol::kPost, "POST"},
};

// Http Client interface
class HttpClient {
 public:
  using HttpHeaders = absl::flat_hash_map<std::string, std::string>;

  enum class Resolver { kIPAny = 0, kIPv4Only, kIPv6Only };

  // Http response that contains http status code, header and body
  struct HttpResponse {
    // The unique_ptr returned outlives the HttpResponse.
    absl::StatusOr<Json::Value> GetBodyJson();

    int code = 0;
    std::string body;
    std::vector<std::string> headers;
  };

  HttpClient() {}
  virtual ~HttpClient() {}

  // Following Get and Post functions are stateless.

  // Execute a GET request and return HttpResponse or absl::Status.
  virtual absl::StatusOr<HttpResponse> Get(absl::string_view uri) = 0;
  // Sub class is responsible for generating the post string_view.
  virtual absl::StatusOr<HttpResponse> Post(absl::string_view uri,
                                            absl::string_view post) = 0;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_HTTP_CLIENT_H_
