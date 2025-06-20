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
                        {"Id", "CPU_1"},
                        {"IdWithSpaces", "My Resource 42"},
                        {"StringProperty", "TestValue1"},
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

  options.predicate = "IdWithSpaces=My\\ Resource\\ 42";
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

TEST(ApplyPredicateRuleTest, FuzzyStringComparisonPredicatesCorrectly) {
  nlohmann::json obj = {
      {"SerialNumber", "8"}, {"Id", "CPU_1"}, {"StringProperty", "TestValueB"}};

  PredicateOptions options;
  // Testing properties that have no digits
  // StringProperty has value of "TestValue1"
  options.predicate = "StringProperty<~TestValueC";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(true));

  options.predicate = "StringProperty<~=TestValueC";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(true));

  options.predicate = "StringProperty<~=TestValueB";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(true));

  options.predicate = "StringProperty~>=TestValueB";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(true));

  options.predicate = "StringProperty~>TestValueA";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(true));

  options.predicate = "StringProperty~>=TestValueA";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(true));

  options.predicate = "StringProperty~>=TestValueA";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(true));
  // Test value is identical at the same positions, but is longer.
  options.predicate = "StringProperty~>TestValueABCD";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(true));
  // Test value is identical at the same positions, but is shorter.
  options.predicate = "StringProperty~>TestValue";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(false));

  // Testing properties that have a mix of chars and digits
  // Id has value of "CPU_1"
  options.predicate = "Id<~CPU_20";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(true));

  options.predicate = "Id~>CPU_0";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(true));

  options.predicate = "Id~>=CPU_1";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(true));

  options.predicate = "Id~>=CPU_1_";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(true));

  // Testing properties that are all digits
  // SerialNumber has value of "8"
  options.predicate = "SerialNumber~>9";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(false));

  options.predicate = "SerialNumber~>6";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(true));

  options.predicate = "SerialNumber~>=8";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(true));

  options.predicate = "SerialNumber<~=8";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(true));

  // Ensure not lexicographic comparison, so ("8" > "70") is false.
  options.predicate = "SerialNumber~>70";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(false));

  options.predicate = "SerialNumber<~70";
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

TEST(ApplyPredicateRuleTest, ShouldWorkWithParenthesis) {
  nlohmann::json obj = {{"MemorySize", 8},
                        {"BoolProperty", true},
                        {"Status", {{"State", "Warning"}}},
                        {"ComponentTemp", 15}};

  PredicateOptions options;
  // same as (false and false) or false
  options.predicate =
      "(MemorySize>16 and Status.State=Enabled) or ComponentTemp<20";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(true));
  // same as (true or (false and true))
  options.predicate =
      "(ComponentTemp<20 or (BoolProperty=false and MemorySize<10))";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(true));
  // same as (false and (false or true))
  options.predicate =
      "(ComponentTemp>20 and (Status.State=Enabled or MemorySize<10))";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(false));
  // same as (true or (false and (false or true)))
  options.predicate =
      "(BoolProperty=true or (ComponentTemp>20 and (Status.State=Enabled or "
      "MemorySize<10)))";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(true));
  // same as (true and (false or (false and true))) or true
  options.predicate =
      "(ComponentTemp>10 and (ComponentTemp<20 or (BoolProperty=false and "
      "MemorySize<10))) or "
      "Status.State=Enabled";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(true));
  // same as false or (false and true)
  options.predicate =
      "(ComponentTemp>20 or (BoolProperty=false and MemorySize<10))";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(false));
  // same as true and (false and true)
  options.predicate =
      "(ComponentTemp<20 and (Status.State=disabled and MemorySize=8))";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(false));
  // same as (false and true) or (true and false)
  options.predicate =
      "(ComponentTemp<15 and MemorySize>10) or (ComponentTemp=30 and "
      "Status.State=Disabled)";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(false));
  // same as (false and true) or (false and false)
  options.predicate =
      "(ComponentTemp<15 and MemorySize>10) or ((ComponentTemp=30) and "
      "Status.State=Disabled)";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(false));

  // tests with last()
  options.node_index = 2;
  options.node_set_size = 3;
  options.predicate =
      "(ComponentTemp<15 and MemorySize>10) or last()";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(true));
  options.node_index = 2;
  options.node_set_size = 3;
  options.predicate = "ComponentTemp<16 and (MemorySize>100 or last())";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(true));
  options.node_set_size = 6;
  options.predicate =
      "(ComponentTemp<15 and MemorySize>10) or last()";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsOkAndHolds(false));

  // Invalid Parenthesis Cases
  // extra opening parenthesis
  options.predicate =
      "((MemorySize>16 and Status.State=Enabled) or ComponentTemp<20 or last()";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsStatusInvalidArgument());
  // extra closing parenthesis
  options.predicate =
      "(MemorySize>16 and Status.State=Enabled) or ComponentTemp<20)";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsStatusInvalidArgument());
  // malformed predicate with parenthesis
  options.predicate =
      "(MemorySize>16and Status.State=Enabled) or ComponentTemp<20)";
  EXPECT_THAT(ApplyPredicateRule(obj, options), IsStatusInvalidArgument());
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
