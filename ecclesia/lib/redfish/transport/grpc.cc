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
#include <chrono>  // NOLINT We have to chromo to make upstream code compile
#include <climits>
#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>

#include "google/protobuf/struct.pb.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.grpc.pb.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.pb.h"
#include "ecclesia/lib/redfish/proto/redfish_v1_grpc_include.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/redfish/transport/struct_proto_conversion.h"
#include "ecclesia/lib/redfish/utils.h"
#include "ecclesia/lib/status/macros.h"
#include "ecclesia/lib/status/rpc.h"
#include "ecclesia/lib/time/clock.h"
#include "grpc/grpc.h"
#include "grpc/grpc_security_constants.h"
#include "grpcpp/client_context.h"
#include "grpcpp/create_channel.h"
#include "grpcpp/security/auth_context.h"
#include "grpcpp/security/credentials.h"
#include "grpcpp/support/channel_arguments.h"
#include "grpcpp/support/client_callback.h"
#include "grpcpp/support/config.h"
#include "grpcpp/support/status.h"
#include "grpcpp/support/string_ref.h"
#include "single_include/nlohmann/json.hpp"
#include "google/protobuf/util/json_util.h"

namespace ecclesia {
namespace {

constexpr absl::string_view kTargetKey = "target";
constexpr absl::string_view kResourceKey = "redfish-resource";
// Both field name of the Authorization header and the token type are case
// insensitive
// References
// https://www.rfc-editor.org/rfc/rfc7230#section-3.2
// https://www.rfc-editor.org/rfc/rfc6749#section-5.1
constexpr absl::string_view kHostHeader = "Host";

struct RequestBody {
  std::optional<absl::string_view> json_str = std::nullopt;
  bool octet_stream = false;
  absl::Span<const std::pair<std::string, std::string>> headers;
};

constexpr RequestBody kNullRequestBody = RequestBody();

template <typename RpcFunc>
absl::StatusOr<RedfishTransport::Result> DoRpc(absl::string_view path,
                                               const RequestBody& body,
                                               absl::string_view target_fqdn,
                                               GrpcTransportParams params,
                                               RpcFunc rpc) {
  redfish::v1::Request request;
  // This header is used when authorizing peers without trust bundle.
  request.mutable_headers()->insert(
      {std::string(kHostHeader), std::string(target_fqdn)});
  for (const auto& [key, value] : body.headers) {
    request.mutable_headers()->insert({key, value});
  }
  request.set_url(std::string(path));
  if (params.max_age != absl::InfiniteDuration()) {
    request.mutable_headers()->insert(
        {"BMCWEB_HINT_MAX_AGE_SEC",
         absl::StrFormat("%d", absl::ToInt64Seconds(params.max_age))});
  }
  if (body.json_str && !body.json_str->empty()) {
    if (body.octet_stream) {
      *request.mutable_octet_stream() = *body.json_str;
    } else {
      *request.mutable_json_str() = *body.json_str;
      // to JSON str.
      ::google::protobuf::Struct request_body;
      if (auto status = google::protobuf::util::JsonStringToMessage(
              std::string(*body.json_str), &request_body,
              google::protobuf::util::JsonParseOptions());
          !status.ok()) {
        // We don't want to fail the request if the JSON string cannot be parsed
        // to a protobuf Struct as it can be an opaque binary blob.
        LOG(WARNING) << "Failed to parse JSON string to message: " << status;
      }

      *request.mutable_json() = request_body;
    }
  }
  grpc::ClientContext context;
  context.set_deadline(
      absl::ToChronoTime(params.clock->Now() + params.timeout));

  ::redfish::v1::Response response;
  if (grpc::Status status = rpc(context, request, &response); !status.ok()) {
    return AsAbslStatus(status);
  }

  RedfishTransport::Result ret_result;
  if (response.has_json()) {
    ret_result.body = StructToJson(response.json());
  } else if (response.has_octet_stream()) {
    ret_result.body = GetBytesFromString(response.octet_stream());
  } else if (response.has_json_str()) {
    ret_result.body =
        nlohmann::json::parse(response.json_str(), nullptr, false);
  }
  ret_result.code = static_cast<int>(response.code());
  ret_result.headers = absl::flat_hash_map<std::string, std::string>(
      response.headers().begin(), response.headers().end());
  return ret_result;
}

// Input could be a tcp_endpoint or a uds_endpoint.
// endpoint: e.g. "dns:///localhost:80", "unix:///var/run/my.socket"
absl::string_view EndpointToFqdn(absl::string_view endpoint) {
  if (absl::StrContains(endpoint, "unix:")) {
    return endpoint;
  }
  if (absl::StrContains(endpoint, "dns:/")) {
    size_t port_pos = endpoint.find_last_of(':');
    size_t slash_pos = endpoint.find_last_of('/');
    return endpoint.substr(slash_pos + 1, port_pos - slash_pos - 1);
  }
  if (absl::StrContains(endpoint, "google:/")) {
    size_t slash_pos = endpoint.find_last_of('/');
    return endpoint.substr(slash_pos + 1, endpoint.size() - slash_pos - 1);
  }
  size_t port_pos = endpoint.find_last_of(':');
  return endpoint.substr(0, port_pos);
}

// NOTE: this class is only kept for backward compatibility purpose.
// DO NOT ADD MORE METADATA.
// Use HTTP headers instead.
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

class GrpcRedfishSubscribeReactor
    : public grpc::ClientReadReactor<::redfish::v1::Response> {
 public:
  GrpcRedfishSubscribeReactor(grpc::ClientContext& context,
                              GrpcRedfishV1::Stub& stub,
                              const redfish::v1::Request& request,
                              RedfishTransport::EventCallback&& on_event,
                              RedfishTransport::StopCallback&& on_stop)
      : request_(request),
        on_event_(std::move(on_event)),
        on_stop_(std::move(on_stop)) {
    DLOG(INFO) << "Calling Subscribe!";
    stub.async()->Subscribe(&context, &request_, this);
    DLOG(INFO) << "Start reading!";
    StartRead(&event_);
  }

  // NOLINT See
  // https://github.com/grpc/proposal/blob/master/L67-cpp-callback-api.md#reactor-model
  // The framework and API guarantees that there is only one |OnReadDone| will
  // be executed at a given time.
  void OnReadDone(bool ok) override {
    DLOG(INFO) << "OnReadDone invoked!";
    if (!ok) {
      // no new read will be called
      LOG(WARNING) << "The previous event read failed!";
      return;
    }
    RedfishTransport::Result result;

    // Every Redfish event shall have the corresponding HTTP header implemented
    // including:
    //  1. payload type, octet_stream, json_str, etc;
    //  2. subscribtion ID
    if (event_.has_octet_stream()) {
      result.body = GetBytesFromString(event_.octet_stream());
    } else if (event_.has_json_str()) {
      result.body = nlohmann::json::parse(event_.json_str(), nullptr, false);
    }
    result.code = static_cast<int>(event_.code());
    DLOG(INFO) << "Executing EventCallback.";
    on_event_(result);
    StartRead(&event_);
  }

  // OnDone will always take place after all other reactions.
  void OnDone(const grpc::Status& status) override {
    DLOG(INFO) << "OnDone invoked! Executing StopCallback.";
    on_stop_(AsAbslStatus(status));
  }

 private:
  redfish::v1::Response event_;
  redfish::v1::Request request_;
  RedfishTransport::EventCallback on_event_;
  RedfishTransport::StopCallback on_stop_;
};

class GrpcRedfishEventStream : public RedfishEventStream {
 public:
  GrpcRedfishEventStream(GrpcRedfishV1::Stub& stub,
                         const redfish::v1::Request& request,
                         RedfishTransport::EventCallback&& on_event,
                         RedfishTransport::StopCallback&& on_stop)
      : reactor_(std::make_unique<GrpcRedfishSubscribeReactor>(
            context_, stub, request, std::move(on_event), std::move(on_stop))) {
  }

  void StartStreaming() override {
    DLOG(INFO) << "Start streaming!";
    reactor_->StartCall();
  }
  void CancelStreaming() override {
    // that a subscription is cancelled. A possible implementation is DELETE on
    // corresponding EventDestination.
    context_.TryCancel();
  }

  ~GrpcRedfishEventStream() override { context_.TryCancel(); };

 private:
  grpc::ClientContext context_;
  std::unique_ptr<GrpcRedfishSubscribeReactor> reactor_;
};

class GrpcRedfishTransport : public RedfishTransport {
 public:
  // Creates a GrpcRedfishTransport using a specified endpoint and customized
  // credentials.
  // Params:
  //   endpoint: e.g. "dns:///localhost:80", "unix:///var/run/my.socket"
  GrpcRedfishTransport(absl::string_view endpoint,
                       const GrpcTransportParams& params,
                       const std::shared_ptr<grpc::Channel>& channel)
      : client_(GrpcRedfishV1::NewStub(channel)),
        params_(params),
        fqdn_(EndpointToFqdn(endpoint)) {}

  explicit GrpcRedfishTransport(absl::string_view endpoint)
      : client_(GrpcRedfishV1::NewStub(grpc::CreateChannel(
            std::string(endpoint), grpc::InsecureChannelCredentials()))),
        params_({}),
        fqdn_(EndpointToFqdn(endpoint)) {}

  ~GrpcRedfishTransport() override = default;

  // Returns the path of the root URI for the Redfish service this transport is
  // connected to.
  absl::string_view GetRootUri() override {
    return RedfishInterface::ServiceRootToUri(ServiceRootUri::kRedfish);
  }

  absl::StatusOr<Result> Get(absl::string_view path) override {
    return DoRpc(
        path, kNullRequestBody, fqdn_, params_,
        [this, path](grpc::ClientContext& context,
                     const redfish::v1::Request& request,
                     ::redfish::v1::Response* response) -> grpc::Status {
          context.set_credentials(
              grpc::experimental::MetadataCredentialsFromPlugin(
                  std::unique_ptr<grpc::MetadataCredentialsPlugin>(
                      std::make_unique<GrpcRedfishCredentials>(fqdn_, path)),
                  GRPC_SECURITY_NONE));
          return client_->Get(&context, request, response);
        });
  }

  absl::StatusOr<Result> Get(absl::string_view path,
                             absl::Duration timeout) override {
    if (timeout <= absl::ZeroDuration()) {
      return absl::DeadlineExceededError(
          "timeout was already exceeded when Get was called");
    }
    if (timeout == absl::InfiniteDuration()) {
      return absl::InvalidArgumentError(
          "Timeout for GET request cannot be infinite");
    }
    return DoRpc(
        path, kNullRequestBody, fqdn_, params_,
        [this, path, timeout](
            grpc::ClientContext& context, const redfish::v1::Request& request,
            ::redfish::v1::Response* response) -> grpc::Status {
          // Use timeout if set.
          context.set_deadline(
              absl::ToChronoTime(params_.clock->Now() + timeout));
          context.set_credentials(
              grpc::experimental::MetadataCredentialsFromPlugin(
                  std::unique_ptr<grpc::MetadataCredentialsPlugin>(
                      std::make_unique<GrpcRedfishCredentials>(fqdn_, path)),
                  GRPC_SECURITY_NONE));
          return client_->Get(&context, request, response);
        });
  }

  absl::StatusOr<Result> Post(absl::string_view path,
                              absl::string_view data) override {
    return Post(path, data, /*octet_stream=*/false, params_.timeout, {});
  }

  absl::StatusOr<Result> Post(
      absl::string_view path, absl::string_view data, bool octet_stream,
      absl::Duration timeout,
      absl::Span<const std::pair<std::string, std::string>> headers) override {
    return DoRpc(
        path,
        RequestBody{
            .json_str = data, .octet_stream = octet_stream, .headers = headers},
        fqdn_, params_,
        [this, path, timeout](
            grpc::ClientContext& context, const redfish::v1::Request& request,
            ::redfish::v1::Response* response) -> grpc::Status {
          context.set_deadline(
              absl::ToChronoTime(params_.clock->Now() + timeout));
          context.set_credentials(
              grpc::experimental::MetadataCredentialsFromPlugin(
                  std::unique_ptr<grpc::MetadataCredentialsPlugin>(
                      std::make_unique<GrpcRedfishCredentials>(fqdn_, path)),
                  GRPC_SECURITY_NONE));
          return client_->Post(&context, request, response);
        });
  }

  absl::StatusOr<Result> Patch(absl::string_view path,
                               absl::string_view data) override {
    return DoRpc(
        path, RequestBody{.json_str = data}, fqdn_, params_,
        [this, path](grpc::ClientContext& context,
                     const redfish::v1::Request& request,
                     ::redfish::v1::Response* response) -> grpc::Status {
          context.set_credentials(
              grpc::experimental::MetadataCredentialsFromPlugin(
                  std::unique_ptr<grpc::MetadataCredentialsPlugin>(
                      std::make_unique<GrpcRedfishCredentials>(fqdn_, path)),
                  GRPC_SECURITY_NONE));
          return client_->Patch(&context, request, response);
        });
  }
  absl::StatusOr<Result> Delete(absl::string_view path,
                                absl::string_view data) override {
    return DoRpc(
        path, RequestBody{.json_str = data}, fqdn_, params_,
        [this, path](grpc::ClientContext& context,
                     const redfish::v1::Request& request,
                     ::redfish::v1::Response* response) -> grpc::Status {
          context.set_credentials(
              grpc::experimental::MetadataCredentialsFromPlugin(
                  std::unique_ptr<grpc::MetadataCredentialsPlugin>(
                      std::make_unique<GrpcRedfishCredentials>(fqdn_, path)),
                  GRPC_SECURITY_NONE));
          return client_->Delete(&context, request, response);
        });
  }

  absl::StatusOr<Result> Put(absl::string_view path,
                             absl::string_view data) override {
    return DoRpc(
        path, RequestBody{.json_str = data}, fqdn_, params_,
        [this, path](grpc::ClientContext& context,
                     const redfish::v1::Request& request,
                     ::redfish::v1::Response* response) -> grpc::Status {
          context.set_credentials(
              grpc::experimental::MetadataCredentialsFromPlugin(
                  std::unique_ptr<grpc::MetadataCredentialsPlugin>(
                      std::make_unique<GrpcRedfishCredentials>(fqdn_, path)),
                  GRPC_SECURITY_NONE));
          return client_->Put(&context, request, response);
        });
  }

  absl::StatusOr<std::unique_ptr<RedfishEventStream>> Subscribe(
      absl::string_view data, EventCallback&& on_event,
      StopCallback&& on_stop) override {
    ::redfish::v1::Request request;
    // This header is used when authorizing peers without trust bundle.
    request.mutable_headers()->insert(
        {std::string(kHostHeader), std::string(fqdn_)});
    request.set_json_str(std::string(data));
    DLOG(INFO) << "Subscribe request to send: " << request.DebugString();
    return std::make_unique<GrpcRedfishEventStream>(
        *client_, request, std::move(on_event), std::move(on_stop));
  }

 private:
  std::unique_ptr<GrpcRedfishV1::Stub> client_;
  GrpcTransportParams params_;
  std::string fqdn_;
};

absl::Status ValidateEndpoint(absl::string_view endpoint) {
  if (absl::StartsWith(endpoint, "unix:")) {
    size_t pos = endpoint.find_last_of(':');
    if (pos != 4) {
      return absl::InvalidArgumentError(
          absl::StrCat("bad endpoint: ", endpoint, " ;no colons inside a uds"));
    }
  } else if (absl::StartsWith(endpoint, "google:")) {
    // Support Google resolver
    return absl::OkStatus();
  } else {
    size_t pos = endpoint.find_last_of(':');
    if (pos == absl::string_view::npos || pos + 1 == endpoint.size()) {
      return absl::InvalidArgumentError(
          absl::StrCat("bad endpoint: ", endpoint, " ;missing port in a dns"));
    }
    for (size_t i = pos + 1; i < endpoint.size(); ++i) {
      if (std::isdigit(endpoint[i]) == 0) {
        return absl::InvalidArgumentError(absl::StrCat(
            "bad endpoint: ", endpoint, " ;port should be an integer"));
      }
    }
  }
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<std::unique_ptr<RedfishTransport>> CreateGrpcRedfishTransport(
    absl::string_view endpoint, const GrpcTransportParams& params,
    const std::shared_ptr<grpc::ChannelCredentials>& creds) {
  ECCLESIA_RETURN_IF_ERROR(ValidateEndpoint(endpoint));

  // Set keepalive parameters according to default values of the OSS version.
  // Reference:
  // 1. OSS https://grpc.github.io/grpc/core/md_doc_keepalive.html
  // copybara:strip_begin(g3 only comments)
  // 2. /net/grpc
  // http://google3/net/grpc/public/src/core/surface/init.cc;l=121;rcl=625188626
  // copybara:strip_end
  grpc::ChannelArguments args;
  args.SetInt(GRPC_ARG_KEEPALIVE_TIME_MS, INT_MAX);
  args.SetInt(GRPC_ARG_KEEPALIVE_TIMEOUT_MS, 20 * 1000 /*20 sec*/);
  args.SetInt(GRPC_ARG_KEEPALIVE_PERMIT_WITHOUT_CALLS, 0);
  auto channel = grpc::CreateCustomChannel(std::string(endpoint), creds, args);

  std::optional<absl::Duration> timeout = params.wait_for_connected_timeout;
  if (timeout.has_value()) {
    std::chrono::system_clock::time_point deadline =
        absl::ToChronoTime(params.clock->Now() + *timeout);
    if (!channel->WaitForConnected(deadline)) {
      return absl::DeadlineExceededError(
          absl::StrCat("Channel did not become healthy within ",
                       absl::FormatDuration(*timeout)));
    }
  }
  return std::make_unique<GrpcRedfishTransport>(endpoint, params, channel);
}

}  // namespace ecclesia
