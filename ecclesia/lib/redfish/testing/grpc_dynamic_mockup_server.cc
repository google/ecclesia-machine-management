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

#include "ecclesia/lib/redfish/testing/grpc_dynamic_mockup_server.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "google/protobuf/util/json_util.h"
#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/meta/type_traits.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "ecclesia/lib/http/codes.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.pb.h"
#include "ecclesia/lib/redfish/test_mockup.h"
#include "ecclesia/lib/status/rpc.h"
#include "grpcpp/security/server_credentials.h"
#include "grpcpp/server.h"
#include "grpcpp/server_builder.h"
#include "grpcpp/server_context.h"
#include "grpcpp/support/status.h"

namespace ecclesia {

namespace {

using ::redfish::v1::Request;

constexpr char kResourceKey[] = "redfish-resource";

absl::Status SetGrpcResponseAndReturnStatus(RedfishVariant variant,
                                            redfish::v1::Response* response) {
  absl::Status status = absl::OkStatus();
  if (!variant.httpcode().has_value()) {
    return absl::InternalError("The response doesn't have HTTP code.");
  }
  response->set_code(variant.httpcode().value());
  if (variant.AsObject() != nullptr) {
    if (absl::Status status = AsAbslStatus(google::protobuf::util::JsonStringToMessage(
            variant.DebugString(), response->mutable_message()));
        !status.ok()) {
      return status;
    }
  }
  return status;
}

grpc::Status CheckRequestResourceAndMetadataResource(
    const grpc::ServerContext& context, const Request& request) {
  if (auto it = context.client_metadata().find(kResourceKey);
      it == context.client_metadata().end()) {
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION,
                        "missing resource URL in client metadata!");
  } else if (it->second != request.url()) {
    return grpc::Status(
        grpc::StatusCode::FAILED_PRECONDITION,
        "Resource URL in client metadata and request shall be the same!");
  }
  return grpc::Status::OK;
}

}  // namespace

grpc::Status GrpcDynamicMockupServer::RedfishV1Impl::Get(
    grpc::ServerContext* context, const Request* request,
    redfish::v1::Response* response) {
  if (grpc::Status status =
          CheckRequestResourceAndMetadataResource(*context, *request);
      !status.ok()) {
    return status;
  }
  absl::MutexLock mu(&patch_lock_);
  if (auto itr = rest_get_handlers_.find(request->url());
      itr != rest_get_handlers_.end()) {
    return itr->second(context, request, response);
  }
  return StatusToGrpcStatus(SetGrpcResponseAndReturnStatus(
      redfish_intf_->UncachedGetUri(request->url()), response));
}
grpc::Status GrpcDynamicMockupServer::RedfishV1Impl::Post(
    grpc::ServerContext* context, const Request* request,
    redfish::v1::Response* response) {
  if (grpc::Status status =
          CheckRequestResourceAndMetadataResource(*context, *request);
      !status.ok()) {
    return status;
  }
  absl::MutexLock mu(&patch_lock_);
  if (auto itr = rest_post_handlers_.find(request->url());
      itr != rest_post_handlers_.end()) {
    return itr->second(context, request, response);
  }

  std::string message;
  absl::Status status = AsAbslStatus(
      google::protobuf::util::MessageToJsonString(request->message(), &message));
  if (!status.ok()) {
    return StatusToGrpcStatus(status);
  }
  return StatusToGrpcStatus(SetGrpcResponseAndReturnStatus(
      redfish_intf_->PostUri(request->url(), message), response));
}
grpc::Status GrpcDynamicMockupServer::RedfishV1Impl::Patch(
    grpc::ServerContext* context, const Request* request,
    redfish::v1::Response* response) {
  if (grpc::Status status =
          CheckRequestResourceAndMetadataResource(*context, *request);
      !status.ok()) {
    return status;
  }
  absl::MutexLock mu(&patch_lock_);
  if (auto itr = rest_patch_handlers_.find(request->url());
      itr != rest_patch_handlers_.end()) {
    return itr->second(context, request, response);
  }

  std::string message;
  absl::Status status = AsAbslStatus(
      google::protobuf::util::MessageToJsonString(request->message(), &message));
  if (!status.ok()) {
    return StatusToGrpcStatus(status);
  }
  return StatusToGrpcStatus(SetGrpcResponseAndReturnStatus(
      redfish_intf_->PatchUri(request->url(), message), response));
}
grpc::Status GrpcDynamicMockupServer::RedfishV1Impl::Put(
    grpc::ServerContext* context, const Request* request,
    redfish::v1::Response* response) {
  if (grpc::Status status =
          CheckRequestResourceAndMetadataResource(*context, *request);
      !status.ok()) {
    return status;
  }
  return StatusToGrpcStatus(
      absl::UnimplementedError("Put RPC is not implemented yet."));
}
grpc::Status GrpcDynamicMockupServer::RedfishV1Impl::Delete(
    grpc::ServerContext* context, const Request* request,
    redfish::v1::Response* response) {
  if (grpc::Status status =
          CheckRequestResourceAndMetadataResource(*context, *request);
      !status.ok()) {
    return status;
  }
  // DELETE.
  return StatusToGrpcStatus(
      absl::UnimplementedError("Delete RPC is not implemented yet."));
}

void GrpcDynamicMockupServer::RedfishV1Impl::AddHttpGetHandler(
    absl::string_view uri, HandlerFunc handler)
    ABSL_LOCKS_EXCLUDED(patch_lock_) {
  absl::MutexLock mu(&patch_lock_);
  rest_get_handlers_[uri] = std::move(handler);
}
void GrpcDynamicMockupServer::RedfishV1Impl::AddHttpPatchHandler(
    absl::string_view uri, HandlerFunc handler)
    ABSL_LOCKS_EXCLUDED(patch_lock_) {
  absl::MutexLock mu(&patch_lock_);
  rest_patch_handlers_[uri] = std::move(handler);
}
void GrpcDynamicMockupServer::RedfishV1Impl::AddHttpPostHandler(
    absl::string_view uri, HandlerFunc handler)
    ABSL_LOCKS_EXCLUDED(patch_lock_) {
  absl::MutexLock mu(&patch_lock_);
  rest_post_handlers_[uri] = std::move(handler);
}

// Clear all registered handlers.
void GrpcDynamicMockupServer::RedfishV1Impl::ClearHandlers()
    ABSL_LOCKS_EXCLUDED(patch_lock_) {
  absl::MutexLock mu(&patch_lock_);
  rest_get_handlers_.clear();
  rest_patch_handlers_.clear();
  rest_post_handlers_.clear();
}

GrpcDynamicMockupServer::GrpcDynamicMockupServer(absl::string_view mockup_shar,
                                                 absl::string_view host,
                                                 int port)
    : GrpcDynamicMockupServer(mockup_shar, host, port,
                              grpc::InsecureServerCredentials()) {}

GrpcDynamicMockupServer::GrpcDynamicMockupServer(
    absl::string_view mockup_shar, absl::string_view host, int port,
    std::shared_ptr<grpc::ServerCredentials> credentials)
    : mockup_server_(mockup_shar),
      redfish_v1_impl_(absl::make_unique<RedfishV1Impl>(
          mockup_server_.RedfishClientInterface())) {
  std::string server_address = absl::StrCat(host, ":", port);
  grpc::ServerBuilder builder;
  builder.AddListeningPort(server_address, std::move(credentials));
  builder.RegisterService(redfish_v1_impl_.get());
  server_ = builder.BuildAndStart();
}

GrpcDynamicMockupServer::GrpcDynamicMockupServer(absl::string_view mockup_shar,
                                                 absl::string_view uds_path)
    : GrpcDynamicMockupServer(mockup_shar, uds_path,
                              grpc::experimental::LocalServerCredentials(UDS)) {
}

GrpcDynamicMockupServer::GrpcDynamicMockupServer(
    absl::string_view mockup_shar, absl::string_view uds_path,
    std::shared_ptr<grpc::ServerCredentials> credentials)
    : mockup_server_(mockup_shar),
      redfish_v1_impl_(absl::make_unique<RedfishV1Impl>(
          mockup_server_.RedfishClientInterface())) {
  std::string server_address = absl::StrCat("unix://", uds_path);
  grpc::ServerBuilder builder;
  builder.AddListeningPort(server_address, std::move(credentials));
  builder.RegisterService(redfish_v1_impl_.get());
  server_ = builder.BuildAndStart();
}

GrpcDynamicMockupServer::~GrpcDynamicMockupServer() { ClearHandlers(); }

void GrpcDynamicMockupServer::ClearHandlers() {
  redfish_v1_impl_->ClearHandlers();
}
void GrpcDynamicMockupServer::AddHttpGetHandler(absl::string_view uri,
                                                HandlerFunc handler) {
  redfish_v1_impl_->AddHttpGetHandler(uri, std::move(handler));
}
void GrpcDynamicMockupServer::AddHttpPatchHandler(absl::string_view uri,
                                                  HandlerFunc handler) {
  redfish_v1_impl_->AddHttpPatchHandler(uri, std::move(handler));
}
void GrpcDynamicMockupServer::AddHttpPostHandler(absl::string_view uri,
                                                 HandlerFunc handler) {
  redfish_v1_impl_->AddHttpPostHandler(uri, std::move(handler));
}

}  // namespace ecclesia
