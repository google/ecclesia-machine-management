/*
 * Copyright 2020 Google LLC
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

#include "ecclesia/lib/http/client.h"

#include <memory>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/substitute.h"
#include "json/reader.h"
#include "json/value.h"

namespace ecclesia {

std::string GetHttpMethodName(ecclesia::Protocol protocol) {
  switch (protocol) {
    case Protocol::kGet:
      return "GET";
    case Protocol::kPost:
      return "POST";
  }
}

absl::StatusOr<Json::Value> HttpClient::HttpResponse::GetBodyJson() {
  auto json = Json::Value();
  Json::Reader reader;
  if (!reader.parse(body, json)) {
    return absl::InvalidArgumentError(absl::Substitute(
        "Failed to parse json: '$0'", reader.getFormattedErrorMessages()));
  }
  return json;
}

}  // namespace ecclesia
