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

#ifndef ECCLESIA_LIB_REDFISH_TESTING_FAKE_REDFISH_SERVER_H_
#define ECCLESIA_LIB_REDFISH_TESTING_FAKE_REDFISH_SERVER_H_

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/test_mockup.h"
#include "tensorflow_serving/util/net_http/server/public/httpserver_interface.h"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"

namespace ecclesia {

// FakeRedfishServer runs a proxy HTTP server in front of a Redfish Mockup.
// By default, the proxy HTTP server will pass through the HTTP responses from
// the mockup. However, the FakeRedfishServer can also be configured with
// patches so that specific URIs return arbitrary results.
//
//  |--------------------------------------------------|
//  | RedfishInterface (from RedfishClientInterface()) |
//  |--------------------------------------------------|
//                          | HTTP
//                          V
//  |--------------------------------------------------|
//  |                   ProxyServer                    |
//  |--------------------------------------------------|
//            |                           | local member variable
//            |  HTTP                     V
//            |                    |-------------|
//            |                    | URI Patches |
//            V                    |-------------|
//  |---------------------|
//  | TestingMockupServer |
//  |---------------------|
//
// Note that the open source Python RedfishMockupServer supports HTTP PATCH
// by default where the Mockup can be modified by clients. The advantage of this
// FakeRedfishServer is specifically for fuzz testing, where we can inject
// arbitrary bytes into the proxy server's HTTP responses. We want the
// FakeRedfishServer to return the fuzzing input directly for the greatest
// amount of flexibility. Trying to inject fuzz data via HTTP PATCH to the
// MockupServer inadvertently will be an exercise in fuzzing the
// RedfishMockupServer's HTTP PATCH interface.
class FakeRedfishServer {
 public:
  FakeRedfishServer(absl::string_view mockup_shar,
                    absl::string_view mockup_uds_path);
  ~FakeRedfishServer();

  using HandlerFunc = std::function<void(
      ::tensorflow::serving::net_http::ServerRequestInterface *req)>;

  // Returns a new RedfishInterface connected to the proxy server.
  std::unique_ptr<libredfish::RedfishInterface> RedfishClientInterface();

  // Clear all registered handlers.
  void ClearHandlers() ABSL_LOCKS_EXCLUDED(patch_lock_);

  // Register a handler to respond to a given HTTP method and URI request.
  void AddHttpGetHandler(std::string uri, HandlerFunc handler)
      ABSL_LOCKS_EXCLUDED(patch_lock_);
  void AddHttpPatchHandler(std::string uri, HandlerFunc handler)
      ABSL_LOCKS_EXCLUDED(patch_lock_);
  void AddHttpPostHandler(std::string uri, HandlerFunc handler)
      ABSL_LOCKS_EXCLUDED(patch_lock_);
  void AddHttpDeleteHandler(std::string uri, HandlerFunc handler)
      ABSL_LOCKS_EXCLUDED(patch_lock_);

  // Convenience function to register a GET handler that returns the provided
  // data.
  void AddHttpGetHandlerWithData(std::string uri, absl::Span<const char> data)
      ABSL_LOCKS_EXCLUDED(patch_lock_);

  struct Config {
    std::string hostname;
    int port;
  };
  Config GetConfig() const;

 private:
  // Store of all patches
  absl::Mutex patch_lock_;
  absl::flat_hash_map<std::string, HandlerFunc> http_get_handlers_
      ABSL_GUARDED_BY(patch_lock_);
  absl::flat_hash_map<std::string, HandlerFunc> http_patch_handlers_
      ABSL_GUARDED_BY(patch_lock_);
  absl::flat_hash_map<std::string, HandlerFunc> http_post_handlers_
      ABSL_GUARDED_BY(patch_lock_);
  absl::flat_hash_map<std::string, HandlerFunc> http_delete_handlers_
      ABSL_GUARDED_BY(patch_lock_);
  // Helper for fetching any registered patches for a given URI.
  void HandleHttpGet(
      ::tensorflow::serving::net_http::ServerRequestInterface *req)
      ABSL_LOCKS_EXCLUDED(patch_lock_);
  void HandleHttpPatch(
      ::tensorflow::serving::net_http::ServerRequestInterface *req)
      ABSL_LOCKS_EXCLUDED(patch_lock_);
  void HandleHttpPost(
      ::tensorflow::serving::net_http::ServerRequestInterface *req)
      ABSL_LOCKS_EXCLUDED(patch_lock_);
  void HandleHttpDelete(
      ::tensorflow::serving::net_http::ServerRequestInterface *req)
      ABSL_LOCKS_EXCLUDED(patch_lock_);
  // The connection configuration of this mockup.
  std::string proxy_uds_path_;
  // The proxy server.
  std::unique_ptr<::tensorflow::serving::net_http::HTTPServerInterface>
      proxy_server_;
  // The mockup server
  libredfish::TestingMockupServer mockup_server_;
  // Interface to the mockup server, used by the proxy server
  std::unique_ptr<libredfish::RedfishInterface> redfish_intf_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_TESTING_FAKE_REDFISH_SERVER_H_
