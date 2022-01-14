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

#include "ecclesia/lib/redfish/transport/grpc.h"

#include <optional>

#include "google/protobuf/util/json_util.h"
#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "ecclesia/lib/http/codes.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.grpc.pb.h"
#include "ecclesia/lib/redfish/transport/struct_proto_conversion.h"
#include "ecclesia/lib/status/macros.h"
#include "grpcpp/client_context.h"
#include "grpcpp/create_channel.h"
#include "grpcpp/security/credentials.h"
#include "grpcpp/support/status.h"
#include "single_include/nlohmann/json.hpp"
#include "ecclesia/lib/status/rpc.h"

namespace ecclesia {
namespace {
template <typename RpcFunc>
absl::StatusOr<RedfishTransport::Result> DoRpc(
    absl::string_view path, std::optional<google::protobuf::Struct> message,
    GrpcRedfishTransport::Params params, RpcFunc rpc) {
  redfish::v1::Request request;
  request.set_url(std::string(path));
  if (message.has_value()) {
    *request.mutable_message() = *std::move(message);
  }
  grpc::ClientContext context;
  context.set_deadline(ToChronoTime(params.clock->Now() + params.timeout));

  ::redfish::v1::Response response;
  if (grpc::Status status = rpc(context, request, &response); !status.ok()) {
    return AsAbslStatus(status);
  }

  RedfishTransport::Result ret_result;
  ret_result.body = StructToJson(response.message());
  return ret_result;
}
}  // namespace

GrpcRedfishTransport::GrpcRedfishTransport(std::string endpoint, Params params,
                                           libredfish::ServiceRoot service_root)
    : client_(redfish::v1::RedfishV1::NewStub(grpc::CreateChannel(
          std::move(endpoint), grpc::InsecureChannelCredentials()))),
      params_(std::move(params)),
      service_root_(std::move(service_root)) {}

GrpcRedfishTransport::GrpcRedfishTransport(std::string endpoint)
    : client_(redfish::v1::RedfishV1::NewStub(grpc::CreateChannel(
          std::move(endpoint), grpc::InsecureChannelCredentials()))),
      params_({}),
      service_root_(std::move(libredfish::ServiceRoot::kRedfish)) {}

void GrpcRedfishTransport::UpdateToNetworkEndpoint(
    absl::string_view tcp_endpoint) {
  absl::WriterMutexLock mu(&mutex_);
  client_ = redfish::v1::RedfishV1::NewStub(
      grpc::CreateChannel(absl::StrCat("dns:///", tcp_endpoint),
                          grpc::InsecureChannelCredentials()));
}
void GrpcRedfishTransport::UpdateToUdsEndpoint(
    absl::string_view unix_domain_socket) {
  absl::WriterMutexLock mu(&mutex_);
  client_ = redfish::v1::RedfishV1::NewStub(
      grpc::CreateChannel(absl::StrCat("unix://", unix_domain_socket),
                          grpc::experimental::LocalCredentials(UDS)));
}

absl::string_view GrpcRedfishTransport::GetRootUri() {
  return libredfish::RedfishInterface::ServiceRootToUri(service_root_);
}

absl::StatusOr<RedfishTransport::Result> GrpcRedfishTransport::Get(
    absl::string_view path) {
  return DoRpc(
      path, std::nullopt, params_,
      [this](grpc::ClientContext& context, const redfish::v1::Request& request,
             ::redfish::v1::Response* response) -> grpc::Status {
        absl::ReaderMutexLock mu(&mutex_);
        return client_->Get(&context, request, response);
      });
}

absl::StatusOr<RedfishTransport::Result> GrpcRedfishTransport::Post(
    absl::string_view path, absl::string_view data) {
  ::google::protobuf::Struct request_body;
  ECCLESIA_RETURN_IF_ERROR(AsAbslStatus(google::protobuf::util::JsonStringToMessage(
      std::string(data), &request_body, google::protobuf::util::JsonParseOptions())));
  return DoRpc(
      path, std::move(request_body), params_,
      [this](grpc::ClientContext& context, const redfish::v1::Request& request,
             ::redfish::v1::Response* response) -> grpc::Status {
        absl::ReaderMutexLock mu(&mutex_);
        return client_->Post(&context, request, response);
      });
}

absl::StatusOr<RedfishTransport::Result> GrpcRedfishTransport::Patch(
    absl::string_view path, absl::string_view data) {
  ::google::protobuf::Struct request_body;
  ECCLESIA_RETURN_IF_ERROR(AsAbslStatus(google::protobuf::util::JsonStringToMessage(
      std::string(data), &request_body, google::protobuf::util::JsonParseOptions())));
  return DoRpc(
      path, std::move(request_body), params_,
      [this](grpc::ClientContext& context, const redfish::v1::Request& request,
             ::redfish::v1::Response* response) -> grpc::Status {
        absl::ReaderMutexLock mu(&mutex_);
        return client_->Patch(&context, request, response);
      });
}

absl::StatusOr<RedfishTransport::Result> GrpcRedfishTransport::Delete(
    absl::string_view path, absl::string_view data) {
  ::google::protobuf::Struct request_body;
  ECCLESIA_RETURN_IF_ERROR(AsAbslStatus(google::protobuf::util::JsonStringToMessage(
      std::string(data), &request_body, google::protobuf::util::JsonParseOptions())));
  return DoRpc(
      path, std::move(request_body), params_,
      [this](grpc::ClientContext& context, const redfish::v1::Request& request,
             ::redfish::v1::Response* response) -> grpc::Status {
        absl::ReaderMutexLock mu(&mutex_);
        return client_->Delete(&context, request, response);
      });
}

}  // namespace ecclesia
