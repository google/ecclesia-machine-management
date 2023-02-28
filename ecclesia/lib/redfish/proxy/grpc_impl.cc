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

#include "ecclesia/lib/redfish/proxy/grpc_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/atomic/sequence.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.grpc.pb.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.pb.h"
#include "ecclesia/lib/status/rpc.h"
#include "grpcpp/client_context.h"
#include "grpcpp/server_context.h"
#include "grpcpp/support/status.h"

namespace ecclesia {

RedfishProxyGrpcBackend::RedfishProxyGrpcBackend(
    std::string name, GrpcRedfishV1::StubInterface *stub)
    : name_(std::move(name)), stub_(stub) {}

void RedfishProxyGrpcBackend::PreCall(
    SequenceNumberGenerator::ValueType seq_num, absl::string_view rpc_name,
    const redfish::v1::Request &request) {
  // Write a log message for any requests which can modify state. These requests
  // should be relatively rare and so shouldn't produce too much log spam, and
  // mutating requests are the most important ones to have some visibility into.
  if (rpc_name != kGet) {
    RpcInfoLog(seq_num, absl::StrCat("sending ", rpc_name,
                                     " request for URL: ", request.url()));
  }
}

void RedfishProxyGrpcBackend::PostCall(
    SequenceNumberGenerator::ValueType seq_num, absl::string_view rpc_name,
    const redfish::v1::Request &request, const grpc::Status &rpc_status) {
  // Write a log message for the RPC name, the request, and the status of the
  // result.
  if (rpc_name != kGet) {
    RpcInfoLog(seq_num,
               absl::StrCat(
                   "completed ", rpc_name, " request for URL: ", request.url(),
                   " with status: ", AsAbslStatus(rpc_status).message()));
  }
}

// Forward requests, write a log message and return the status of result
grpc::Status RedfishProxyGrpcBackend::GetHandler(
    grpc::ClientContext *context, const redfish::v1::Request *request,
    redfish::v1::Response *response) {
  SequenceNumberGenerator::ValueType seq_num = GenerateSeqNum();
  PreCall(seq_num, kGet, *request);
  grpc::Status status = stub_->Get(context, *request, response);
  PostCall(seq_num, kGet, *request, status);
  return status;
}

grpc::Status RedfishProxyGrpcBackend::PostHandler(
    grpc::ClientContext *context, const redfish::v1::Request *request,
    redfish::v1::Response *response) {
  SequenceNumberGenerator::ValueType seq_num = GenerateSeqNum();
  PreCall(seq_num, kPost, *request);
  grpc::Status status = stub_->Post(context, *request, response);
  PostCall(seq_num, kPost, *request, status);
  return status;
}

grpc::Status RedfishProxyGrpcBackend::PutHandler(
    grpc::ClientContext *context, const redfish::v1::Request *request,
    redfish::v1::Response *response) {
  SequenceNumberGenerator::ValueType seq_num = GenerateSeqNum();
  PreCall(seq_num, kPut, *request);
  grpc::Status status = stub_->Put(context, *request, response);
  PostCall(seq_num, kPut, *request, status);
  return status;
}

grpc::Status RedfishProxyGrpcBackend::PatchHandler(
    grpc::ClientContext *context, const redfish::v1::Request *request,
    redfish::v1::Response *response) {
  SequenceNumberGenerator::ValueType seq_num = GenerateSeqNum();
  PreCall(seq_num, kPatch, *request);
  grpc::Status status = stub_->Patch(context, *request, response);
  PostCall(seq_num, kPatch, *request, status);
  return status;
}

grpc::Status RedfishProxyGrpcBackend::DeleteHandler(
    grpc::ClientContext *context, const redfish::v1::Request *request,
    redfish::v1::Response *response) {
  SequenceNumberGenerator::ValueType seq_num = GenerateSeqNum();
  PreCall(seq_num, kDelete, *request);
  grpc::Status status = stub_->Delete(context, *request, response);
  PostCall(seq_num, kDelete, *request, status);
  return status;
}

}  // namespace ecclesia
