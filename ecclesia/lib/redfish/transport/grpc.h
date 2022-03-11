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

struct GrpcTransportParams {
  // Clock used for all operations.
  Clock* clock = Clock::RealClock();
  // Timeout used for all operations.
  absl::Duration timeout = absl::Seconds(5);
};

absl::StatusOr<std::unique_ptr<RedfishTransport>>
CreateGrpcRedfishTransport(
    std::string_view endpoint, const GrpcTransportParams& params,
    const GrpcDynamicImplOptions& options,
    ServiceRootUri service_root = ServiceRootUri::kRedfish);

absl::StatusOr<std::unique_ptr<RedfishTransport>>
CreateGrpcRedfishTransport(
    std::string_view endpoint, const GrpcTransportParams& params,
    const std::shared_ptr<grpc::ChannelCredentials>& creds,
    ServiceRootUri service_root = ServiceRootUri::kRedfish);


}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_TRANSPORT_GRPC_H_
