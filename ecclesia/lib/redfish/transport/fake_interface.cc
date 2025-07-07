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

#include "ecclesia/lib/redfish/transport/fake_interface.h"

#include "absl/flags/flag.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ecclesia/lib/redfish/transport/interface.h"

ABSL_FLAG(bool, apply_overrides, true,
          "Apply overrides to the fake redfish transport.");

namespace ecclesia {

absl::StatusOr<RedfishTransport::Result> FakeRedfishTransport::Get(
    absl::string_view path) {
  absl::StatusOr<Result> result;
  if (override_callback_(path, result)) {
    return result;
  }

  return base_transport_->Get(path);
};

absl::StatusOr<RedfishTransport::Result> FakeRedfishTransport::Get(
    absl::string_view path, absl::Duration timeout) {
  absl::StatusOr<Result> result;
  if (override_callback_(path, result)) {
    return result;
  }

  return base_transport_->Get(path, timeout);
};

}  // namespace ecclesia
