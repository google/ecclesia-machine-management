/*
 * Copyright 2022 Google LLC
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

#ifndef ECCLESIA_LIB_REDFISH_TRANSPORT_GRPC_H_
#define ECCLESIA_LIB_REDFISH_TRANSPORT_GRPC_H_

#include <memory>

#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.grpc.pb.h"
#include "ecclesia/lib/redfish/transport/grpc_dynamic_options.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/time/clock.h"

namespace ecclesia {

class GrpcRedfishTransport : public RedfishTransport {
 public:
  struct Params {
    // Clock used for all operations.
    Clock* clock = Clock::RealClock();
    // Timeout used for all operations.
    absl::Duration timeout = absl::Seconds(5);
  };

  // Creates an GrpcRedfishTransport using a specified endpoint.
  // Params:
  //   endpoint: e.g. "dns:///localhost:80", "unix:///var/run/my.socket"
  GrpcRedfishTransport(std::string_view endpoint, const Params& params,
                       const GrpcDynamicImplOptions& options,
                       ServiceRootUri service_root = ServiceRootUri::kRedfish);
  GrpcRedfishTransport(std::string_view endpoint);

  ~GrpcRedfishTransport() override {}

  // Updates the current GrpcRedfishTransport instance to a new endpoint.
  // It is valid to switch from a TCP endpoint to a UDS endpoint and vice-versa.
  void UpdateToNetworkEndpoint(absl::string_view tcp_endpoint)
      ABSL_LOCKS_EXCLUDED(mutex_) override;
  void UpdateToUdsEndpoint(absl::string_view unix_domain_socket)
      ABSL_LOCKS_EXCLUDED(mutex_) override;

  // Returns the path of the root URI for the Redfish service this transport is
  // connected to.
  absl::string_view GetRootUri() override;

  absl::StatusOr<Result> Get(absl::string_view path)
      ABSL_LOCKS_EXCLUDED(mutex_) override;
  absl::StatusOr<Result> Post(absl::string_view path, absl::string_view data)
      ABSL_LOCKS_EXCLUDED(mutex_) override;
  absl::StatusOr<Result> Patch(absl::string_view path, absl::string_view data)
      ABSL_LOCKS_EXCLUDED(mutex_) override;
  absl::StatusOr<Result> Delete(absl::string_view path, absl::string_view data)
      ABSL_LOCKS_EXCLUDED(mutex_) override;

 private:
  absl::Mutex mutex_;
  std::unique_ptr<::redfish::v1::RedfishV1::Stub> client_
      ABSL_GUARDED_BY(mutex_);
  Params params_;
  // The service root for RedfishInterface.
  const ServiceRootUri service_root_;
  std::string fqdn_;
};

absl::StatusOr<std::unique_ptr<GrpcRedfishTransport>>
CreateGrpcRedfishTransport(
    std::string_view endpoint, const GrpcRedfishTransport::Params& params,
    const GrpcDynamicImplOptions& options,
    ServiceRootUri service_root = ServiceRootUri::kRedfish);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_TRANSPORT_GRPC_H_
