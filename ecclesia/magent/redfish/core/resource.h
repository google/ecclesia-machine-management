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

#ifndef ECCLESIA_MAGENT_REDFISH_CORE_RESOURCE_H_
#define ECCLESIA_MAGENT_REDFISH_CORE_RESOURCE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "json/value.h"
#include "tensorflow_serving/util/net_http/server/public/httpserver_interface.h"
#include "tensorflow_serving/util/net_http/server/public/response_code_enum.h"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"

namespace ecclesia {

// Abstract base class to represent a Redfish resource.
class Resource {
 public:
  explicit Resource(std::string uri) : uri_(std::move(uri)) {}
  Resource(const Resource &) = delete;
  Resource &operator=(const Resource &) = delete;

  virtual ~Resource() {}

  // Register a request handler to route requests corresponding to uri_.
  // The default implementation does not support regex.
  // To register a handler with regex, override this method and register a
  // custom request dispatcher instead.
  virtual void RegisterRequestHandler(
      tensorflow::serving::net_http::HTTPServerInterface *server) {
    tensorflow::serving::net_http::RequestHandlerOptions handler_options;
    server->RegisterRequestHandler(
        this->Uri(),
        [this](tensorflow::serving::net_http::ServerRequestInterface *req) {
          return this->RequestHandler(req);
        },
        handler_options);
  }

 protected:
  using ParamsType = std::vector<absl::variant<int, std::string>>;
  // Generates a response for Http GET request
  // "params" is to allow a regex dispatcher to pass capture values to the
  // method.
  virtual void Get(tensorflow::serving::net_http::ServerRequestInterface *req,
                   const ParamsType &params) = 0;

  // Generates a response for HTTP POST response
  virtual void Post(tensorflow::serving::net_http::ServerRequestInterface *req,
                    const ParamsType &params) {
    req->ReplyWithStatus(
        tensorflow::serving::net_http::HTTPStatusCode::METHOD_NA);
  }

  virtual void RequestHandler(
      tensorflow::serving::net_http::ServerRequestInterface *req) {
    if (req->http_method() == "GET") {
      Get(req, ParamsType());
    } else if (req->http_method() == "POST") {
      Post(req, ParamsType());
    } else {
      req->ReplyWithStatus(
          tensorflow::serving::net_http::HTTPStatusCode::METHOD_NA);
    }
  }

  // Get the URI corresponding to the resource
  // This can be a regex when the resource is a member within a collection
  const absl::string_view Uri() const { return uri_; }

 private:
  const std::string uri_;
};

// Factory function to create a resource given the construction arguments.
// It is recommended to use this function to create an instance of a resource.
template <typename ResourceType, typename... Args>
std::unique_ptr<Resource> CreateResource(
    tensorflow::serving::net_http::HTTPServerInterface *server,
    Args &&...args) {
  auto resource = absl::make_unique<ResourceType>(std::forward<Args>(args)...);
  resource->RegisterRequestHandler(server);
  return std::move(resource);
}

// Generate a response with the input json object and set http status OK.
inline void JSONResponseOK(
    const Json::Value &json,
    tensorflow::serving::net_http::ServerRequestInterface *req) {
  tensorflow::serving::net_http::SetContentType(req, "application/json");
  req->WriteResponseString(json.toStyledString());
  req->ReplyWithStatus(tensorflow::serving::net_http::HTTPStatusCode::OK);
}

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_REDFISH_CORE_RESOURCE_H_
