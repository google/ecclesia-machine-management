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

#include <string>

#include "single_include/nlohmann/json.hpp"

namespace ecclesia {

std::string GetHttpMethodName(ecclesia::Protocol protocol) {
  switch (protocol) {
    case Protocol::kGet:
      return "GET";
    case Protocol::kPost:
      return "POST";
    case Protocol::kDelete:
      return "DELETE";
    case Protocol::kPatch:
      return "PATCH";
  }
}

nlohmann::json HttpClient::HttpResponse::GetBodyJson() {
  return nlohmann::json::parse(body, nullptr, /*allow_exceptions=*/false);
}

}  // namespace ecclesia
