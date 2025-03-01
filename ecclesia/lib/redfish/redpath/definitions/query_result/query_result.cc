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
#include <tuple>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/status/macros.h"
#include "ecclesia/lib/time/proto.h"

namespace ecclesia {

namespace {

bool operator==(const RedfishLocation& a, const RedfishLocation& b) {
  return std::tie(a.service_label(), a.part_location_context()) ==
         std::tie(b.service_label(), b.part_location_context());
}

bool operator==(const Identifier& a, const Identifier& b) {
  return std::tie(a.local_devpath(), a.machine_devpath(),
                  a.embedded_location_context()) ==
             std::tie(b.local_devpath(), b.machine_devpath(),
                      b.embedded_location_context()) &&
         a.redfish_location() == b.redfish_location();
}

bool RemoveDataForIdentifier(QueryValue& value,
                                 const Identifier& identifier) {
  bool result = false;
  switch (value.kind_case()) {
    case QueryValue::kSubqueryValue: {
      auto& fields = *(value.mutable_subquery_value()->mutable_fields());
      if (auto it = fields.find(kIdentifierTag); it != fields.end()) {
        if (RemoveDataForIdentifier(it->second, identifier)) {
          value.clear_subquery_value();
          result = true;
          break;
        }
      }
      for (auto& [key, subquery_value] : fields) {
        result |= RemoveDataForIdentifier(subquery_value, identifier);
      }
      break;
    }
    case QueryValue::kListValue: {
      for (auto& list_value : *value.mutable_list_value()->mutable_values()) {
        result |= RemoveDataForIdentifier(list_value, identifier);
      }
      break;
    }
    case QueryValue::kIdentifier: {
      result = identifier == value.identifier();
      break;
    }
    default:
      result = false;
  }
  return result;
}

void GetDataForIdentifier(const QueryValue& value,
                              const Identifier& identifier,
                              std::vector<QueryResultData>& result) {
  switch (value.kind_case()) {
    case QueryValue::kSubqueryValue: {
      if (auto it = value.subquery_value().fields().find(kIdentifierTag);
          it != value.subquery_value().fields().end()) {
        if (it->second.identifier() == identifier) {
          result.push_back(value.subquery_value());
          break;
        }
      }
      for (const auto& [key, subquery_value] :
           value.subquery_value().fields()) {
        GetDataForIdentifier(subquery_value, identifier, result);
      }
      break;
    }
    case QueryValue::kListValue: {
      for (const auto& list_value : value.list_value().values()) {
        GetDataForIdentifier(list_value, identifier, result);
      }
      break;
    }
    default:
      break;
  }
}

}  // namespace

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

std::vector<std::string> QueryValueReader::field_keys() const {
  std::vector<std::string> keys;
  for (const auto& [key, value] : query_value_.subquery_value().fields()) {
    keys.push_back(key);
  }
  return keys;
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

absl::StatusOr<Identifier> QueryValueReader::GetIdentifier() const {
  ECCLESIA_ASSIGN_OR_RETURN(QueryValueReader reader, Get(kIdentifierTag));
  if (reader.kind() != QueryValue::kIdentifier) {
    return absl::InvalidArgumentError("No identifier available.");
  }
  return reader.identifier();
}

absl::StatusOr<QueryResult> GetQueryResult(QueryIdToResult result,
                                           absl::string_view query_id) {
  if (result.results().empty()) {
    return absl::FailedPreconditionError(
        absl::StrCat("No results for query_id: ", query_id,
                     ". The output from the Query Engine is empty."));
  }
  auto it = result.mutable_results()->find(query_id);
  if (it == result.mutable_results()->end()) {
    return absl::NotFoundError(absl::StrCat(
        "Query result doesn't contain result for query: ", query_id, "."));
  }
  QueryResult query_result = std::move(it->second);
  if (!QueryResultHasErrors(query_result)) {
    return std::move(query_result);
  }
  std::string error_message =
      absl::StrCat("Query result contains errors for query: ", query_id, ".\n");
  // Enumerate all errors, seperated by newlines.
  for (const auto& error : query_result.status().errors()) {
    absl::StrAppend(&error_message, error, "\n");
  }
  return absl::InternalError(error_message);
}

bool QueryResultHasErrors(const QueryResult& query_result) {
  return (query_result.has_status() && !query_result.status().errors().empty());
}

bool QueryOutputHasErrors(const QueryIdToResult& query_output) {
  for (const auto& [query_id, query_result] : query_output.results()) {
    if (QueryResultHasErrors(query_result)) {
      return true;
    }
  }
  return false;
}

absl::StatusOr<absl::Duration> GetQueryDuration(
    const QueryResult& query_result) {
  if (!query_result.has_stats() || !query_result.stats().has_start_time() ||
      !query_result.stats().has_end_time()) {
    return absl::InternalError("Query result has no time statistics.");
  }
  absl::Time start_time =
      AbslTimeFromProtoTime(query_result.stats().start_time());
  absl::Time end_time = AbslTimeFromProtoTime(query_result.stats().end_time());
  if (start_time > end_time) {
    return absl::InternalError(
        "Query result has invalid time statistics. Start time is after end "
        "time.");
  }
  return end_time - start_time;
}

bool RemoveDataForIdentifier(QueryResult& query_result,
                                 const Identifier& identifier) {
  bool result = false;
  for (auto& [key, value] : *query_result.mutable_data()->mutable_fields()) {
    result |= RemoveDataForIdentifier(value, identifier);
  }
  return result;
}

absl::StatusOr<std::vector<QueryResultData>> GetDataForIdentifier(
    const QueryResult& query_result, const Identifier& identifier) {
  std::vector<QueryResultData> result;
  for (const auto& [key, value] : query_result.data().fields()) {
    GetDataForIdentifier(value, identifier, result);
  }
  if (result.empty()) {
    return absl::NotFoundError(
        absl::StrCat("No subquery result found for devpath: ",
                     identifier.machine_devpath()));
  }
  return std::move(result);
}

absl::Status StatusFromQueryResultStatus(const ecclesia::Status& status) {
  std::string error_message = absl::StrJoin(status.errors(), "\n");
  switch (status.error_code()) {
    case ecclesia::ErrorCode::ERROR_NONE:
      return absl::OkStatus();
    case ecclesia::ErrorCode::ERROR_NETWORK:
      return absl::DeadlineExceededError(error_message);
    case ecclesia::ErrorCode::ERROR_SERVICE_ROOT_UNREACHABLE:
      return absl::FailedPreconditionError(error_message);
    case ecclesia::ErrorCode::ERROR_UNAUTHENTICATED:
      return absl::UnauthenticatedError(error_message);
    case ecclesia::ErrorCode::ERROR_QUERY_TIMEOUT:
      return absl::DeadlineExceededError(error_message);
    case ecclesia::ErrorCode::ERROR_INTERNAL:
      return absl::InternalError(error_message);
    case ecclesia::ErrorCode::ERROR_UNAVAILABLE:
      return absl::UnavailableError(error_message);
    case ecclesia::ErrorCode::ERROR_CANCELLED:
      return absl::CancelledError(error_message);
    // Account for all cases so method won't compile if new Statuses are
    // added.
    case ecclesia::ErrorCode_INT_MIN_SENTINEL_DO_NOT_USE_:
    case ecclesia::ErrorCode_INT_MAX_SENTINEL_DO_NOT_USE_:
      return absl::UnknownError(error_message);
  }
}

}  // namespace ecclesia
