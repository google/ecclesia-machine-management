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

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {

// RedfishEventStream defines a data-layer-protocol agnostic interface for the
// server-side event streaming.
// An implementation shall be thread-safe.
class RedfishEventStream {
 public:
  // Starts the Redfish streaming until any unexpected errors happen or being
  // cancelled.
  // This function is nonblocking. Callbacks are executed in a separate thread.
  virtual void StartStreaming() = 0;
  // Cancels and stops the Redfish streaming. Does nothing if the stream is
  // already stopped.
  // NOTE: When a stream is stopped, it can never be started again. Create a new
  // stream instead.
  virtual void CancelStreaming() = 0;

  virtual ~RedfishEventStream() = default;
};

// RedfishTransport defines a data-layer-protocol agnostic interface for the
// raw RESTful operations to a Redfish Service.
class RedfishTransport {
 public:
  using bytes = std::vector<uint8_t>;
  // Result contains a successful REST response.
  struct Result {
    // HTTP code.
    int code = 0;
    // If the result is in the Redfish tree, it will be parsed as JSON.
    // Otherwise, it will be treated as bytes.
    std::variant<nlohmann::json, bytes> body =
        nlohmann::json::value_t::discarded;
    // Headers returned in the response.
    absl::flat_hash_map<std::string, std::string> headers;
  };

  using EventCallback = std::function<void(const Result&)>;
  using StopCallback = std::function<void(const absl::Status&)>;

  virtual ~RedfishTransport() = default;

  // Fetches the root uri and returns it.
  virtual absl::string_view GetRootUri() = 0;

  // REST operations.
  // These return a Status if the operation failed to be sent/received.
  // The application-level success or failure is captured in Result.code.
  virtual absl::StatusOr<Result> Get(absl::string_view path) = 0;
  virtual absl::StatusOr<Result> Get(absl::string_view path,
                                     absl::Duration timeout) = 0;
  virtual absl::StatusOr<Result> Post(absl::string_view path,
                                      absl::string_view data) = 0;
  // If octet_stream is true, the data will be sent as octet_stream.
  virtual absl::StatusOr<Result> Post(
      absl::string_view path, absl::string_view data, bool octet_stream,
      absl::Duration timeout,
      absl::Span<const std::pair<std::string, std::string>> headers) {
    return absl::UnimplementedError(
        "Post with timeout and octet_stream is not implemented yet for this "
        "transport.");
  }
  virtual absl::StatusOr<Result> Patch(absl::string_view path,
                                       absl::string_view data) = 0;
  virtual absl::StatusOr<Result> Delete(absl::string_view path,
                                        absl::string_view data) = 0;

  // A Redfish eventing implemented by a server side stream RPC.
  // 1. |data| represents the subscription configuration. TODO(nanzhou): link
  // to schema
  // 2. |on_event| will be invoked every time an event (modelled as |Result|) is
  // sent from the server. A callback must never block.
  // 3. |on_stop| will be invoked when the stream stops.
  // Returns a Redfish event stream on success where clients can start or stop
  // the streaming.
  virtual absl::StatusOr<std::unique_ptr<RedfishEventStream>> Subscribe(
      absl::string_view data, EventCallback&& on_event,
      StopCallback&& on_stop) {
    return absl::UnimplementedError("Streaming is not implemented yet.");
  }
};

// NullTransport provides a placeholder implementation which gracefully fails
// all of its methods.
class NullTransport : public RedfishTransport {
  absl::string_view GetRootUri() override { return ""; }
  absl::StatusOr<Result> Get(absl::string_view path) override {
    return absl::InternalError("NullTransport");
  }
  absl::StatusOr<Result> Get(absl::string_view path,
                             absl::Duration timeout) override {
    return absl::InternalError("NullTransport");
  }
  absl::StatusOr<Result> Post(absl::string_view path,
                              absl::string_view data) override {
    return absl::InternalError("NullTransport");
  }
  absl::StatusOr<Result> Patch(absl::string_view path,
                               absl::string_view data) override {
    return absl::InternalError("NullTransport");
  }
  absl::StatusOr<Result> Delete(absl::string_view path,
                                absl::string_view data) override {
    return absl::InternalError("NullTransport");
  }
  absl::StatusOr<std::unique_ptr<RedfishEventStream>> Subscribe(
      absl::string_view data, EventCallback&& callback,
      StopCallback&& on_stop) override {
    return absl::InternalError("NullTransport");
  }
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_TRANSPORT_INTERFACE_H_
