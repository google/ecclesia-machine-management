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

#include "ecclesia/lib/redfish/transport/grpc_dynamic_impl.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "google/protobuf/struct.pb.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.grpc.pb.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.pb.h"
#include "ecclesia/lib/redfish/transport/grpc_dynamic_options.h"
#include "ecclesia/lib/redfish/transport/proto_variant.h"
#include "ecclesia/lib/status/rpc.h"
#include "grpcpp/client_context.h"
#include "grpcpp/create_channel.h"
#include "grpcpp/security/auth_context.h"
#include "grpcpp/security/credentials.h"
#include "grpcpp/support/time.h"  // IWYU pragma: keep

namespace ecclesia {

namespace {

using ::google::protobuf::Struct;
using ::google::protobuf::Value;
using ::libredfish::RedfishInterface;
using ::libredfish::RedfishVariant;
using ::redfish::v1::RedfishV1;
using ::redfish::v1::Request;
using ::redfish::v1::Response;

constexpr absl::string_view kTargetKey = "target";
constexpr absl::string_view kResourceKey = "redfish-resource";

RedfishVariant GetRedfishVariant(absl::Status status, Response response) {
  if (!status.ok()) {
    return RedfishVariant(status);
  }
  Value value;
  value.set_allocated_struct_value(new Struct(response.message()));
  return RedfishVariant(absl::make_unique<ProtoVariantImpl>(value));
}

template <typename Rpc>
libredfish::RedfishVariant DoRpc(
    absl::string_view uri, absl::optional<google::protobuf::Struct> message,
    const GrpcDynamicImplOptions& options, Rpc rpc) {
  Request request;
  request.set_url(std::string(uri));
  if (message.has_value()) {
    request.set_allocated_message(new Struct(std::move(*message)));
  }
  grpc::ClientContext context;
  context.set_deadline(absl::ToChronoTime(absl::Now() + options.GetTimeout()));

  Response response;
  grpc::Status status = rpc(context, request, &response);
  return GetRedfishVariant(AsAbslStatus(status), std::move(response));
}

template <class... Ts>
struct Overloaded : Ts... {
  using Ts::operator()...;
};

template <class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

Struct GetRequestMessage(
    absl::Span<const std::pair<std::string, RedfishInterface::ValueVariant>>
        kv_span) {
  Struct message;
  for (const auto& [key, value_variant] : kv_span) {
    message.mutable_fields()->insert(
        {key, absl::visit(Overloaded{[](int variant) {
                                       Value val;
                                       val.set_number_value(variant);
                                       return val;
                                     },
                                     [](double variant) {
                                       Value val;
                                       val.set_number_value(variant);
                                       return val;
                                     },
                                     [](bool variant) {
                                       Value val;
                                       val.set_bool_value(variant);
                                       return val;
                                     },
                                     [](const std::string& variant) {
                                       Value val;
                                       val.set_string_value(variant);
                                       return val;
                                     },
                                     [](const char* variant) {
                                       Value val;
                                       val.set_string_value(variant);
                                       return val;
                                     }},
                          value_variant)});
  }
  return message;
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
}  // namespace

bool GrpcDynamicImpl::IsTrusted() const { return trusted_ == kTrusted; }

libredfish::RedfishVariant GrpcDynamicImpl::GetRoot(GetParams params) {
  if (service_root_ == libredfish::ServiceRoot::kGoogle) {
    return GetUri(kGoogleServiceRoot, params);
  }
  return GetUri(kServiceRoot, params);
}

libredfish::RedfishVariant GrpcDynamicImpl::GetUri(absl::string_view uri,
                                                   GetParams params) {
  return DoRpc(
      uri, absl::nullopt, options_,
      [this, uri](grpc::ClientContext& context, const Request& request,
                  Response* response) -> grpc::Status {
        context.set_credentials(grpc::MetadataCredentialsFromPlugin(
            std::unique_ptr<grpc::MetadataCredentialsPlugin>(
                new GrpcRedfishCredentials(target_.fqdn, uri.data()))));
        return stub_->Get(&context, request, response);
      });
}

libredfish::RedfishVariant GrpcDynamicImpl::PostUri(
    absl::string_view uri,
    absl::Span<const std::pair<std::string, ValueVariant>> kv_span) {
  return DoRpc(
      uri, GetRequestMessage(kv_span), options_,
      [this, uri](grpc::ClientContext& context, const Request& request,
                  Response* response) -> grpc::Status {
        context.set_credentials(grpc::MetadataCredentialsFromPlugin(
            std::unique_ptr<grpc::MetadataCredentialsPlugin>(
                new GrpcRedfishCredentials(target_.fqdn, uri.data()))));
        return stub_->Post(&context, request, response);
      });
}

libredfish::RedfishVariant GrpcDynamicImpl::PatchUri(
    absl::string_view uri,
    absl::Span<const std::pair<std::string, ValueVariant>> kv_span) {
  return DoRpc(
      uri, GetRequestMessage(kv_span), options_,
      [this, uri](grpc::ClientContext& context, const Request& request,
                  Response* response) -> grpc::Status {
        context.set_credentials(grpc::MetadataCredentialsFromPlugin(
            std::unique_ptr<grpc::MetadataCredentialsPlugin>(
                new GrpcRedfishCredentials(target_.fqdn, uri.data()))));
        return stub_->Patch(&context, request, response);
      });
}

GrpcDynamicImpl::GrpcDynamicImpl(const Target& target,
                                 RedfishInterface::TrustedEndpoint trusted,
                                 const GrpcDynamicImplOptions& options)
    : options_(options),
      target_(target),
      stub_(RedfishV1::NewStub(
          grpc::CreateChannel(absl::StrCat(target_.fqdn, ":", target_.port),
                              options_.GetChannelCredentials()))),
      trusted_(trusted),
      service_root_(libredfish::ServiceRoot::kRedfish) {}

}  // namespace ecclesia
