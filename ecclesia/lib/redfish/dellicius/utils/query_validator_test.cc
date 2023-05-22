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
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/testing/status.h"

namespace ecclesia {
namespace {

using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using Warning = RedPathQueryValidator::Warning;

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
      ElementsAre(Field(&Warning::type, Eq(Warning::Type::kDeepQuery))));
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
  EXPECT_THAT(
      deep_redpath_validator.GetWarnings(),
      ElementsAre(Field(&Warning::type, Eq(Warning::Type::kDeepRedPath))));
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
  EXPECT_THAT(
      wide_branching_validator.GetWarnings(),
      ElementsAre(Field(&Warning::type, Eq(Warning::Type::kWideBranching))));
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
  EXPECT_THAT(mixed_warnings_validator.GetWarnings(),
              UnorderedElementsAre(
                  Field(&Warning::type, Eq(Warning::Type::kWideBranching)),
                  Field(&Warning::type, Eq(Warning::Type::kDeepQuery))));
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

}  // namespace
}  // namespace ecclesia
