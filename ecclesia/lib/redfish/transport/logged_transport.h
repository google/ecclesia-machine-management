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

#ifndef ECCLESIA_LIB_REDFISH_TRANSPORT_LOGGED_TRANSPORT_H_
#define ECCLESIA_LIB_REDFISH_TRANSPORT_LOGGED_TRANSPORT_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ecclesia/lib/redfish/transport/interface.h"

namespace ecclesia {

// Decorates RedfishTransport with verbose LOG printing.
class RedfishLoggedTransport : public RedfishTransport {
 public:
  explicit RedfishLoggedTransport(std::unique_ptr<RedfishTransport> base)
      : base_transport_(std::move(base)) {}

  explicit RedfishLoggedTransport(std::unique_ptr<RedfishTransport> base,
                                  absl::string_view context, bool log_payload,
                                  bool log_latency)
      : base_transport_(std::move(base)),
        context_(context),
        log_payload_(log_payload),
        log_latency_(log_latency) {}

  absl::string_view GetRootUri() override;
  absl::StatusOr<Result> Get(absl::string_view path) override;
  absl::StatusOr<Result> Get(absl::string_view path,
                             absl::Duration timeout) override;
  absl::StatusOr<Result> Post(absl::string_view path,
                              absl::string_view data) override;
  absl::StatusOr<Result> Patch(absl::string_view path,
                               absl::string_view data) override;
  absl::StatusOr<Result> Delete(absl::string_view path,
                                absl::string_view data) override;
  absl::StatusOr<Result> Put(absl::string_view path,
                             absl::string_view data) override;

 private:
  std::unique_ptr<RedfishTransport> base_transport_;
  std::optional<std::string> context_;
  bool log_payload_ = false;
  bool log_latency_ = true;

  absl::StatusOr<Result> LogMethodDataAndResult(
      absl::string_view method, absl::string_view path,
      std::optional<absl::string_view> data,
      absl::AnyInvocable<absl::StatusOr<Result>()> call_method) const;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_TRANSPORT_LOGGED_TRANSPORT_H_
