/*
 * Copyright 2025 Google LLC
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

#ifndef ECCLESIA_LIB_REDFISH_TRANSPORT_FAKE_INTERFACE_H_
#define ECCLESIA_LIB_REDFISH_TRANSPORT_FAKE_INTERFACE_H_

#include <memory>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ecclesia/lib/redfish/transport/interface.h"

namespace ecclesia {

// FakeRedfishTransport is a RedfishTransport that can be used to simulate
// failures at the Redfish transport layer.
//
//   FakeRedfishTransport transport(std::move(options),
//                                  std::make_unique<FakeRedfishTransport>(
//                                      FakeRedfishTransport::Options(),
//                                      /*base=*/nullptr));

class FakeRedfishTransport : public RedfishTransport {
 public:
  using GetCallback =
      absl::AnyInvocable<bool(absl::string_view, absl::StatusOr<Result>&)>;

  FakeRedfishTransport(std::unique_ptr<RedfishTransport> base,
                       GetCallback override_callback)
      : base_transport_(std::move(base)),
        override_callback_(std::move(override_callback)) {}

  absl::string_view GetRootUri() override {
    return base_transport_->GetRootUri();
  };

  absl::StatusOr<Result> Get(absl::string_view path) override;

  absl::StatusOr<Result> Get(absl::string_view path,
                             absl::Duration timeout) override;

  absl::StatusOr<Result> Post(absl::string_view path,
                              absl::string_view data) override {
    return base_transport_->Post(path, data);
  }

  absl::StatusOr<Result> Patch(absl::string_view path,
                               absl::string_view data) override {
    return base_transport_->Patch(path, data);
  }

  absl::StatusOr<Result> Delete(absl::string_view path,
                                absl::string_view data) override {
    return base_transport_->Delete(path, data);
  }

  absl::StatusOr<Result> Put(absl::string_view path,
                             absl::string_view data) override {
    return base_transport_->Put(path, data);
  }

 private:
  std::unique_ptr<RedfishTransport> base_transport_;
  absl::AnyInvocable<bool(absl::string_view, absl::StatusOr<Result>&)>
      override_callback_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_TRANSPORT_FAKE_INTERFACE_H_
