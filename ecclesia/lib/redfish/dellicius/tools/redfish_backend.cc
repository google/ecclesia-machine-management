/*
 * Copyright 2023 Google LLC
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

#include "ecclesia/lib/redfish/dellicius/tools/redfish_backend.h"

#include <memory>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/http/cred.pb.h"
#include "ecclesia/lib/http/curl_client.h"
#include "ecclesia/lib/redfish/transport/grpc.h"
#include "ecclesia/lib/redfish/transport/grpc_tls_options.h"
#include "ecclesia/lib/redfish/transport/http.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "grpcpp/security/tls_certificate_verifier.h"

namespace ecclesia {
namespace {

constexpr absl::string_view kDnsPrefix = "dns:///";
constexpr absl::string_view kLocalhost = "localhost";

enum class RedfishTransportType {
  kUnknown = 0,
  kHttp,
  kLoasGrpc,
  kInsecureGrpc,
  kMtlsGrpc,
};

RedfishTransportType StringToRedfishTransportType(absl::string_view type) {
  if (absl::EqualsIgnoreCase(type, "http")) {
    return RedfishTransportType::kHttp;
  }
  if (absl::EqualsIgnoreCase(type, "loas_grpc")) {
    return RedfishTransportType::kLoasGrpc;
  }
  if (absl::EqualsIgnoreCase(type, "insecure_grpc")) {
    return RedfishTransportType::kInsecureGrpc;
  }
  if (absl::EqualsIgnoreCase(type, "mtls_grpc")) {
    return RedfishTransportType::kMtlsGrpc;
  }
  return RedfishTransportType::kUnknown;
}

}  // namespace

absl::StatusOr<std::unique_ptr<ecclesia::RedfishTransport>>
CreateRedfishTransport(const RedfishTransportConfig& config) {
  RedfishTransportType redfish_backend =
      StringToRedfishTransportType(config.type);

  switch (redfish_backend) {
    case RedfishTransportType::kInsecureGrpc: {
      ecclesia::StaticBufferBasedTlsOptions grpc_options;
      grpc_options.SetToInsecure();
      return ecclesia::CreateGrpcRedfishTransport(
          config.GetTarget(), ecclesia::GrpcTransportParams(),
          grpc_options.GetChannelCredentials());
    } break;
    case RedfishTransportType::kMtlsGrpc: {
      return absl::UnimplementedError(
        "MTLS based credentials is not available");
    } break;
    case RedfishTransportType::kLoasGrpc: {
      return absl::UnimplementedError(
          "Loas based credentials is not available");
    } break;
    case RedfishTransportType::kHttp:
      return HttpRedfishTransport::MakeNetwork(
          std::make_unique<CurlHttpClient>(LibCurlProxy::CreateInstance(),
                                           HttpCredential()),
          config.GetTarget());
    case RedfishTransportType::kUnknown:
      return absl::InternalError(
          absl::StrCat("Unknown transport type: ", config.type));
  }

  return absl::InternalError(
      absl::StrCat("Unknown transport type: ", config.type));
}

}  // namespace ecclesia
