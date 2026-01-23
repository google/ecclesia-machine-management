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
#include <optional>
#include <string>
#include <variant>

#include "absl/functional/any_invocable.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/redfish/utils.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {
namespace {

std::string PrintResult(const nlohmann::json& result) {
  return JsonToString(result, /*indent=*/1);
}
std::string PrintResult(const RedfishTransport::bytes& result) {
  return "<bytes output>";
}

}  // namespace

absl::StatusOr<RedfishLoggedTransport::Result>
RedfishLoggedTransport::LogMethodDataAndResult(
    absl::string_view method, absl::string_view path,
    std::optional<absl::string_view> data,
    absl::AnyInvocable<absl::StatusOr<Result>()> call_method) const {
  std::string message;
  if (context_.has_value()) {
    absl::StrAppend(&message, "(Context=", *context_, ") ");
  }

  absl::StrAppend(&message, "(Method=", method, ") ");
  absl::StrAppend(&message, "(Path=", path, ") ");

  if (data.has_value()) {
    absl::StrAppend(&message, "(Data=", *data, ") ");
  }

  absl::Time start = absl::Now();
  auto result = call_method();
  absl::Duration latency = absl::Now() - start;

  if (log_latency_) {
    absl::StrAppend(&message, "[Lag=", absl::FormatDuration(latency), "] ");
  }

  if (result.ok()) {
    LOG(INFO) << absl::StrCat(
        message,
        log_payload_
            ? std::visit([&](const auto& body) { return PrintResult(body); },
                         result->body)
            : " Success");
  } else {
    LOG(WARNING) << absl::StrCat(message,
                                 " Error: ", result.status().message());
  }

  return result;
}

absl::string_view RedfishLoggedTransport::GetRootUri() {
  CHECK(base_transport_ != nullptr);
  return base_transport_->GetRootUri();
}

absl::StatusOr<RedfishTransport::Result> RedfishLoggedTransport::Get(
    absl::string_view path) {
  CHECK(base_transport_ != nullptr);
  return LogMethodDataAndResult("Get", path, std::nullopt,
                                [&]() { return base_transport_->Get(path); });
}
absl::StatusOr<RedfishTransport::Result> RedfishLoggedTransport::Get(
    absl::string_view path, absl::Duration timeout) {
  CHECK(base_transport_ != nullptr);
  return LogMethodDataAndResult("Get", path, std::nullopt, [&]() {
    return base_transport_->Get(path, timeout);
  });
}

absl::StatusOr<RedfishTransport::Result> RedfishLoggedTransport::Post(
    absl::string_view path, absl::string_view data) {
  CHECK(base_transport_ != nullptr);
  return LogMethodDataAndResult(
      "Post", path, data, [&]() { return base_transport_->Post(path, data); });
}
absl::StatusOr<RedfishTransport::Result> RedfishLoggedTransport::Patch(
    absl::string_view path, absl::string_view data) {
  CHECK(base_transport_ != nullptr);
  return LogMethodDataAndResult("Patch", path, data, [&]() {
    return base_transport_->Patch(path, data);
  });
}
absl::StatusOr<RedfishTransport::Result> RedfishLoggedTransport::Delete(
    absl::string_view path, absl::string_view data) {
  CHECK(base_transport_ != nullptr);
  return LogMethodDataAndResult("Delete", path, data, [&]() {
    return base_transport_->Delete(path, data);
  });
}
absl::StatusOr<RedfishTransport::Result> RedfishLoggedTransport::Put(
    absl::string_view path, absl::string_view data) {
  if (base_transport_ == nullptr) {
    return absl::InternalError("base_transport_ is null");
  }
  return LogMethodDataAndResult("Put", path, data, [this, path, data]() {
    return base_transport_->Put(path, data);
  });
}

}  // namespace ecclesia
