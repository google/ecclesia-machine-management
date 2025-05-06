/*
 * Copyright 2024 Google LLC
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

#include "ecclesia/lib/redfish/redpath/definitions/query_result/path_util.h"

#include <cstddef>
#include <cstdint>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/status/macros.h"
#include "ecclesia/lib/time/proto.h"

namespace ecclesia {
namespace {

enum class Operation : std::uint8_t { kGet, kRemove };

absl::StatusOr<std::string> QueryValueToString(const QueryValue& query_value) {
  switch (query_value.kind_case()) {
    case QueryValue::kStringValue:
      return query_value.string_value();
    case QueryValue::kIntValue:
      return absl::StrCat(query_value.int_value());
    case QueryValue::kDoubleValue:
      return absl::StrCat(query_value.double_value());
    case QueryValue::kTimestampValue: {
      return absl::FormatTime(
          "%Y-%m-%dT%H:%M:%E*SZ",
          AbslTimeFromProtoTime(query_value.timestamp_value()),
          absl::UTCTimeZone());
    }
    case QueryValue::kBoolValue:
      return absl::StrFormat("%v", query_value.bool_value());
    case QueryValue::kIdentifier: {
      std::vector<std::string> identifier_string;
      if (query_value.identifier().has_local_devpath()) {
        identifier_string.push_back(absl::StrCat(
            "_local_devpath_:", query_value.identifier().local_devpath()));
      }
      if (query_value.identifier().has_machine_devpath()) {
        identifier_string.push_back(absl::StrCat(
            "_machine_devpath_:", query_value.identifier().machine_devpath()));
      }

      if (!identifier_string.empty()) {
        return absl::StrCat("{", absl::StrJoin(identifier_string, ","), "}");
      }
      return absl::InvalidArgumentError("Identifier is empty");
    }
    default:
      return absl::InvalidArgumentError("QueryValue is not a supported type");
  }

  return absl::InvalidArgumentError("QueryValue is not a supported type");
}

bool CompareListIdentifier(
    const QueryValue& query_value,
    absl::Span<const std::pair<std::string, std::string>> identifier) {
  for (const auto& [key, value] : identifier) {
    const auto& fields = query_value.subquery_value().fields();
    auto it = fields.find(key);
    if (it == fields.end()) {
      return false;
    }
    absl::StatusOr<std::string> value_str = QueryValueToString(it->second);
    if (!value_str.ok() || *value_str != value) {
      return false;
    }
  }

  return true;
}

absl::StatusOr<QueryValue*> ProcessNextMutableQueryValue(
    QueryValue* value, absl::string_view subquery_id, Operation operation) {
  switch (value->kind_case()) {
    case QueryValue::kSubqueryValue: {
      auto& fields = *value->mutable_subquery_value()->mutable_fields();
      if (auto it = fields.find(subquery_id); it != fields.end()) {
        if (operation == Operation::kRemove) {
          fields.erase(it);
          return nullptr;
        }
        return &it->second;
      }
      return absl::NotFoundError(
          absl::StrCat("Subquery ID '", subquery_id, "' doesn't exist"));
    }
    case QueryValue::kListValue: {
      if (subquery_id.front() == '[' && subquery_id.back() == ']') {
        absl::string_view identifier_set =
            subquery_id.substr(1, subquery_id.size() - 2);
        // [property_name="value1",property_name="value2"], contains comma
        // separated list of <property_name,"value">
        if (absl::StrContains(identifier_set, "=")) {
          std::vector<std::pair<std::string, std::string>> identifier_parts;
          for (absl::string_view identifier :
               absl::StrSplit(identifier_set, ';')) {
            std::vector<std::string> parts = absl::StrSplit(identifier, '=');
            if (parts.size() != 2) {
              return absl::InvalidArgumentError(
                  absl::StrCat("Identifier '", subquery_id, "' is not valid"));
            }
            identifier_parts.push_back(std::make_pair(parts[0], parts[1]));
          }
          for (int i = 0; i < value->list_value().values_size(); ++i) {
            if (CompareListIdentifier(value->list_value().values(i),
                                      identifier_parts)) {
              if (operation == Operation::kRemove) {
                auto& values = *value->mutable_list_value()->mutable_values();
                values.erase(values.begin() + i);
                return nullptr;
              }
              return value->mutable_list_value()->mutable_values(i);
            }
          }
          return absl::NotFoundError(
              absl::StrCat("Identifier '", subquery_id, "' doesn't exist"));
        }

        // Contains index in brackets.
        int index;
        if (!absl::SimpleAtoi(identifier_set, &index)) {
          return absl::InvalidArgumentError(absl::StrCat(
              "The list index is not valid, expected a valid integer: ",
              subquery_id));
        }
        if (index < 0 || index >= value->list_value().values_size()) {
          return absl::NotFoundError(
              absl::StrCat("The list index is out of bounds: ", subquery_id));
        }
        if (operation == Operation::kRemove) {
          value->mutable_list_value()->mutable_values()->erase(
              value->mutable_list_value()->mutable_values()->begin() + index);
          return nullptr;
        }
        return value->mutable_list_value()->mutable_values(index);
      }
      break;
    }
    default:
      return absl::InvalidArgumentError("Value is not a subquery or list");
  }
  return absl::NotFoundError(
      absl::StrCat("Path '", subquery_id, "' doesn't exist"));
}

absl::StatusOr<QueryValue*> ProcessQueryValueFromResult(QueryResult& result,
                                                        absl::string_view path,
                                                        Operation operation) {
  if (path.empty()) {
    return absl::InvalidArgumentError("Path is empty.");
  }

  std::queue<absl::string_view> path_parts(absl::StrSplit(path, '.'));
  if (path_parts.front() != result.query_id()) {
    return absl::InvalidArgumentError(
        absl::StrCat("Query ID does not match: ", path_parts.front(), " vs ",
                     result.query_id()));
  }
  path_parts.pop();

  if (path_parts.empty()) {
    return absl::InvalidArgumentError(
        absl::StrCat("Path is not a subquery: ", path));
  }

  const size_t parts_size = path_parts.size();
  for (size_t i = 0; i < parts_size; ++i) {
    absl::string_view front_part = path_parts.front();
    path_parts.pop();
    if (auto pos = front_part.find_first_of('[');
        pos != absl::string_view::npos) {
      path_parts.push(front_part.substr(0, pos));
      path_parts.push(front_part.substr(pos));
    } else {
      path_parts.push(front_part);
    }
  }

  if (!result.has_data() || result.data().fields().empty()) {
    return absl::InvalidArgumentError("Query result has no data.");
  }

  QueryValue* current_value;
  if (auto it =
          result.mutable_data()->mutable_fields()->find(path_parts.front());
      it != result.data().fields().end()) {
    current_value = &it->second;
    path_parts.pop();
  } else {
    return absl::NotFoundError(
        absl::StrCat("Subquery ID '", path_parts.front(), "' doesn't exist"));
  }

  Operation actual_operation = Operation::kGet;
  while (!path_parts.empty()) {
    if (operation == Operation::kRemove && path_parts.size() == 1) {
      actual_operation = Operation::kRemove;
    }
    ECCLESIA_ASSIGN_OR_RETURN(
        current_value,
        ProcessNextMutableQueryValue(current_value, path_parts.front(),
                                     actual_operation));
    path_parts.pop();
  }
  return current_value;
}

}  // namespace

absl::StatusOr<QueryValue*> GetMutableQueryValueFromResult(
    QueryResult& result, absl::string_view path) {
  return ProcessQueryValueFromResult(result, path, Operation::kGet);
}

absl::StatusOr<QueryValue> GetQueryValueFromResult(const QueryResult& result,
                                                   absl::string_view path) {
  QueryValue* value = nullptr;
  ECCLESIA_ASSIGN_OR_RETURN(value, GetMutableQueryValueFromResult(
                                       const_cast<QueryResult&>(result), path));
  return *value;
}

absl::Status RemoveQueryValueFromResult(QueryResult& result,
                                        absl::string_view path) {
  ECCLESIA_RETURN_IF_ERROR(
      ProcessQueryValueFromResult(result, path, Operation::kRemove).status());
  return absl::OkStatus();
}

}  // namespace ecclesia
