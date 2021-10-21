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

#ifndef ECCLESIA_LIB_REDFISH_TRANSPORT_HTTP_H_
#define ECCLESIA_LIB_REDFISH_TRANSPORT_HTTP_H_

#include <memory>
#include <string>
#include <variant>

#include "absl/base/thread_annotations.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "ecclesia/lib/http/client.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {

// HttpRedfishTransport implements RedfishTransport with an HttpClient.
class HttpRedfishTransport : public RedfishTransport {
 public:
  // Creates an HttpRedfishTransport using a network endpoint.
  // Params:
  //   client: HttpClient instance
  //   tcp_endpoint: e.g. "localhost:80", "https://10.0.0.1", "[::1]:8000"
  static std::unique_ptr<HttpRedfishTransport> MakeNetwork(
      std::unique_ptr<HttpClient> client, std::string tcp_endpoint);

  // Creates an HttpRedfishTransport using a unix domain socket endpoint.
  // Params:
  //   client: HttpClient instance
  //   unix_domain_socket: e.g. "/var/run/my.socket"
  static std::unique_ptr<HttpRedfishTransport> MakeUds(
      std::unique_ptr<HttpClient> client, std::string unix_domain_socket);

  // Performs the Redfish Session Login Authorization procedure, as documented
  // in the Redfish Spec (DSP0266 Redfish Specification v1.14.0 Section 13.3.4:
  // Redfish session login authentication).
  // This method is declared only in HttpRedfishTransport and not the general
  // RedfishTransport as the mechanism requires sending X-Auth-Tokens in HTTP
  // headers and therefore is not generalizable to all transport types.
  absl::Status DoSessionAuth(std::string username, std::string password)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Destructor needs to close any open sessions if applicable.
  ~HttpRedfishTransport() ABSL_LOCKS_EXCLUDED(mutex_) override;

  // Updates the current HttpRedfishTransport instance to a new endpoint.
  // It is valid to switch from a TCP endpoint to a UDS endpoint and vice-versa.
  // If Session Auth credentials were provided, the same credentials will be
  // used for establishing a new session on the new endpoint. An existing
  // connection will first be logged off (via HTTP DELETE).
  void UpdateToNetworkEndpoint(absl::string_view tcp_endpoint)
      ABSL_LOCKS_EXCLUDED(mutex_) override;
  void UpdateToUdsEndpoint(absl::string_view unix_domain_socket)
      ABSL_LOCKS_EXCLUDED(mutex_) override;

  absl::StatusOr<Result> Get(absl::string_view path)
      ABSL_LOCKS_EXCLUDED(mutex_) override;
  absl::StatusOr<Result> Post(absl::string_view path, absl::string_view data)
      ABSL_LOCKS_EXCLUDED(mutex_) override;
  absl::StatusOr<Result> Patch(absl::string_view path, absl::string_view data)
      ABSL_LOCKS_EXCLUDED(mutex_) override;
  absl::StatusOr<Result> Delete(absl::string_view path, absl::string_view data)
      ABSL_LOCKS_EXCLUDED(mutex_) override;

 private:
  // Simple struct wrappers to define a TCP endpoint or a UDS endpoint.
  struct TcpTarget {
    std::string endpoint;
  };
  struct UdsTarget {
    std::string path;
  };

  // Internal REST methods to be called while holding the mutex.
  absl::StatusOr<Result> LockedGet(absl::string_view path)
      ABSL_SHARED_LOCKS_REQUIRED(mutex_);
  absl::StatusOr<Result> LockedPost(absl::string_view path,
                                    absl::string_view data)
      ABSL_SHARED_LOCKS_REQUIRED(mutex_);
  absl::StatusOr<Result> LockedPatch(absl::string_view path,
                                     absl::string_view data)
      ABSL_SHARED_LOCKS_REQUIRED(mutex_);
  absl::StatusOr<Result> LockedDelete(absl::string_view path,
                                      absl::string_view data)
      ABSL_SHARED_LOCKS_REQUIRED(mutex_);

  // Actually perform the session auth procedure using member variables.
  absl::Status LockedDoSessionAuth() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  // Log out of the current session by sending HTTP DELETE on the session URI.
  void EndCurrentSession() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Private constructor for creating a transport with a client and target.
  // The public Make* functions should be used instead to avoid exposing the
  // internal target structs in the public interface.
  HttpRedfishTransport(std::unique_ptr<HttpClient> client,
                       std::variant<TcpTarget, UdsTarget> target);

  // Helper function for creating a HTTP request, overloaded on the target type.
  std::unique_ptr<HttpClient::HttpRequest> MakeRequest(TcpTarget target,
                                                       absl::string_view path,
                                                       absl::string_view data)
      ABSL_SHARED_LOCKS_REQUIRED(mutex_);
  std::unique_ptr<HttpClient::HttpRequest> MakeRequest(UdsTarget target,
                                                       absl::string_view path,
                                                       absl::string_view data)
      ABSL_SHARED_LOCKS_REQUIRED(mutex_);

  absl::Mutex mutex_;
  std::unique_ptr<HttpClient> client_ ABSL_GUARDED_BY(mutex_);
  std::variant<TcpTarget, UdsTarget> target_ ABSL_GUARDED_BY(mutex_);

  // Session auth parameters.
  // Save the username and password in case we need to re-establish a session.
  std::string session_username_ ABSL_GUARDED_BY(mutex_);
  std::string session_password_ ABSL_GUARDED_BY(mutex_);
  // The X-Auth-Token to be used in HTTP request headers.
  std::string x_auth_token_ ABSL_GUARDED_BY(mutex_);
  // The session URI that stores our session state.
  std::string session_auth_uri_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_TRANSPORT_HTTP_H_
