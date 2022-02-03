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

#ifndef ECCLESIA_LIB_REDFISH_TRANSPORT_GRPC_DYNAMIC_IMPL_H_
#define ECCLESIA_LIB_REDFISH_TRANSPORT_GRPC_DYNAMIC_IMPL_H_

#include <memory>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.grpc.pb.h"
#include "ecclesia/lib/redfish/transport/cache.h"
#include "ecclesia/lib/redfish/transport/grpc_dynamic_options.h"
#include "ecclesia/lib/redfish/transport/interface.h"

namespace ecclesia {

// The gRPC based implementation. The gRPC Redfish Service is based on Struct, a
// dynamically typed proto object.
// GrpcDynamicImpl is thread-safe.
class GrpcDynamicImpl : public RedfishInterface {
 public:
  struct Target {
    std::string fqdn = "[::1]";
    int port = 0;
  };

  GrpcDynamicImpl(const Target& target, TrustedEndpoint trusted,
                  const GrpcDynamicImplOptions& options);

  // Not copyable or movable
  GrpcDynamicImpl(GrpcDynamicImpl&& other) = delete;
  GrpcDynamicImpl& operator=(GrpcDynamicImpl&& other) = delete;
  GrpcDynamicImpl(const GrpcDynamicImpl&) = delete;
  GrpcDynamicImpl& operator=(const GrpcDynamicImpl&) = delete;

  ABSL_DEPRECATED(
      "Create a new instance instead rather than update the endpoint")
  // Note: program calling this function will always crash.
  void UpdateTransport(std::unique_ptr<RedfishTransport> new_transport,
                       std::unique_ptr<RedfishCachedGetterInterface> new_cache,
                       TrustedEndpoint trusted) override {
    Check(false, "Create a new instance instead");
  }

  // Returns whether the endpoint is trusted.
  bool IsTrusted() const override;

  // Fetches the root payload and returns it.
  RedfishVariant GetRoot(GetParams params) override;

  // Fetches the given URI and returns it.
  RedfishVariant CachedGetUri(absl::string_view uri, GetParams params) override;
  RedfishVariant UncachedGetUri(absl::string_view uri,
                                GetParams params) override;

  // Post to the given URI and returns result.
  RedfishVariant PostUri(
      absl::string_view uri,
      absl::Span<const std::pair<std::string, ValueVariant>> kv_span) override;

  // Post to the given URI and returns result.
  // Note: program calling this function will always crash.
  ABSL_DEPRECATED("Use the kv_span version instead")
  RedfishVariant PostUri(absl::string_view uri,
                         absl::string_view data) override {
    Check(false, "Use the kv_span version instead");
    return RedfishVariant(absl::UnimplementedError(
        "Not implemented: use the kv_span version of PostUri instead."));
  }

  // Patch the given URI and returns result.
  RedfishVariant PatchUri(
      absl::string_view uri,
      absl::Span<const std::pair<std::string, ValueVariant>> kv_span) override;

  // Note: program calling this function will always crash.
  ABSL_DEPRECATED("Use the kv_span version instead")
  RedfishVariant PatchUri(absl::string_view uri,
                          absl::string_view data) override {
    Check(false, "Use the kv_span version instead");
    return RedfishVariant(absl::UnimplementedError(
        "Not implemented: use the kv_span version of PostUri instead."));
  }

 private:
  GrpcDynamicImplOptions options_;
  Target target_;
  // The stub is thread-safe and should be reused re-used for concurrent RPCs.
  std::unique_ptr<::redfish::v1::RedfishV1::Stub> stub_;
  TrustedEndpoint trusted_;
  ServiceRootUri service_root_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_TRANSPORT_GRPC_DYNAMIC_IMPL_H_
