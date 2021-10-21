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

#ifndef ECCLESIA_LIB_REDFISH_TRANSPORT_INTERFACE_H_
#define ECCLESIA_LIB_REDFISH_TRANSPORT_INTERFACE_H_

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {

// RedfishTransport defines a data-layer-protocol agnostic interface for the
// raw RESTful operations to a Redfish Service.
class RedfishTransport {
 public:
  // Result contains a successful REST response.
  struct Result {
    // HTTP code.
    int code = 0;
    // If the response body was JSON format, it will be parsed here.
    nlohmann::json body = nlohmann::json::value_t::discarded;
    // Headers returned in the response.
    absl::flat_hash_map<std::string, std::string> headers;
  };

  virtual ~RedfishTransport() = default;

  // Updates the endpoint for this transport implementation to a network addr.
  virtual void UpdateToNetworkEndpoint(absl::string_view endpoint) = 0;
  // Updates the endpoint for this transport implementation to a unix socket.
  virtual void UpdateToUdsEndpoint(absl::string_view unix_domain_socket) = 0;

  // REST operations.
  // These return a Status if the operation failed to be sent/received.
  // The application-level success or failure is captured in Result.code.
  virtual absl::StatusOr<Result> Get(absl::string_view path) = 0;
  virtual absl::StatusOr<Result> Post(absl::string_view path,
                                      absl::string_view data) = 0;
  virtual absl::StatusOr<Result> Patch(absl::string_view path,
                                       absl::string_view data) = 0;
  virtual absl::StatusOr<Result> Delete(absl::string_view path,
                                        absl::string_view data) = 0;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_TRANSPORT_INTERFACE_H_
