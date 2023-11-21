/*
 * Copyright 2023 Google LLC
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

#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.h"

#include <cstdint>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/status/macros.h"

namespace ecclesia {

// Returns a QueryValueReader for the underlying subquery result for the given
// key; or an error if the key is not found.
absl::StatusOr<QueryValueReader> QueryValueReader::Get(
    absl::string_view key) const {
  if (!query_value_.has_subquery_value()) {
    return absl::InternalError("QueryValue has no subquery_value");
  }
  auto it = query_value_.subquery_value().fields().find(key);
  if (it == query_value_.subquery_value().fields().end()) {
    return absl::NotFoundError(absl::StrCat("Key '", key, "' doesn't exist"));
  }
  return QueryValueReader(&it->second);
}

// Returns the string value for a given key; returns error if the key is not
// present or is not a string property.
absl::StatusOr<std::string> QueryValueReader::GetStringValue(
    absl::string_view key) const {
  ECCLESIA_ASSIGN_OR_RETURN(QueryValueReader reader, Get(key));
  if (reader.kind() != QueryValue::kStringValue) {
    return absl::InvalidArgumentError(
        absl::StrCat("Property ", key, " does not have a string value."));
  }
  return reader.string_value();
}

// Returns the int value for a given key; returns error if the key is not
// present or is not an int property.
absl::StatusOr<int64_t> QueryValueReader::GetIntValue(
    absl::string_view key) const {
  ECCLESIA_ASSIGN_OR_RETURN(QueryValueReader reader, Get(key));
  if (reader.kind() != QueryValue::kIntValue) {
    return absl::InvalidArgumentError(
        absl::StrCat("Property ", key, " does not have an int value."));
  }
  return reader.int_value();
}

// Returns the double value for a given key; returns error if the key is not
// present or is not a double property.
absl::StatusOr<double> QueryValueReader::GetDoubleValue(
    absl::string_view key) const {
  ECCLESIA_ASSIGN_OR_RETURN(QueryValueReader reader, Get(key));
  if (reader.kind() != QueryValue::kDoubleValue) {
    return absl::InvalidArgumentError(
        absl::StrCat("Property ", key, " does not have a double value."));
  }
  return reader.double_value();
}

// Returns the boolean value for a given key; returns error if the key is not
// present or is not a bool property.
absl::StatusOr<bool> QueryValueReader::GetBoolValue(
    absl::string_view key) const {
  ECCLESIA_ASSIGN_OR_RETURN(QueryValueReader reader, Get(key));
  if (reader.kind() != QueryValue::kBoolValue) {
    return absl::InvalidArgumentError(
        absl::StrCat("Property ", key, " does not have a bool value."));
  }
  return reader.bool_value();
}

// Returns a QueryValueReader for the given key; or an error if the key is not
// found.
absl::StatusOr<QueryValueReader> QueryResultDataReader::Get(
    absl::string_view key) const {
  if (auto it = query_result_.fields().find(key);
      it != query_result_.fields().end()) {
    return QueryValueReader(&it->second);
  }
  return absl::NotFoundError(absl::StrCat("Key '", key, "' doesn't exist"));
}

}  // namespace ecclesia
