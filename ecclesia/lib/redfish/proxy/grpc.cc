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

#include "ecclesia/lib/redfish/proxy/grpc.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/atomic/sequence.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.grpc.pb.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.pb.h"
#include "ecclesia/lib/redfish/proto/redfish_v1_grpc_include.h"
#include "ecclesia/lib/status/rpc.h"
#include "grpcpp/client_context.h"
#include "grpcpp/server_context.h"
#include "grpcpp/support/status.h"

namespace ecclesia {
namespace {

// Forward metadata values from the client to the server.
void ForwardMetadataFromServerContext(grpc::ServerContext &server_context,
                                      grpc::ClientContext &client_context) {
  for (const auto &[key, value] : server_context.client_metadata()) {
    client_context.AddMetadata(std::string(key.data(), key.size()),
                               std::string(value.data(), value.size()));
  }
}

}  // namespace

RedfishV1GrpcProxy::RedfishV1GrpcProxy(
    std::string name, GrpcRedfishV1::StubInterface *stub)
    : name_(std::move(name)), stub_(stub) {}

// All of the HTTP handlers are implemented in the exact same way just with
// different RPC names, so to minimize boilerplate we use a macro to generate
// all of the definitions
#define DEFINE_RPC_HANDLER(name)                                             \
  grpc::Status RedfishV1GrpcProxy::name(grpc::ServerContext *context,        \
                                        const redfish::v1::Request *request, \
                                        redfish::v1::Response *response) {   \
    SequenceNumberGenerator::ValueType seq_num = GenerateSeqNum();           \
    if (!IsRpcAuthorized(*context)) {                                        \
      return grpc::Status(grpc::StatusCode::PERMISSION_DENIED,               \
                          "User is not authorized to call this RPC");        \
    }                                                                        \
    auto client_context = grpc::ClientContext::FromServerContext(*context);  \
    ForwardMetadataFromServerContext(*context, *client_context);             \
    PreCall(seq_num, #name, *request);                                       \
    grpc::Status status =                                                    \
        stub_->name(client_context.get(), *request, response);               \
    PostCall(seq_num, #name, *request, status);                              \
    return status;                                                           \
  }
DEFINE_RPC_HANDLER(Get)
DEFINE_RPC_HANDLER(Post)
DEFINE_RPC_HANDLER(Patch)
DEFINE_RPC_HANDLER(Put)
DEFINE_RPC_HANDLER(Delete)
#undef DEFINE_RPC_HANDLER

void RedfishV1GrpcProxy::PreCall(SequenceNumberGenerator::ValueType seq_num,
                                 absl::string_view rpc_name,
                                 const redfish::v1::Request &request) {
  // Write a log message for any requests which can modify state. These requests
  // should be relatively rare and so shouldn't produce too much log spam, and
  // mutating requests are the most important ones to have some visibility into.
  if (rpc_name != "Get") {
    RpcInfoLog(seq_num, absl::StrCat("sending ", rpc_name,
                                     " request for URL: ", request.url()));
  }
}

void RedfishV1GrpcProxy::PostCall(SequenceNumberGenerator::ValueType seq_num,
                                  absl::string_view rpc_name,
                                  const redfish::v1::Request &request,
                                  const grpc::Status &rpc_status) {
  if (rpc_name != "Get") {
    RpcInfoLog(seq_num,
               absl::StrCat(
                   "completed ", rpc_name, " request for URL: ", request.url(),
                   " with status: ", AsAbslStatus(rpc_status).message()));
  }
}

}  // namespace ecclesia
