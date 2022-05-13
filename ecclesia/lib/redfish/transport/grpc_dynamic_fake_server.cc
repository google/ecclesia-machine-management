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

#include "ecclesia/lib/redfish/transport/grpc_dynamic_fake_server.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "absl/strings/str_cat.h"
#include "ecclesia/lib/network/testing.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.pb.h"
#include "grpcpp/security/server_credentials.h"
#include "grpcpp/server.h"
#include "grpcpp/server_builder.h"
#include "grpcpp/server_context.h"
#include "grpcpp/support/status.h"

namespace ecclesia {

using ::redfish::v1::Request;

GrpcDynamicFakeServer::GrpcDynamicFakeServer()
    : GrpcDynamicFakeServer(grpc::InsecureServerCredentials()) {}

GrpcDynamicFakeServer::GrpcDynamicFakeServer(int port)
    : GrpcDynamicFakeServer(port, grpc::InsecureServerCredentials()) {}

GrpcDynamicFakeServer::GrpcDynamicFakeServer(
    std::shared_ptr<grpc::ServerCredentials> credentials)
    : GrpcDynamicFakeServer(FindUnusedPortOrDie(), std::move(credentials)) {}

GrpcDynamicFakeServer::GrpcDynamicFakeServer(
    int port, std::shared_ptr<grpc::ServerCredentials> credentials)
    : port_(port) {
  std::string server_address(absl::StrCat("localhost:", port_));
  grpc::ServerBuilder builder;
  builder.AddListeningPort(server_address, std::move(credentials));
  builder.RegisterService(&service_impl_);
  grpc_server_ = builder.BuildAndStart();
}

GrpcDynamicFakeServer::~GrpcDynamicFakeServer() { grpc_server_->Shutdown(); }

void GrpcDynamicFakeServer::SetCallback(
    std::function<grpc::Status(grpc::ServerContext *, const Request *,
                               redfish::v1::Response *)>
        callback) {
  service_impl_.SetCallback(std::move(callback));
}

}  // namespace ecclesia
