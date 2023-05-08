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

#ifndef ECCLESIA_LIB_REDFISH_PROXY_REDFISH_PROXY_H_
#define ECCLESIA_LIB_REDFISH_PROXY_REDFISH_PROXY_H_

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.grpc.pb.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.pb.h"
#include "ecclesia/lib/redfish/proto/redfish_v1_grpc_include.h"
#include "grpcpp/server_context.h"
#include "grpcpp/support/status.h"

namespace ecclesia {
inline constexpr absl::string_view kGet = "Get";
inline constexpr absl::string_view kPatch = "Patch";
inline constexpr absl::string_view kPost = "Post";
inline constexpr absl::string_view kPut = "Put";
inline constexpr absl::string_view kDelete = "Delete";

class RedfishV1GrpcProxy : public GrpcRedfishV1::Service {
 public:
  RedfishV1GrpcProxy() = default;
  ~RedfishV1GrpcProxy() = default;
  // All of the proxy RPCs.
  grpc::Status Get(grpc::ServerContext *context,
                   const redfish::v1::Request *request,
                   redfish::v1::Response *response) override;
  grpc::Status Post(grpc::ServerContext *context,
                    const redfish::v1::Request *request,
                    redfish::v1::Response *response) override;
  grpc::Status Patch(grpc::ServerContext *context,
                     const redfish::v1::Request *request,
                     redfish::v1::Response *response) override;
  grpc::Status Put(grpc::ServerContext *context,
                   const redfish::v1::Request *request,
                   redfish::v1::Response *response) override;
  grpc::Status Delete(grpc::ServerContext *context,
                      const redfish::v1::Request *request,
                      redfish::v1::Response *response) override;

 private:
  // Virtual function that subclasses can override to do authorization checks.
  // This will be called with the server context on every proxy call. If this
  // returns false, then the RPC will be rejected with a "permission denied"
  // error. The default implementation just returns true.
  virtual absl::Status IsRpcAuthorized(grpc::ServerContext &context) {
    return absl::OkStatus();
  }

  // Virtual function that subclasses should override to do proxy RPCs
  // implementation on different backend.
  virtual grpc::Status GetHandler(grpc::ClientContext *context,
                                  const redfish::v1::Request *request,
                                  redfish::v1::Response *response) = 0;
  virtual grpc::Status PostHandler(grpc::ClientContext *context,
                                   const redfish::v1::Request *request,
                                   redfish::v1::Response *response) = 0;
  virtual grpc::Status PatchHandler(grpc::ClientContext *context,
                                    const redfish::v1::Request *request,
                                    redfish::v1::Response *response) = 0;
  virtual grpc::Status PutHandler(grpc::ClientContext *context,
                                  const redfish::v1::Request *request,
                                  redfish::v1::Response *response) = 0;
  virtual grpc::Status DeleteHandler(grpc::ClientContext *context,
                                     const redfish::v1::Request *request,
                                     redfish::v1::Response *response) = 0;
};
}  // namespace ecclesia
#endif  // ECCLESIA_LIB_REDFISH_PROXY_REDFISH_PROXY_H_
