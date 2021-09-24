/*
 * Copyright 2021 Google LLC
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

// Provides a helper function for parsing an embedded JSON file into a
// Json::Value using the third party JsonCPP library.

#ifndef ECCLESIA_LIB_FILE_PARSE_JSON_H_
#define ECCLESIA_LIB_FILE_PARSE_JSON_H_

#include <array>
#include <cstddef>
#include <optional>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/file/cc_embed_interface.h"
#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/logging.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {

// Given a path to an embedded file, attempts to parse into nlohmann:json,
// returning a failing status if errors are encountered.
template <size_t N>
absl::StatusOr<const nlohmann::json> ParseJsonValueFromEmbeddedFile(
    absl::string_view file_path, const std::array<EmbeddedFile, N> &array) {
  std::optional<absl::string_view> embedded_file_contents(
      GetEmbeddedFileWithName(file_path, array));

  if (embedded_file_contents == std::nullopt) {
    ErrorLog() << "Embedded file with name " << file_path << " not found";
    return absl::NotFoundError("Embedded file not found.");
  }

  // parse without allowing exceptions
  nlohmann::json json_contents =
      nlohmann::json::parse(embedded_file_contents.value(), nullptr, false);

  // Check for parsing error
  if (json_contents.is_discarded()) {
    ErrorLog() << "Error(s) parsing embedded file contents.";
    return absl::InternalError("Embedded Data not JSON parseable.");
  }

  return json_contents;
}

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_FILE_PARSE_JSON_H_
