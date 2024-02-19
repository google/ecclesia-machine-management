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

#include "ecclesia/lib/redfish/dellicius/utils/query_validator.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/testing/status.h"

namespace ecclesia {
namespace {

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::StartsWith;
using Issue = RedPathQueryValidator::Issue;

// Constants for valid RedPathQuery tests.
constexpr absl::string_view kPathToValidQueryBasic =
    "lib/redfish/dellicius/utils/"
    "query_validator_test_valid_query.textproto";

// Constants for valid RedPathQuery tests that trigger warnings.
constexpr absl::string_view kPathToValidQueryDeepQuery =
    "/path/to/valid/deep_query";
const char kValidQueryDeepQueryFileContents[] = R"pb(
  query_id: "ChassisQuery"
  subquery {
    subquery_id: "Processors"
    redpath: "/Systems[*]/Processors[*]"
    properties { property: "@odata\\.id" type: STRING }
  }
  subquery {
    subquery_id: "Chassis"
    redpath: "/Chassis[*]"
    properties { property: "@odata\\.id" type: STRING }
  }
  subquery {
    subquery_id: "ComputerSystems"
    root_subquery_ids: "Chassis"
    redpath: "/Links/ComputerSystems[*]"
    properties { property: "@odata\\.id" type: STRING }
  }
  subquery {
    subquery_id: "PCIeDevices"
    root_subquery_ids: "ComputerSystems"
    redpath: "/PCIeDevices[*]"
    properties { name: "uri" property: "@odata\\.id" type: STRING }
  }
  subquery {
    subquery_id: "PCIeFunctions"
    root_subquery_ids: "PCIeDevices"
    redpath: "/PCIeFunctions[*]"
    properties { name: "uri" property: "@odata\\.id" type: STRING }
  })pb";

constexpr absl::string_view kPathToValidQueryDeepRedPath =
    "/path/to/valid/deep_redpath";
const char kValidQueryDeepRedPathFileContents[] = R"pb(
  query_id: "Processors"
  subquery {
    subquery_id: "SubProcessors"
    redpath: "/Systems[*]/Processors[*]/SubProcessors[*]/SubProcessors[*]/SubProcessors[Id=1]"
    properties { property: "Id" type: STRING }
  })pb";

constexpr absl::string_view kPathToValidQueryWideBranching =
    "/path/to/valid/wide_branching";
const char kValidQueryWideBranchingFileContents[] = R"pb(
  query_id: "ChassisQuery"
  subquery {
    subquery_id: "Chassis"
    redpath: "/Chassis[*]"
    properties { property: "@odata\\.id" type: STRING }
  }
  subquery {
    subquery_id: "Contains"
    root_subquery_ids: "Chassis"
    redpath: "/Links/Contains[*]"
    properties { property: "@odata\\.id" type: STRING }
  }
  subquery {
    subquery_id: "Processors"
    root_subquery_ids: "Chassis"
    redpath: "/Links/Processors[*]"
    properties { property: "@odata\\.id" type: STRING }
  }
  subquery {
    subquery_id: "ComputerSystems"
    root_subquery_ids: "Chassis"
    redpath: "/Links/ComputerSystems[*]"
    properties { property: "@odata\\.id" type: STRING }
  })pb";

// Should trigger both wide branching and deep query warnings.
constexpr absl::string_view kPathToValidQueryMixedWarnings =
    "/path/to/valid/mixed_warnings";
const char kValidQueryMixedWarningsFileContents[] = R"pb(
  query_id: "ChassisQuery"
  subquery {
    subquery_id: "Processors"
    redpath: "/Systems[*]/Processors[*]"
    properties { property: "@odata\\.id" type: STRING }
  }
  subquery {
    subquery_id: "Chassis"
    redpath: "/Chassis[*]"
    properties { property: "@odata\\.id" type: STRING }
  }
  subquery {
    subquery_id: "ComputerSystems"
    root_subquery_ids: "Chassis"
    redpath: "/Links/ComputerSystems[*]"
    properties { property: "@odata\\.id" type: STRING }
  }
  subquery {
    subquery_id: "Storage"
    root_subquery_ids: "Chassis"
    redpath: "/Links/Storage[*]"
    properties { property: "@odata\\.id" type: STRING }
  }
  subquery {
    subquery_id: "PCIeDevices"
    root_subquery_ids: "ComputerSystems"
    redpath: "/PCIeDevices[*]"
    properties { name: "uri" property: "@odata\\.id" type: STRING }
  }
  subquery {
    subquery_id: "PCIeFunctions"
    root_subquery_ids: "PCIeDevices"
    redpath: "/PCIeFunctions[*]"
    properties { name: "uri" property: "@odata\\.id" type: STRING }
  })pb";

constexpr absl::string_view kPathToInvalidQueryConflictingSubqueryIds =
    "/path/to/invalid/conflicting_subquery_ids";
const char kInvalidQueryConflictingSubqueryIdsFileContents[] = R"pb(
  query_id: "ChassisQuery"
  subquery {
    subquery_id: "Query1"
    redpath: "/Systems[*]/Processors[*]"
    properties { property: "@odata\\.id" type: STRING }
  }
  subquery {
    subquery_id: "Query1"
    redpath: "/Chassis[*]"
    properties { property: "@odata\\.id" type: STRING }
  }
)pb";

constexpr absl::string_view kPathToInvalidQueryConflictingProperties =
    "/path/to/invalid/conflicting_properties";
const char kInvalidQueryConflictingPropertiesFileContents[] = R"pb(
  query_id: "ChassisQuery"
  subquery {
    subquery_id: "Query1"
    redpath: "/Systems[*]/Processors[*]"
    properties { name: "odata" property: "@odata\\.id" type: STRING }
    properties { name: "odata" property: "@odata\\.type" type: STRING }
  }
)pb";

// Example where a child subquery id is the same as a property name of the
// parent.
constexpr absl::string_view kPathToInvalidQueryConflictingIdProperty =
    "/path/to/invalid/conflicting_subquery_ids_and_properties";
const char kInvalidQueryConflictingIdPropertyFileContents[] = R"pb(
  query_id: "ChassisQuery"
  subquery {
    subquery_id: "Query1"
    redpath: "/Systems[*]"
    properties { property: "@odata\\.id" type: STRING }
    properties { name: "Query2" property: "LocationType" type: STRING }
  }
  subquery {
    subquery_id: "Query2"
    root_subquery_ids: "Query1"
    redpath: "/Chassis[*]"
    properties { property: "@odata\\.id" type: STRING }
  }
)pb";

constexpr absl::string_view kPathToInvalidQueryInvalidPredicate =
    "/path/to/invalid/invalid_predicate";
const char kToInvalidQueryInvalidPredicateFileContents[] = R"pb(
  query_id: "ChassisQuery"
  subquery {
    subquery_id: "Query1"
    redpath: "/Systems[*]/Processors[ProcessorType=$type and $another_predicate]"
    properties { property: "@odata\\.id" type: STRING }
  }
  subquery {
    subquery_id: "Query2"
    redpath: "/Chassis[*]"
    properties { property: "@odata\\.id" type: STRING }
  }
  subquery {
    subquery_id: "Query3"
    redpath: "/Chassis[last()]"
    properties { property: "@odata\\.id" type: STRING }
  }
  subquery {
    subquery_id: "Query4"
    redpath: "/Chassis[Id=xyz]"
    properties { property: "@odata\\.id" type: STRING }
  }
  subquery {
    subquery_id: "Query5"
    redpath: "/Chassis[*]/Links/Memory[$memory_location_predicate]"
    properties { property: "@odata\\.id" type: STRING }
  }
)pb";

// Constants for invalid RedPathQuery tests that should trigger errors.
constexpr absl::string_view kPathToMissingQuery = "/path/to/missing/query/";
constexpr absl::string_view kPathToInvalidQuery =
    "lib/redfish/dellicius/utils/"
    "query_validator_test_invalid_query.textproto";

TEST(RedPathQueryValidationTest, ReadAndValdiateValidQueryFileSuccess) {
  RedPathQueryValidator validator;
  EXPECT_THAT(validator.ValidateQueryFile(
                  GetTestDataDependencyPath(kPathToValidQueryBasic)),
              IsOk());
  EXPECT_THAT(validator.GetErrors(), IsEmpty());
  EXPECT_THAT(validator.GetWarnings(), IsEmpty());
}

TEST(RedPathQueryValidationTest, ValidQueryWithDeepQueryWarning) {
  RedPathQueryValidator deep_query_validator(
      [&](absl::string_view path) -> absl::StatusOr<DelliciusQuery> {
        EXPECT_THAT(path, Eq(kPathToValidQueryDeepQuery));
        return ParseTextAsProtoOrDie<DelliciusQuery>(
            kValidQueryDeepQueryFileContents);
      });
  EXPECT_THAT(
      deep_query_validator.ValidateQueryFile(kPathToValidQueryDeepQuery),
      IsOk());
  EXPECT_THAT(deep_query_validator.GetErrors(), IsEmpty());
  EXPECT_THAT(
      deep_query_validator.GetWarnings(),
      ElementsAre(AllOf(Field(&Issue::type, Eq(Issue::Type::kDeepQuery)),
                        Field(&Issue::path, Eq(kPathToValidQueryDeepQuery)))));
}

TEST(RedPathQueryValidationTest, ValidQueryWithDeepRedPathWarning) {
  RedPathQueryValidator deep_redpath_validator(
      [&](absl::string_view path) -> absl::StatusOr<DelliciusQuery> {
        EXPECT_THAT(path, Eq(kPathToValidQueryDeepRedPath));
        return ParseTextAsProtoOrDie<DelliciusQuery>(
            kValidQueryDeepRedPathFileContents);
      });
  EXPECT_THAT(
      deep_redpath_validator.ValidateQueryFile(kPathToValidQueryDeepRedPath),
      IsOk());
  EXPECT_THAT(deep_redpath_validator.GetErrors(), IsEmpty());
  EXPECT_THAT(deep_redpath_validator.GetWarnings(),
              ElementsAre(AllOf(
                  Field(&Issue::type, Eq(Issue::Type::kDeepRedPath)),
                  Field(&Issue::path, Eq(kPathToValidQueryDeepRedPath)))));
}

TEST(RedPathQueryValidationTest, ValidQueryWithWideBranchingWarning) {
  RedPathQueryValidator wide_branching_validator(
      [&](absl::string_view path) -> absl::StatusOr<DelliciusQuery> {
        EXPECT_THAT(path, Eq(kPathToValidQueryWideBranching));
        return ParseTextAsProtoOrDie<DelliciusQuery>(
            kValidQueryWideBranchingFileContents);
      });
  EXPECT_THAT(wide_branching_validator.ValidateQueryFile(
                  kPathToValidQueryWideBranching),
              IsOk());
  EXPECT_THAT(wide_branching_validator.GetErrors(), IsEmpty());
  EXPECT_THAT(wide_branching_validator.GetWarnings(),
              ElementsAre(AllOf(
                  Field(&Issue::type, Eq(Issue::Type::kWideBranching)),
                  Field(&Issue::path, Eq(kPathToValidQueryWideBranching)))));
}

TEST(RedPathQueryValidationTest, MixedWarnings) {
  RedPathQueryValidator mixed_warnings_validator(
      [&](absl::string_view path) -> absl::StatusOr<DelliciusQuery> {
        EXPECT_THAT(path, Eq(kPathToValidQueryMixedWarnings));
        return ParseTextAsProtoOrDie<DelliciusQuery>(
            kValidQueryMixedWarningsFileContents);
      });
  EXPECT_THAT(mixed_warnings_validator.ValidateQueryFile(
                  kPathToValidQueryMixedWarnings),
              IsOk());
  EXPECT_THAT(mixed_warnings_validator.GetErrors(), IsEmpty());
  EXPECT_THAT(
      mixed_warnings_validator.GetWarnings(),
      UnorderedElementsAre(Field(&Issue::type, Eq(Issue::Type::kWideBranching)),
                           Field(&Issue::type, Eq(Issue::Type::kDeepQuery))));
}

TEST(RedPathQueryValidationTest, ReadFailureTriggersErrors) {
  // Use the default validator to try to read from a file that doesn't exist.
  RedPathQueryValidator validator;
  EXPECT_THAT(validator.ValidateQueryFile(kPathToMissingQuery),
              IsStatusNotFound());
  // Try read from a file with an invalid redpath query.
  EXPECT_THAT(validator.ValidateQueryFile(
                  GetTestDataDependencyPath(kPathToInvalidQuery)),
              IsStatusInvalidArgument());
}

TEST(RedPathQueryValidationTest, InvalidQueryWithConflictingSubqueryIds) {
  RedPathQueryValidator validator(
      [&](absl::string_view path) -> absl::StatusOr<DelliciusQuery> {
        EXPECT_THAT(path, Eq(kPathToInvalidQueryConflictingSubqueryIds));
        return ParseTextAsProtoOrDie<DelliciusQuery>(
            kInvalidQueryConflictingSubqueryIdsFileContents);
      });
  EXPECT_THAT(
      validator.ValidateQueryFile(kPathToInvalidQueryConflictingSubqueryIds),
      IsOk());
  EXPECT_THAT(
      validator.GetErrors(),
      ElementsAre(AllOf(
          Field(&Issue::type, Eq(Issue::Type::kConflictingIds)),
          Field(&Issue::path, Eq(kPathToInvalidQueryConflictingSubqueryIds)))));
}

TEST(RedPathQueryValidationTest, InvalidQueryWithConflictingProperties) {
  RedPathQueryValidator validator(
      [&](absl::string_view path) -> absl::StatusOr<DelliciusQuery> {
        EXPECT_THAT(path, Eq(kPathToInvalidQueryConflictingProperties));
        return ParseTextAsProtoOrDie<DelliciusQuery>(
            kInvalidQueryConflictingPropertiesFileContents);
      });
  EXPECT_THAT(
      validator.ValidateQueryFile(kPathToInvalidQueryConflictingProperties),
      IsOk());
  EXPECT_THAT(
      validator.GetErrors(),
      ElementsAre(AllOf(
          Field(&Issue::type, Eq(Issue::Type::kConflictingIds)),
          Field(&Issue::path, Eq(kPathToInvalidQueryConflictingProperties)))));
}

TEST(RedPathQueryValidationTest, InvalidQueryWithConflictingIdAndProperty) {
  RedPathQueryValidator validator(
      [&](absl::string_view path) -> absl::StatusOr<DelliciusQuery> {
        EXPECT_THAT(path, Eq(kPathToInvalidQueryConflictingIdProperty));
        return ParseTextAsProtoOrDie<DelliciusQuery>(
            kInvalidQueryConflictingIdPropertyFileContents);
      });
  EXPECT_THAT(
      validator.ValidateQueryFile(kPathToInvalidQueryConflictingIdProperty),
      IsOk());
  EXPECT_THAT(
      validator.GetErrors(),
      ElementsAre(AllOf(
          Field(&Issue::type, Eq(Issue::Type::kConflictingIds)),
          Field(&Issue::path, Eq(kPathToInvalidQueryConflictingIdProperty)))));
}

TEST(RedPathQueryValidationTest, InvalidQueryWithInvalidPredicate) {
  RedPathQueryValidator validator(
      [&](absl::string_view path) -> absl::StatusOr<DelliciusQuery> {
        EXPECT_THAT(path, Eq(kPathToInvalidQueryInvalidPredicate));
        return ParseTextAsProtoOrDie<DelliciusQuery>(
            kToInvalidQueryInvalidPredicateFileContents);
      });
  EXPECT_THAT(validator.ValidateQueryFile(kPathToInvalidQueryInvalidPredicate),
              IsOk());
  auto x = validator.GetErrors().front();
  EXPECT_THAT(
      validator.GetErrors(),
      ElementsAre(
          AllOf(Field(&Issue::type, Eq(Issue::Type::kDisallowedPredicate)),
                Field(&Issue::message,
                      StartsWith(
                          "Disallowed predicate: $another_predicate")),
                Field(&Issue::path, Eq(kPathToInvalidQueryInvalidPredicate))),
          AllOf(Field(&Issue::type, Eq(Issue::Type::kDisallowedPredicate)),
                Field(&Issue::message,
                      StartsWith(
                          "Disallowed predicate: $memory_location_predicate")),
                Field(&Issue::path, Eq(kPathToInvalidQueryInvalidPredicate)))));
}

}  // namespace
}  // namespace ecclesia
