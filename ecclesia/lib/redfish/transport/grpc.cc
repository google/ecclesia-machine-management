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

#include <cctype>
#include <cstddef>
#include <optional>

#include "google/protobuf/util/json_util.h"
#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "ecclesia/lib/http/codes.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.grpc.pb.h"
#include "ecclesia/lib/redfish/transport/grpc_dynamic_options.h"
#include "ecclesia/lib/redfish/transport/struct_proto_conversion.h"
#include "ecclesia/lib/status/macros.h"
#include "ecclesia/lib/status/rpc.h"
#include "grpcpp/client_context.h"
#include "grpcpp/create_channel.h"
#include "grpcpp/security/credentials.h"
#include "grpcpp/support/status.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {
namespace {

constexpr absl::string_view kTargetKey = "target";
constexpr absl::string_view kResourceKey = "redfish-resource";

template <typename RpcFunc>
absl::StatusOr<RedfishTransport::Result> DoRpc(
    absl::string_view path, std::optional<google::protobuf::Struct> message,
    GrpcTransportParams params, RpcFunc rpc) {
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
  ret_result.code = response.code();
  return ret_result;
}

// Input could be a tcp_endpoint or a uds_endpoint.
// endpoint: e.g. "dns:///localhost:80", "unix:///var/run/my.socket"
absl::string_view EndpointToFqdn(absl::string_view endpoint) {
  if (absl::StrContains(endpoint, "unix:")) {
    return endpoint;
  }
  size_t pos = endpoint.find_last_of(':');
  return endpoint.substr(0, pos);
}

class GrpcRedfishCredentials : public grpc::MetadataCredentialsPlugin {
 public:
  explicit GrpcRedfishCredentials(absl::string_view target_fqdn,
                                  absl::string_view resource)
      : target_fqdn_(target_fqdn), resource_(resource) {}
  // Sends out the target server and the Redfish resource as part of
  // gRPC credentials.
  grpc::Status GetMetadata(
      grpc::string_ref /*service_url*/, grpc::string_ref /*method_name*/,
      const grpc::AuthContext& /*channel_auth_context*/,
      std::multimap<grpc::string, grpc::string>* metadata) override {
    metadata->insert(std::make_pair(kTargetKey, target_fqdn_));
    metadata->insert(std::make_pair(kResourceKey, resource_));
    return grpc::Status::OK;
  }

 private:
  std::string target_fqdn_;
  std::string resource_;
};

class GrpcRedfishTransport : public RedfishTransport {
 public:
  // Creates an GrpcRedfishTransport using a specified endpoint.
  // Params:
  //   endpoint: e.g. "dns:///localhost:80", "unix:///var/run/my.socket"
  GrpcRedfishTransport(std::string_view endpoint,
                       const GrpcTransportParams& params,
                       const GrpcDynamicImplOptions& options,
                       ServiceRootUri service_root)
      : client_(redfish::v1::RedfishV1::NewStub(grpc::CreateChannel(
            std::string(endpoint), options.GetChannelCredentials()))),
        params_(std::move(params)),
        service_root_(std::move(service_root)),
        fqdn_(EndpointToFqdn(endpoint)) {}
  GrpcRedfishTransport(std::string_view endpoint)
      : client_(redfish::v1::RedfishV1::NewStub(grpc::CreateChannel(
            std::string(endpoint), grpc::InsecureChannelCredentials()))),
        params_({}),
        service_root_(std::move(ServiceRootUri::kRedfish)),
        fqdn_(EndpointToFqdn(endpoint)) {}

  ~GrpcRedfishTransport() override {}

  // Returns the path of the root URI for the Redfish service this transport is
  // connected to.
  absl::string_view GetRootUri() override {
    return RedfishInterface::ServiceRootToUri(service_root_);
  }

  absl::StatusOr<Result> Get(absl::string_view path)
      ABSL_LOCKS_EXCLUDED(mutex_) override {
    return DoRpc(
        path, std::nullopt, params_,
        [this, path](grpc::ClientContext& context,
                     const redfish::v1::Request& request,
                     ::redfish::v1::Response* response) -> grpc::Status {
          absl::ReaderMutexLock mu(&mutex_);
          context.set_credentials(grpc::MetadataCredentialsFromPlugin(
              std::unique_ptr<grpc::MetadataCredentialsPlugin>(
                  std::make_unique<GrpcRedfishCredentials>(fqdn_, path))));
          return client_->Get(&context, request, response);
        });
  }
  absl::StatusOr<Result> Post(absl::string_view path, absl::string_view data)
      ABSL_LOCKS_EXCLUDED(mutex_) override {
    ::google::protobuf::Struct request_body;
    ECCLESIA_RETURN_IF_ERROR(AsAbslStatus(google::protobuf::util::JsonStringToMessage(
        std::string(data), &request_body, google::protobuf::util::JsonParseOptions())));
    return DoRpc(
        path, std::move(request_body), params_,
        [this, path](grpc::ClientContext& context,
                     const redfish::v1::Request& request,
                     ::redfish::v1::Response* response) -> grpc::Status {
          absl::ReaderMutexLock mu(&mutex_);
          context.set_credentials(grpc::MetadataCredentialsFromPlugin(
              std::unique_ptr<grpc::MetadataCredentialsPlugin>(
                  std::make_unique<GrpcRedfishCredentials>(fqdn_, path))));
          return client_->Post(&context, request, response);
        });
  }
  absl::StatusOr<Result> Patch(absl::string_view path, absl::string_view data)
      ABSL_LOCKS_EXCLUDED(mutex_) override {
    ::google::protobuf::Struct request_body;
    ECCLESIA_RETURN_IF_ERROR(AsAbslStatus(google::protobuf::util::JsonStringToMessage(
        std::string(data), &request_body, google::protobuf::util::JsonParseOptions())));
    return DoRpc(
        path, std::move(request_body), params_,
        [this, path](grpc::ClientContext& context,
                     const redfish::v1::Request& request,
                     ::redfish::v1::Response* response) -> grpc::Status {
          absl::ReaderMutexLock mu(&mutex_);
          context.set_credentials(grpc::MetadataCredentialsFromPlugin(
              std::unique_ptr<grpc::MetadataCredentialsPlugin>(
                  std::make_unique<GrpcRedfishCredentials>(fqdn_, path))));
          return client_->Patch(&context, request, response);
        });
  }
  absl::StatusOr<Result> Delete(absl::string_view path, absl::string_view data)
      ABSL_LOCKS_EXCLUDED(mutex_) override {
    ::google::protobuf::Struct request_body;
    ECCLESIA_RETURN_IF_ERROR(AsAbslStatus(google::protobuf::util::JsonStringToMessage(
        std::string(data), &request_body, google::protobuf::util::JsonParseOptions())));
    return DoRpc(
        path, std::move(request_body), params_,
        [this, path](grpc::ClientContext& context,
                     const redfish::v1::Request& request,
                     ::redfish::v1::Response* response) -> grpc::Status {
          absl::ReaderMutexLock mu(&mutex_);
          context.set_credentials(grpc::MetadataCredentialsFromPlugin(
              std::unique_ptr<grpc::MetadataCredentialsPlugin>(
                  std::make_unique<GrpcRedfishCredentials>(fqdn_, path))));
          return client_->Delete(&context, request, response);
        });
  }

 private:
  absl::Mutex mutex_;
  std::unique_ptr<::redfish::v1::RedfishV1::Stub> client_
      ABSL_GUARDED_BY(mutex_);
  GrpcTransportParams params_;
  // The service root for RedfishInterface.
  const ServiceRootUri service_root_;
  std::string fqdn_;
};

}  // namespace

absl::StatusOr<std::unique_ptr<RedfishTransport>>
CreateGrpcRedfishTransport(absl::string_view endpoint,
                           const GrpcTransportParams& params,
                           const GrpcDynamicImplOptions& options,
                           ServiceRootUri service_root) {
  if (absl::StartsWith(endpoint, "unix:")) {
    size_t pos = endpoint.find_last_of(':');
    if (pos == 4) {
      return std::make_unique<GrpcRedfishTransport>(
          std::string(endpoint), params, options, service_root);
    }
    return absl::InvalidArgumentError(
        absl::StrCat("bad endpoint: ", endpoint, " ;no colons inside a uds"));
  } else {
    size_t pos = endpoint.find_last_of(':');
    if (pos == endpoint.npos)
      return absl::InvalidArgumentError(
          absl::StrCat("bad endpoint: ", endpoint, " ;missing port in a dns"));
    for (size_t i = pos + 1; i < endpoint.size(); ++i) {
      if (!std::isdigit(endpoint[i])) {
        return absl::InvalidArgumentError(absl::StrCat(
            "bad endpoint: ", endpoint, " ;port should be an integer"));
      }
    }
    return std::make_unique<GrpcRedfishTransport>(std::string(endpoint), params,
                                                  options, service_root);
  }
}

}  // namespace ecclesia
