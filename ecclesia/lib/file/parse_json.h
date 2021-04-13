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

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/optional.h"
#include "ecclesia/lib/file/cc_embed_interface.h"
#include "ecclesia/lib/logging/logging.h"
#include "json/json.h"
#include "json/value.h"

namespace ecclesia {

// Given a path to an embedded file, attempts to parse into a Json::Value,
// returning a failing status if errors are encountered.
template <size_t N>
absl::StatusOr<const Json::Value> ParseJsonValueFromEmbeddedFile(
    absl::string_view file_path, const std::array<EmbeddedFile, N> &array) {
  absl::optional<absl::string_view> embedded_file_contents(
      GetEmbeddedFileWithName(file_path, array));

  if (embedded_file_contents == absl::nullopt) {
    ErrorLog() << "Embedded file with name " << file_path << " not found";
    return absl::NotFoundError("Embedded file not found.");
  }

  Json::CharReaderBuilder builder;
  Json::Value json_contents;
  JSONCPP_STRING errors;

  const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
  if (!reader->parse(embedded_file_contents->begin(),
                     embedded_file_contents->end(), &json_contents, &errors)) {
    ErrorLog() << "Error(s) parsing embedded file contents:" << errors;
    return absl::InternalError("Embedded Data not JSON parseable.");
  }

  return json_contents;
}

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_FILE_PARSE_JSON_H_
