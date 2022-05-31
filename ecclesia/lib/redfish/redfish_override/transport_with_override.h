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
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "ecclesia/lib/redfish/redfish_override/rf_override.pb.h"
#include "ecclesia/lib/redfish/transport/interface.h"

namespace ecclesia {

class RedfishTransportWithOverride : public RedfishTransport {
 public:
  RedfishTransportWithOverride(
      std::unique_ptr<RedfishTransport> redfish_transport,
      OverridePolicy override_policy)
      : redfish_transport_(std::move(redfish_transport)),
        override_policy_(std::move(override_policy)) {}

  ~RedfishTransportWithOverride() override = default;

  absl::string_view GetRootUri() override {
    return redfish_transport_->GetRootUri();
  }

  void ReloadOverridePolicy(OverridePolicy override_policy) {
    absl::MutexLock lock(&policy_lock_);
    override_policy_ = std::move(override_policy);
  }

  // The RedfishOverride may intercept the request and manipulate the response
  // from the underneath transport layer.
  absl::StatusOr<Result> Get(absl::string_view path) override;

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

  absl::StatusOr<Result> Patch(absl::string_view path,
                               absl::string_view data) override {
    return redfish_transport_->Patch(path, data);
  }

  absl::StatusOr<Result> Delete(absl::string_view path,
                                absl::string_view data) override {
    return redfish_transport_->Delete(path, data);
  }

 private:
  std::unique_ptr<RedfishTransport> redfish_transport_;

  absl::Mutex policy_lock_;
  OverridePolicy override_policy_ ABSL_GUARDED_BY(policy_lock_);
};

}  // namespace ecclesia
#endif  // ECCLESIA_LIB_REDFISH_REDFISH_OVERRIDE_TRANSPORT_WITH_OVERRIDE_H_
