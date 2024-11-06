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

#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/converter.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/status/macros.h"

namespace ecclesia {
namespace {
bool CompareListIdentifer(
    const QueryValue& query_value,
    absl::Span<const std::pair<std::string, std::string>> identifier) {
  for (const auto& [key, value] : identifier) {
    auto it = query_value.subquery_value().fields().find(key);
    if (it == query_value.subquery_value().fields().end()) {
      return false;
    }
    if (it->second.has_identifier() &&
        IdentifierValueToJson(it->second.identifier()).dump() != value) {
      return false;
    }
    if (ValueToJson(it->second).dump() != value) {
      return false;
    }
  }

  return true;
}

absl::StatusOr<const QueryValue*> GetNextQueryValue(
    const QueryValue* value, absl::string_view subquery_id) {
  switch (value->kind_case()) {
    case QueryValue::kSubqueryValue: {
      auto it = value->subquery_value().fields().find(subquery_id);
      if (it == value->subquery_value().fields().end()) {
        return absl::InvalidArgumentError(
            absl::StrCat("Subquery ID '", subquery_id, "' doesn't exist"));
      }
      return &it->second;
    }
    case QueryValue::kListValue: {
      if (absl::StrContains(subquery_id, "=")) {
        std::vector<std::pair<std::string, std::string>> identifier_parts;
        for (absl::string_view identifier : absl::StrSplit(subquery_id, ',')) {
          std::vector<std::string> parts = absl::StrSplit(identifier, '=');
          if (parts.size() != 2) {
            return absl::InvalidArgumentError(
                absl::StrCat("Identifier '", subquery_id, "' is not valid"));
          }
          identifier_parts.push_back(std::make_pair(parts[0], parts[1]));
        }

        for (const auto& list_value : value->list_value().values()) {
          if (CompareListIdentifer(list_value, identifier_parts)) {
            return &list_value;
          }
        }
        return absl::NotFoundError(
            absl::StrCat("Identifier '", subquery_id, "' doesn't exist"));
      }

      if (subquery_id.front() == '[' && subquery_id.back() == ']') {
        int index;
        if (!absl::SimpleAtoi(subquery_id.substr(1, subquery_id.size() - 2),
                              &index)) {
          return absl::InvalidArgumentError(absl::StrCat(
              "The list index is not valid, expected a valid integer: ",
              subquery_id));
        }
        if (index < 0 || index >= value->list_value().values_size()) {
          return absl::NotFoundError(
              absl::StrCat("The list index is out of bounds: ", subquery_id));
        }
        return &value->list_value().values(index);
      }
      break;
    }
    default:
      return absl::InvalidArgumentError("Value is not a subquery or list");
  }
  return absl::InvalidArgumentError(
      absl::StrCat("Path '", subquery_id, "' doesn't exist"));
}

}  // namespace

absl::StatusOr<QueryValue> GetQueryValueFromResult(const QueryResult& result,
                                                   absl::string_view path) {
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

  if (!result.has_data() || result.data().fields().empty()) {
    return absl::InvalidArgumentError("Query result has no data.");
  }

  const QueryValue* current_value;
  if (auto it = result.data().fields().find(path_parts.front());
      it != result.data().fields().end()) {
    current_value = &it->second;
    path_parts.pop();
  } else {
    return absl::InvalidArgumentError(
        absl::StrCat("Path is not a subquery: ", path));
  }

  while (!path_parts.empty()) {
    ECCLESIA_ASSIGN_OR_RETURN(
        current_value, GetNextQueryValue(current_value, path_parts.front()));

    path_parts.pop();
  }
  return *current_value;
}

}  // namespace ecclesia
