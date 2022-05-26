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

#ifndef ECCLESIA_LIB_REDFISH_PROXY_GRPC_H_
#define ECCLESIA_LIB_REDFISH_PROXY_GRPC_H_

#include <string>

#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.grpc.pb.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.pb.h"
#include "grpcpp/server_context.h"
#include "grpcpp/support/status.h"

namespace ecclesia {

class RedfishV1GrpcProxy final : public redfish::v1::RedfishV1::Service {
 public:
  // Define a proxy with the given name that forward requests to the given stub.
  // The name is for use in logging and other debugging and tracing contexts.
  RedfishV1GrpcProxy(std::string name,
                     redfish::v1::RedfishV1::StubInterface *stub);

  // The name of the proxy service.
  const std::string &name() const { return name_; }

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
  // Generic method that gets called before every request is forwarded. Is given
  // the RPC name and the request. Used for any generic pre-RPC operations such
  // as logging the request.
  void PreCall(absl::string_view rpc_name, const redfish::v1::Request &request);

  std::string name_;
  redfish::v1::RedfishV1::StubInterface *stub_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_PROXY_GRPC_H_
