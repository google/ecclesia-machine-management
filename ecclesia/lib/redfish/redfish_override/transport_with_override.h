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

#ifndef ECCLESIA_LIB_REDFISH_REDFISH_OVERRIDE_TRANSPORT_WITH_OVERRIDE_H_
#define ECCLESIA_LIB_REDFISH_REDFISH_OVERRIDE_TRANSPORT_WITH_OVERRIDE_H_

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ecclesia/lib/redfish/redfish_override/rf_override.pb.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "grpcpp/security/credentials.h"
#include "re2/re2.h"

namespace ecclesia {

// Returns the policy by reading file on the machine directly.
absl::StatusOr<OverridePolicy> TryGetOverridePolicy(
    absl::string_view policy_file_path);
// Returns the policy by BMC's hostname, port(optional) and a
// Chennel credential for gRPC. If this function fails to find an override
// policy, it'll return an empty policy with some warning log as no policy
// should not be a blocker.
// The `port` argument can be set as std::nullopt if it's not required.
// Otherwise, the target address will be "{hostname}:{port}."
absl::StatusOr<OverridePolicy> TryGetOverridePolicy(
    absl::string_view hostname, std::optional<int> port,
    const std::shared_ptr<grpc::ChannelCredentials>& creds);

// Same as TryGetOverridePolicy, except returns a default OverridePolicy on
// failure.
OverridePolicy GetOverridePolicy(absl::string_view policy_file_path);
OverridePolicy GetOverridePolicy(
    absl::string_view hostname, std::optional<int> port,
    const std::shared_ptr<grpc::ChannelCredentials>& creds);

class RedfishTransportWithOverride : public RedfishTransport {
 public:
  RedfishTransportWithOverride(
      std::unique_ptr<RedfishTransport> redfish_transport,
      absl::AnyInvocable<absl::StatusOr<OverridePolicy>()> override_policy_cb)
      : redfish_transport_(std::move(redfish_transport)),
        override_policy_cb_(std::move(override_policy_cb)) {
    absl::StatusOr<OverridePolicy> override_policy = override_policy_cb_();
    if (!override_policy.ok()) return;
    // Precompile regex for overrides.
    override_re2_and_content_.reserve(
        override_policy->override_content_map_regex_size());
    for (auto& [regex_str, override_content] :
         *override_policy->mutable_override_content_map_regex()) {
      override_re2_and_content_.push_back(std::make_pair(
          std::make_unique<RE2>(regex_str), std::move(override_content)));
    }
  }
  RedfishTransportWithOverride(
      std::unique_ptr<RedfishTransport> redfish_transport,
      OverridePolicy override_policy)
      : RedfishTransportWithOverride(
            std::move(redfish_transport),
            [override_policy = std::move(override_policy)]() {
              return override_policy;
            }) {}

  ~RedfishTransportWithOverride() override = default;

  absl::string_view GetRootUri() override {
    return redfish_transport_->GetRootUri();
  }

  // The RedfishOverride may intercept the request and manipulate the response
  // from the underneath transport layer.
  absl::StatusOr<Result> Get(absl::string_view path) override;

  // Same as Get(), but with a timeout to govern the base transport GET request.
  absl::StatusOr<Result> Get(absl::string_view path,
                             absl::Duration timeout) override;

  // A helper function to get the original response, i.e., without any override.
  absl::StatusOr<Result> GetOriginalResponse(absl::string_view path) {
    return redfish_transport_->Get(path);
  }

  // Passthrough the Post, Patch, Delete request to the underneath transport
  // without any override.
  absl::StatusOr<Result> Post(absl::string_view path,
                              absl::string_view data) override {
    return redfish_transport_->Post(path, data);
  }
  absl::StatusOr<Result> Post(
      absl::string_view path, absl::string_view data, bool octet_stream,
      absl::Duration timeout,
      absl::Span<const std::pair<std::string, std::string>> headers) override {
    return redfish_transport_->Post(path, data, octet_stream, timeout, headers);
  }

  absl::StatusOr<Result> Patch(absl::string_view path,
                               absl::string_view data) override {
    return redfish_transport_->Patch(path, data);
  }

  absl::StatusOr<Result> Delete(absl::string_view path,
                                absl::string_view data) override {
    return redfish_transport_->Delete(path, data);
  }

 private:
  // If we do not have an override, try to fetch it fresh before calling Get.
  absl::StatusOr<RedfishTransport::Result> TryApplyingOverride(
      absl::string_view path, RedfishTransport::Result result);

  std::unique_ptr<RedfishTransport> redfish_transport_;
  bool has_override_policy_ = false;
  OverridePolicy override_policy_;  // Use a default empty override policy.
  absl::AnyInvocable<absl::StatusOr<OverridePolicy>()> override_policy_cb_;
  std::vector<std::pair<std::unique_ptr<RE2>, OverridePolicy::OverrideContent>>
      override_re2_and_content_;
};
}  // namespace ecclesia
#endif  // ECCLESIA_LIB_REDFISH_REDFISH_OVERRIDE_TRANSPORT_WITH_OVERRIDE_H_
