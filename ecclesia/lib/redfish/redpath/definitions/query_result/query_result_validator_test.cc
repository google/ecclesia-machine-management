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

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/testing/status.h"

namespace ecclesia {
namespace {

TEST(QueryResultValidatorTest, FailCreateEmptyQueryCofnig) {
  auto validator = QueryResultValidator::Create(nullptr);
  ASSERT_THAT(validator.status(), IsStatusInvalidArgument());
  EXPECT_EQ(validator.status().message(), "Query Configuration is Empty");
}
TEST(QueryResultValidatorTest, FailCreateEmptyQueryId) {
  DelliciusQuery query = ParseTextProtoOrDie(R"pb(
    subquery {
      subquery_id: "subquery_id"
      properties { property: "property" type: STRING }
    }
  )pb");

  absl::StatusOr<std::unique_ptr<QueryResultValidatorIntf>> validator =
      QueryResultValidator::Create(&query);
  ASSERT_THAT(validator.status(), IsStatusInvalidArgument());
  EXPECT_EQ(validator.status().message(), "Query ID is empty");
}
TEST(QueryResultValidatorTest, FailCreateMissingRootId) {
  DelliciusQuery query = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    subquery {
      subquery_id: "subquery_id1"
      properties { property: "property" type: STRING }
    }
    subquery {
      subquery_id: "subquery_id2"
      root_subquery_ids: "subquery_id"
      properties { property: "property" type: STRING }
    }
  )pb");
  absl::StatusOr<std::unique_ptr<QueryResultValidatorIntf>> validator =
      QueryResultValidator::Create(&query);
  ASSERT_THAT(validator.status(), IsStatusNotFound());
  EXPECT_EQ(validator.status().message(), "No subquery found for subquery_id");
}
TEST(QueryResultValidatorTest, FailCreateDuplicatePaths) {
  DelliciusQuery query = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    subquery {
      subquery_id: "subquery_id1"
      properties { property: "property" type: STRING }
    }
    subquery {
      subquery_id: "subquery_id2"
      root_subquery_ids: "subquery_id1"
      properties { property: "property" type: STRING }
    }
    subquery {
      subquery_id: "subquery_id2"
      root_subquery_ids: "subquery_id1"
      properties { property: "property" type: STRING }
    }
  )pb");
  absl::StatusOr<std::unique_ptr<QueryResultValidatorIntf>> validator =
      QueryResultValidator::Create(&query);
  ASSERT_THAT(validator.status(), IsStatusInternal());
  EXPECT_EQ(validator.status().message(),
            "Duplicate path: /subquery_id1/subquery_id2");
}

TEST(QueryResultValidatorTest, FailCreateCycleDetected) {
  DelliciusQuery query = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    subquery {
      subquery_id: "subquery_id1"
      properties { property: "property" type: STRING }
    }
    subquery {
      subquery_id: "subquery_id2"
      root_subquery_ids: "subquery_id1"
      properties { property: "property" type: STRING }
    }
    subquery {
      subquery_id: "subquery_id2"
      root_subquery_ids: "subquery_id2"
      properties { property: "property" type: STRING }
    }
  )pb");
  absl::StatusOr<std::unique_ptr<QueryResultValidatorIntf>> validator =
      QueryResultValidator::Create(&query);
  ASSERT_THAT(validator.status(), IsStatusInternal());
  EXPECT_EQ(validator.status().message(), "Cycle detected: subquery_id2");
}
TEST(QueryResultValidatorTest, SuccessCreateValidator) {
  DelliciusQuery query = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    subquery {
      subquery_id: "subquery_id"
      properties { property: "property" type: STRING }
    }
  )pb");
  EXPECT_THAT(QueryResultValidator::Create(&query), IsOk());
}
TEST(QueryResultValidatorTest, SuccessCreateValidator2Levels) {
  DelliciusQuery query = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    subquery {
      subquery_id: "subquery_id1"
      properties { property: "property" type: STRING }
    }
    subquery {
      subquery_id: "subquery_id2"
      root_subquery_ids: "subquery_id1"
      properties { property: "property" type: STRING }
    }
  )pb");
  EXPECT_THAT(QueryResultValidator::Create(&query), IsOk());
}

TEST(QueryResultValidatorTest, SuccessCreateValidator2Branched2Levels) {
  DelliciusQuery query = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    subquery {
      subquery_id: "subquery_id1"
      properties { property: "property" type: STRING }
    }
    subquery {
      subquery_id: "subquery_id2"
      root_subquery_ids: "subquery_id1"
      properties { property: "property" type: STRING }
    }
    subquery {
      subquery_id: "subquery_id3"
      root_subquery_ids: "subquery_id1"
      properties { property: "property" type: STRING }
    }
  )pb");
  EXPECT_THAT(QueryResultValidator::Create(&query), IsOk());
}

TEST(QueryResultValidatorTest, SuccessCreateValidatorMultiBranch) {
  DelliciusQuery query = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    subquery {
      subquery_id: "subquery_id1"
      properties { property: "property" type: STRING }
    }
    subquery {
      subquery_id: "subquery_id2"
      root_subquery_ids: "subquery_id1"
      properties { property: "property" type: STRING }
    }
    subquery {
      subquery_id: "subquery_id3"
      root_subquery_ids: "subquery_id1"
      properties { property: "property" type: STRING }
    }
    subquery {
      subquery_id: "subquery_id2"
      root_subquery_ids: "subquery_id3"
      properties { property: "property" type: STRING }
    }
  )pb");
  EXPECT_THAT(QueryResultValidator::Create(&query), IsOk());
}

TEST(QueryResultValidatorTest, SuccessValidate) {
  DelliciusQuery query = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    subquery {
      subquery_id: "subquery_id"
      properties { property: "property" type: STRING }
    }
  )pb");

  absl::StatusOr<std::unique_ptr<QueryResultValidatorIntf>> validator =
      QueryResultValidator::Create(&query);
  ASSERT_THAT(validator, IsOk());
  EXPECT_THAT((*validator)->Validate({}), IsOk());
}

TEST(QueryResultValidatorTest, SuccessStatelessValidator) {
  DelliciusQuery query = ParseTextProtoOrDie(R"pb(
    query_id: "query_id"
    subquery {
      subquery_id: "subquery_id"
      properties { property: "property" type: STRING }
    }
  )pb");
  EXPECT_THAT(ValidateQueryResult(query, {}), IsOk());
}

}  // namespace
}  // namespace ecclesia
