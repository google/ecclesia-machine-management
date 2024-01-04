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

#ifndef ECCLESIA_LIB_REDFISH_DELLICIUS_UTILS_QUERY_VALIDATOR_H_
#define ECCLESIA_LIB_REDFISH_DELLICIUS_UTILS_QUERY_VALIDATOR_H_

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"

namespace ecclesia {

// This class is used to construct a validator object that operates on a given
// redpath query (definition in
// ecclesia/lib/redfish/dellicius/query/query.proto.h),
// validating it against current CSDL schema and issuing warnings
// for queries that have potentially expensive or suboptimal configurations.
//
// Example Usage:
//  constexpr absl::string_view kPathToRedpathQuery = path/to/query.textproto
//  RedPathQueryValidator query_validator;
//  RETURN_IF_ERROR(query_validator.ValidateQueryFile(kPathToRedpathQuery));
//  DisplayWarnings(query_validator.GetWarnings());
//  ...
//
class RedPathQueryValidator {
 public:
  struct Issue {
    enum class Type : uint8_t {
      // Occurs when property names within a subquery conflict with
      // each other or the subquery id, or subquery ids conflict across a
      // redpath query.
      kConflictingIds,
      // Occurs when 2+ RedPathResources depend on the same resource context.
      kWideBranching,
      // Occurs when querying more than 5 RedPathResources.
      kDeepQuery,
      // Occurs when a query has more than 5 nodes in one Redpath.
      kDeepRedPath
    };

    static absl::string_view GetDescriptor(Type type) {
      switch (type) {
        case Type::kConflictingIds:
          return "Conflicting Identifiers";
        case Type::kWideBranching:
          return "Wide Branching";
        case Type::kDeepQuery:
          return "Deep Query";
        case Type::kDeepRedPath:
          return "Deep Redpath";
      }
    }

    Type type;
    std::string message;
    std::string path;
  };

  // Function alias for the method used to retrieve a RedPath Query message,
  // given a depot file path to the textproto containing the query.
  using QueryRetrievalFunction =
      absl::AnyInvocable<absl::StatusOr<ecclesia::DelliciusQuery>(
          absl::string_view)>;

  // By default, a RedPathQueryValidator will use a QueryRetrievalFunction that
  // uses a file reader to just parse the file at the provided location to a
  // DelliciusQuery proto, returning an InvalidArgumentError if parsing fails.
  explicit RedPathQueryValidator(
      QueryRetrievalFunction get_redpath_query = GetRedPathQuery)
      : get_redpath_query_(std::move(get_redpath_query)) {}

  // Returns errors that reflect incongruencies when comparing against the most
  // valid CSDL schema.
  absl::Span<const Issue> GetErrors() { return errors_; }

  // Returns all warnings that were stored for any of the Issue types.
  absl::Span<const Issue> GetWarnings() { return warnings_; }

  // Validates a RedPath Query proto file. Errors and warnings that occur can
  // be accessed from GetErrors and GetWarnings, respectively.
  absl::Status ValidateQueryFile(absl::string_view path);

 private:
  static absl::StatusOr<DelliciusQuery> GetRedPathQuery(absl::string_view path);

  QueryRetrievalFunction get_redpath_query_;
  std::vector<Issue> errors_;
  std::vector<Issue> warnings_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_DELLICIUS_UTILS_QUERY_VALIDATOR_H_
