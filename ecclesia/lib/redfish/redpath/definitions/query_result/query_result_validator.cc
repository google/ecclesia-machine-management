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

#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result_validator.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ecclesia/lib/status/macros.h"

namespace ecclesia {

absl::StatusOr<std::unique_ptr<QueryResultValidator>>
QueryResultValidator::Create(const DelliciusQuery* query) {
  if (query == nullptr) {
    return absl::InternalError("Invalid Query");
  }

  absl::flat_hash_map<std::string, PropertyVector*> subquery_id_to_property;
  return absl::WrapUnique(
      new QueryResultValidator(*query, std::move(subquery_id_to_property)));
}

absl::Status QueryResultValidator::Validate(const QueryResult& query_result) {
  return absl::OkStatus();
}

absl::Status ValidateQueryResult(const DelliciusQuery& query,
                                 const QueryResult& result) {
  ECCLESIA_ASSIGN_OR_RETURN(
      std::unique_ptr<QueryResultValidatorIntf> query_result_validator,
      QueryResultValidator::Create(&query));

  return query_result_validator->Validate(result);
}

}  // namespace ecclesia
