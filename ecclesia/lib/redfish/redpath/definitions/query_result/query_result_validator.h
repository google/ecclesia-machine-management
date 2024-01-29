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

#ifndef ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_RESULT_QUERY_RESULT_VALIDATOR_H_
#define ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_RESULT_QUERY_RESULT_VALIDATOR_H_

#include <memory>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"

namespace ecclesia {

// The result will be validated against an actual query.
class QueryResultValidatorIntf {
 public:
  virtual ~QueryResultValidatorIntf() = default;
  // Results:
  // OK - check is ok
  // Internal Error - Something went wrong check message
  virtual absl::Status Validate(const QueryResult& query_result) = 0;
};

// This does not take ownership of the DelliciusQuery configuration. The query
// configuration must exist after the creation of the QueryResultValidator
class QueryResultValidator : public QueryResultValidatorIntf {
 public:
  using PropertyVector = const ::google::protobuf::RepeatedPtrField<
      DelliciusQuery::Subquery::RedfishProperty>;

  // The query must outlive the QueryResultValidator.
  static absl::StatusOr<std::unique_ptr<QueryResultValidator>> Create(
      const DelliciusQuery* query);

  absl::Status Validate(const QueryResult& query_result) override;

 private:
  QueryResultValidator(
      const DelliciusQuery& query,
      absl::flat_hash_map<std::string, PropertyVector*> subquery_id_to_property)
      : query_(query),
        subquery_id_to_property_(std::move(subquery_id_to_property)) {}

  const DelliciusQuery& query_;
  absl::flat_hash_map<std::string, PropertyVector*> subquery_id_to_property_;
};

// Helper function that will validate a query result via
// QueryResultValidatorIntf's validate. This is a stateless function.
absl::Status ValidateQueryResult(const DelliciusQuery& query,
                                 const QueryResult& result);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_QUERY_RESULT_QUERY_RESULT_VALIDATOR_H_
