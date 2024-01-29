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

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/statusor.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/testing/status.h"

namespace ecclesia {
namespace {

TEST(QueryResultValidatorTest, SuccessCreateValidator) {
  DelliciusQuery query = ParseTextProtoOrDie(R"pb(
    subquery {
      subquery_id: "subquery_id"
      properties { property: "property" type: STRING }
    }
  )pb");
  EXPECT_THAT(QueryResultValidator::Create(&query), IsOk());
}

TEST(QueryResultValidatorTest, SuccessValidate) {
  DelliciusQuery query = ParseTextProtoOrDie(R"pb(
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
    subquery {
      subquery_id: "subquery_id"
      properties { property: "property" type: STRING }
    }
  )pb");
  EXPECT_THAT(ValidateQueryResult(query, {}), IsOk());
}

}  // namespace
}  // namespace ecclesia
