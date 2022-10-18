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

#include "ecclesia/lib/redfish/transport/logged_transport.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "ecclesia/lib/redfish/utils.h"

namespace ecclesia {
namespace {

std::string PrintResult(const nlohmann::json &result) {
  return JsonToString(result, /*indent=*/1);
}
std::string PrintResult(const RedfishTransport::bytes &result) {
  return "<bytes output>";
}

}  // namespace

absl::string_view RedfishLoggedTransport::GetRootUri() {
  CHECK(base_transport_ != nullptr);
  return base_transport_->GetRootUri();
}

absl::StatusOr<RedfishTransport::Result> RedfishLoggedTransport::Get(
    absl::string_view path) {
  CHECK(base_transport_ != nullptr);
  auto result = base_transport_->Get(path);
  if (result.ok()) {
    LOG(INFO) << absl::StrFormat(
        "Get(path=%s): %s", path,
        std::visit([&](const auto &body) { return PrintResult(body); },
                   result->body));
  } else {
    LOG(ERROR) << absl::StrFormat("Get(path=%s): %s", path,
                                  result.status().message());
  }
  return result;
}
absl::StatusOr<RedfishTransport::Result> RedfishLoggedTransport::Post(
    absl::string_view path, absl::string_view data) {
  CHECK(base_transport_ != nullptr);
  auto result = base_transport_->Post(path, data);
  if (result.ok()) {
    LOG(INFO) << absl::StrFormat(
        "Post(path=%s, data=%s): %s", path, data,
        std::visit([&](const auto &body) { return PrintResult(body); },
                   result->body));
  } else {
    LOG(ERROR) << absl::StrFormat("Post(path=%s, data=%s): %s", path, data,
                                  result.status().message());
  }
  return result;
}
absl::StatusOr<RedfishTransport::Result> RedfishLoggedTransport::Patch(
    absl::string_view path, absl::string_view data) {
  CHECK(base_transport_ != nullptr);
  auto result = base_transport_->Patch(path, data);
  if (result.ok()) {
    LOG(INFO) << absl::StrFormat(
        "Patch(path=%s, data=%s): %s", path, data,
        std::visit([&](const auto &body) { return PrintResult(body); },
                   result->body));
  } else {
    LOG(ERROR) << absl::StrFormat("Patch(path=%s, data=%s): %s", path, data,
                                  result.status().message());
  }
  return result;
}
absl::StatusOr<RedfishTransport::Result> RedfishLoggedTransport::Delete(
    absl::string_view path, absl::string_view data) {
  CHECK(base_transport_ != nullptr);
  auto result = base_transport_->Delete(path, data);
  if (result.ok()) {
    LOG(INFO) << absl::StrFormat(
        "Delete(path=%s, data=%s): %s", path, data,
        std::visit([&](const auto &body) { return PrintResult(body); },
                   result->body));
  } else {
    LOG(ERROR) << absl::StrFormat("Delete(path=%s, data=%s): %s", path, data,
                                  result.status().message());
  }
  return result;
}
}  // namespace ecclesia
