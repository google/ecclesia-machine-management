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

#include "ecclesia/lib/redfish/redpath/definitions/query_predicates/predicates.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "ecclesia/lib/testing/status.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {

namespace {

TEST(ApplyPredicateRuleTest, ShouldApplyRelationalOperatorsCorrectly) {
  nlohmann::json obj = {{"MemorySize", 32},
                        {"Status", {{"State", "Enabled"}}},
                        {"SerialNumber", "8"},
                        {"ComponentTemp", 30},
                        {"Created", "2023-09-16T18:50:24.633362+00:00"},
                        {"BoolProperty", true},
                        {"NullObject", {}}};

  PredicateOptions options;

  options.predicate = "MemorySize>16";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(true));

  options.predicate = "MemorySize>=32";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(true));

  options.predicate = "MemorySize=32";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(true));

  options.predicate = "MemorySize=0";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(false));

  options.predicate = "SerialNumber>6";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(true));

  options.predicate = "Status.State=Enabled";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(true));

  options.predicate = "Status.State!=Disabled";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(true));

  options.predicate = "Status.State!=Disabled";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(true));

  options.predicate = "!Status.UnknownProperty";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(true));

  options.predicate = "BoolProperty=true";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(true));

  options.predicate = "BoolProperty!=false";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(true));

  options.predicate = "NullObject=null";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(true));

  options.predicate = "Status.UnknownPropertyComparison=3";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(false));

  options.predicate = "ComponentTemp<=30";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(true));

  options.predicate = "ComponentTemp!=30";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(false));

  // Timestamp operations
  options.predicate = "Created>2022-03-16T15:51:00";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(true));

  options.predicate = "Created<2022-03-16T15:53:00Z";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(false));

  options.predicate = "Created=2023-09-16T18:50:24.633362+00:00";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(true));
}

TEST(ApplyPredicateRuleTest, ShouldApplyLogicalOperatorsCorrectly) {
  nlohmann::json obj = {{"MemorySize", 8},
                        {"Status", {{"State", "Warning"}}},
                        {"ComponentTemp", 15}};

  PredicateOptions options;

  options.predicate = "MemorySize>16 and Status.State=Enabled";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(false));

  options.predicate = "ComponentTemp<15 or MemorySize<10";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(true));

  options.predicate = "ComponentTemp<15 or MemorySize>10";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(false));

  options.predicate = "ComponentTemp>14 or MemorySize>10";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(true));

  options.predicate =
      "ComponentTemp>14 or MemorySize>10 and Status.State=Enabled";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(false));
}

TEST(ApplyPredicateRuleTest, ShouldApplySpecicalPredicatesCorrectly) {
  nlohmann::json obj = {{"Id", "1"}, {"Id", "2"}, {"Id", "3"}};

  PredicateOptions options;

  // When "*" is used, predicate test is always true.
  options.predicate = "*";
  options.node_index = 1;
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(true));

  options.predicate = "last()";
  options.node_index = 2;
  options.node_set_size = 3;
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(true));

  options.predicate = "last()";
  options.node_index = 2;
  options.node_set_size = 6;
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(false));

  options.predicate = "1";
  options.node_index = 1;
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(true));
}

TEST(ApplyPredicateRuleTest, ShouldReturnErrorOnInvalidPredicates) {
  nlohmann::json obj = {{"MemorySize", 32},
                        {"Status", {{"State", "Enabled"}}},
                        {"Created", "2023-11-24T08:40:15-08:00"},
                        {"InvalidCreated", "2023-11-32T08:40:15-08:00"}};

  PredicateOptions options;
  options.node_set_size = 1;
  options.node_index = 0;

  // Syntax Errors
  options.predicate = "";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsStatusInvalidArgument());

  options.predicate = "MemorySize > 10";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsStatusInvalidArgument());

  options.predicate = "MemorySize>16 or and Status.State=Enabled";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsStatusInvalidArgument());

  options.predicate = "MemorySize<=>10";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsStatusInvalidArgument());

  options.predicate = "MemorySize>10 OR";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsStatusInvalidArgument());

  options.predicate = "Status.State==Enabled !";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsStatusInvalidArgument());

  // Semantic Errors
  options.predicate = "[MissingNode]=Value";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsStatusInvalidArgument());

  options.predicate = "Status.State>=23";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsStatusInvalidArgument());

  options.predicate = "MemorySize==not_a_number";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsStatusInvalidArgument());

  options.predicate = "Created=2023-11-32T12:00:00";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsStatusInvalidArgument());

  options.predicate = "InvalidCreated=2023-09-16T18:50:24.633362+00:00";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsStatusInternal());
}

}  // namespace

}  // namespace ecclesia
