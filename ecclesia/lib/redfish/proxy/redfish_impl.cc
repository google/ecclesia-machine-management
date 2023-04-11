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

#include "ecclesia/lib/redfish/proxy/redfish_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/atomic/sequence.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.pb.h"
#include "ecclesia/lib/redfish/proxy/redfish_proxy.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/redfish/utils.h"
#include "ecclesia/lib/status/rpc.h"
#include "grpcpp/client_context.h"
#include "grpcpp/server_context.h"
#include "grpcpp/support/status.h"

namespace ecclesia {

RedfishProxyRedfishBackend::RedfishProxyRedfishBackend(
    std::string name, RedfishTransport *redfish_transport)
    : name_(std::move(name)), redfish_transport_(redfish_transport) {}

std::string RedfishProxyRedfishBackend::PreCall(
    SequenceNumberGenerator::ValueType seq_num, absl::string_view rpc_name,
    const redfish::v1::Request &request) {
  // Write a log message for any requests which can modify state. These
  // requests should be relatively rare and so shouldn't produce too much log
  // spam, and mutating requests are the most important ones to have some
  // visibility into.
  std::string redfish_data;
  if (rpc_name != kGet) {
    RpcInfoLog(seq_num, absl::StrCat("sending ", rpc_name,
                                     " request for URL: ", request.url()));
    // Extract the body information from RedfishV1 request for Redfish
    // transport.
    redfish_data = request.json_str();
  }
  return redfish_data;
}

void RedfishProxyRedfishBackend::PostCall(
    SequenceNumberGenerator::ValueType seq_num, absl::string_view rpc_name,
    const redfish::v1::Request &request, const grpc::Status &rpc_status,
    const RedfishTransport::Result &redfish_result,
    redfish::v1::Response *grpc_response) {
  if (rpc_name != kGet) {
    RpcInfoLog(seq_num,
               absl::StrCat(
                   "completed ", rpc_name, " request for URL: ", request.url(),
                   " with status: ", AsAbslStatus(rpc_status).message()));
  }

  // Extract body information from Redfish transport result.
  std::string json_str;
  if (std::holds_alternative<nlohmann::json>(redfish_result.body)) {
    json_str = std::get<nlohmann::json>(redfish_result.body).dump(1);
  } else if (std::holds_alternative<RedfishTransport::bytes>(
                 redfish_result.body)) {
    json_str = RedfishTransportBytesToString(
        std::get<RedfishTransport::bytes>(redfish_result.body));
  }

  // Generate RedfishV1 response from Redfish transport result.
  *grpc_response->mutable_json_str() = std::move(json_str);
  grpc_response->set_code(redfish_result.code);
  for (const auto &[key, value] : redfish_result.headers) {
    grpc_response->mutable_headers()->insert({key, value});
  }
}

grpc::Status RedfishProxyRedfishBackend::GetHandler(
    grpc::ClientContext *context, const redfish::v1::Request *request,
    redfish::v1::Response *response) {
  SequenceNumberGenerator::ValueType seq_num = GenerateSeqNum();

  // Get method does not need the RedfishV1 request body information.
  PreCall(seq_num, kGet, *request);

  // Call the Redfish transport method.
  absl::StatusOr<RedfishTransport::Result> redfish_result =
      redfish_transport_->Get(request->url());
  if (!redfish_result.ok()) {
    return grpc::Status(
        grpc::StatusCode::INTERNAL,
        absl::StrFormat(
            "Backend returns the following error: code: %d message: %s",
            redfish_result.status().code(), redfish_result.status().message()));
  }
  grpc::Status status(grpc::StatusCode::OK, "HTTP response OK");

  // Translate the Redfish transport result to Grpc response.
  PostCall(seq_num, kGet, *request, status, *redfish_result, response);
  return status;
}

grpc::Status RedfishProxyRedfishBackend::PostHandler(
    grpc::ClientContext *context, const redfish::v1::Request *request,
    redfish::v1::Response *response) {
  SequenceNumberGenerator::ValueType seq_num = GenerateSeqNum();

  // Extract RedfishV1 request body information from request
  std::string redfish_data = PreCall(seq_num, kPost, *request);

  // Call the Redfish transport method.
  absl::StatusOr<RedfishTransport::Result> redfish_result =
      redfish_transport_->Post(request->url(), redfish_data);
  if (!redfish_result.ok()) {
    return grpc::Status(
        grpc::StatusCode::INTERNAL,
        absl::StrFormat(
            "Backend returns the following error: code: %d message: %s",
            redfish_result.status().code(), redfish_result.status().message()));
  }
  grpc::Status status(grpc::StatusCode::OK, "HTTP response OK");

  // Translate the Redfish transport result to Grpc response.
  PostCall(seq_num, kPost, *request, status, *redfish_result, response);
  return status;
}

grpc::Status RedfishProxyRedfishBackend::PutHandler(
    grpc::ClientContext *context, const redfish::v1::Request *request,
    redfish::v1::Response *response) {
  // There is no Put method in Redfish transport.
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED,
                      "Redfish HTTP Put Methord Is Unimplemented.");
}

grpc::Status RedfishProxyRedfishBackend::PatchHandler(
    grpc::ClientContext *context, const redfish::v1::Request *request,
    redfish::v1::Response *response) {
  SequenceNumberGenerator::ValueType seq_num = GenerateSeqNum();

  // Extract RedfishV1 request body information from request
  std::string redfish_data = PreCall(seq_num, kPatch, *request);

  // Call the Redfish transport method.
  absl::StatusOr<RedfishTransport::Result> redfish_result =
      redfish_transport_->Patch(request->url(), redfish_data);
  if (!redfish_result.ok()) {
    return grpc::Status(
        grpc::StatusCode::INTERNAL,
        absl::StrFormat(
            "Backend returns the following error: code: %d message: %s",
            redfish_result.status().code(), redfish_result.status().message()));
  }
  grpc::Status status(grpc::StatusCode::OK, "HTTP response OK");

  // Translate the Redfish transport result to Grpc response.
  PostCall(seq_num, kPatch, *request, status, *redfish_result, response);
  return status;
}

grpc::Status RedfishProxyRedfishBackend::DeleteHandler(
    grpc::ClientContext *context, const redfish::v1::Request *request,
    redfish::v1::Response *response) {
  SequenceNumberGenerator::ValueType seq_num = GenerateSeqNum();

  // Extract RedfishV1 request body information from request
  std::string redfish_data = PreCall(seq_num, kDelete, *request);

  // Call the Redfish transport method.
  absl::StatusOr<RedfishTransport::Result> redfish_result =
      redfish_transport_->Delete(request->url(), redfish_data);
  if (!redfish_result.ok()) {
    return grpc::Status(
        grpc::StatusCode::INTERNAL,
        absl::StrFormat(
            "Backend returns the following error: code: %d message: %s",
            redfish_result.status().code(), redfish_result.status().message()));
  }
  grpc::Status status(grpc::StatusCode::OK, "HTTP response OK");

  // Translate the Redfish transport result to Grpc response.
  PostCall(seq_num, kDelete, *request, status, *redfish_result, response);
  return status;
}

}  // namespace ecclesia
