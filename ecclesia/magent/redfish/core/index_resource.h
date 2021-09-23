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

#ifndef ECCLESIA_MAGENT_REDFISH_CORE_INDEX_RESOURCE_H_
#define ECCLESIA_MAGENT_REDFISH_CORE_INDEX_RESOURCE_H_

#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "ecclesia/lib/strings/regex.h"
#include "ecclesia/lib/types/overloaded.h"
#include "ecclesia/magent/redfish/core/resource.h"
#include "tensorflow_serving/util/net_http/server/public/httpserver_interface.h"
#include "tensorflow_serving/util/net_http/server/public/response_code_enum.h"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"
#include "re2/re2.h"

namespace ecclesia {
// When a redfish resource is a part of a collection, and the uri contains one
// or more indices to the resource, prefer to derive from this class.
// Unlike the generic resource, the URI of this IndexResource is normally a
// regex pattern, e.g., "/redfish/v1/Systems/(\\w+)/Memory/(\\d+)"
// The template takes a list of param types(int|std::string).
// There should be one-to-one correspondence between the capture group and
// the param type.
template <typename... ArgTypes>
class IndexResource : public Resource {
 public:
  explicit IndexResource(const std::string &uri_pattern)
      : Resource(uri_pattern), uri_regex_(this->Uri()) {}

  virtual ~IndexResource() {}

  void RegisterRequestHandler(
      tensorflow::serving::net_http::HTTPServerInterface *server) override {
    server->RegisterRequestDispatcher(
        [this](
            tensorflow::serving::net_http::ServerRequestInterface *http_request)
            -> tensorflow::serving::net_http::RequestHandler {
          if (RE2::FullMatch(http_request->uri_path(), uri_regex_)) {
            return [this](tensorflow::serving::net_http::ServerRequestInterface
                              *req) { return this->RequestHandler(req); };
          } else {
            return nullptr;
          }
        },
        tensorflow::serving::net_http::RequestHandlerOptions());
  }

 protected:
  // Helper method to validate the resource index from the request URI
  // To be called from the Get/Post methods
  bool ValidateResourceIndex(const absl::variant<int, std::string> &param,
                             int num_resources) {
    return absl::visit(
        Overloaded{[num_resources](int index) {
                     return (index >= 0 && index < num_resources);
                   },
                   [](std::string index) { return true; }},
        param);
  }

 private:
  void RequestHandler(
      tensorflow::serving::net_http::ServerRequestInterface *req) override {
    // The URI of this IndexResource is normally a regex pattern. Here we get
    // all the resource index from the request URI and store in a tuple.
    // Then cast the tuple to a vector, the extracted indices will be passed as
    // parameter into the corresponding resource handler.
    auto maybe_indices_tuple =
        RegexFullMatch<ArgTypes...>(req->uri_path(), uri_regex_);

    if (!maybe_indices_tuple.has_value()) {
      req->ReplyWithStatus(
          tensorflow::serving::net_http::HTTPStatusCode::NOT_FOUND);
      return;
    }

    std::vector<absl::variant<int, std::string>> indices;
    std::apply(
        [&indices](auto &&...elements) {
          (indices.push_back(std::forward<decltype(elements)>(elements)), ...);
        },
        *maybe_indices_tuple);

    HandleRequestWithindices(req, indices);
  }

  void HandleRequestWithindices(
      tensorflow::serving::net_http::ServerRequestInterface *req,
      const std::vector<absl::variant<int, std::string>> &indices) {
    // Pass along the index to the Get() / Post() handlers
    if (req->http_method() == "GET") {
      Get(req, indices);
    } else if (req->http_method() == "POST") {
      Post(req, indices);
    } else {
      req->ReplyWithStatus(
          tensorflow::serving::net_http::HTTPStatusCode::METHOD_NA);
    }
  }

  // Regular expression for matching this IndexResource's URI.
  RE2 uri_regex_;
};

}  // namespace ecclesia
#endif  // ECCLESIA_MAGENT_REDFISH_CORE_INDEX_RESOURCE_H_
