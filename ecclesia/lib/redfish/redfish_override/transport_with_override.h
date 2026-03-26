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
#include <string>
#include <utility>
#include <vector>

#include "absl/base/call_once.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
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

    SetOverridePolicyOnce(*std::move(override_policy));
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

  absl::StatusOr<Result> Put(absl::string_view path,
                             absl::string_view data) override {
    return redfish_transport_->Put(path, data);
  }

 private:
  // If we do not have an override, try to fetch it fresh before calling Get.
  absl::StatusOr<RedfishTransport::Result> TryApplyingOverride(
      RedfishTransport::Result result);

  // Set override policy and precompile the regexes.
  // This function is thread-safe, using a `call_once` to ensure the override
  // policy can only be set once.
  //
  // Because the state of `absl::once_flag` is not exposed, we use a
  // `absl::Notification` to indicate whether override policy has been
  // initialized, allowing the code to only invoke the `override_policy_cb_` if
  // we have not already fetched & successfully initialized the override policy.
  void SetOverridePolicyOnce(OverridePolicy policy);

  std::unique_ptr<RedfishTransport> redfish_transport_;

  absl::AnyInvocable<absl::StatusOr<OverridePolicy>()> override_policy_cb_;

  // Context for the override policy, including the policy itself and the
  // precompiled regexes.
  struct OverrideContext {
    OverridePolicy policy;
    std::vector<
        std::pair<std::unique_ptr<RE2>, OverridePolicy::OverrideContent>>
        re2_and_content;
  };

  // A note regarding thread-safety of the override policy:
  //
  // The override policy is initialized exactly once, either in the constructor
  // or in `TryApplyingOverride`. This is ensured by the `once_flag`.
  // `override_context_initialized_` is used to indicate whether override policy
  // has been initialized, allowing the code to only invoke the
  // `override_policy_cb_` if we have not already fetched & successfully
  // initialized the override policy.
  //
  // `override_context_initialized_` should also be checked to determine whether
  // `override_context_` is valid, not whether `override_context_` is null. This
  // is because the override context is lazy initialized. Doing it this way
  // allows us to avoid a potential race condition where the override context is
  // being updated while another thread is trying to access it.
  //
  // This is preferable to using a mutex because it allows us to make the act of
  // applying an override lock-free -- something that is valuable in a function
  // that is called in the critical path of a Redfish GET request.
  //
  // This is also preferable over the `ecclesia::RcuStore` because that RCU
  // store blocks with a mutex during read and also has additional CPU overhead
  // due to the metadata necessary to manage cache invalidation notifications &
  // snapshots.

  // Synchronization flag for lazy initialization of override policy.
  absl::once_flag override_initialized_once_flag_;
  // If notified, override policy has been initialized. Needed because an
  // `absl::once_flag` does not otherwise expose its state.
  absl::Notification override_context_initialized_;

  // Context for the override policy, including the policy itself and the
  // precompiled regexes. Only valid if `override_context_initialized_` is
  // notified. Checking if this is null is not a valid way to determine if
  // override policy has been initialized, as this would *not* be thread-safe.
  std::unique_ptr<OverrideContext> override_context_ = nullptr;
};
}  // namespace ecclesia
#endif  // ECCLESIA_LIB_REDFISH_REDFISH_OVERRIDE_TRANSPORT_WITH_OVERRIDE_H_
