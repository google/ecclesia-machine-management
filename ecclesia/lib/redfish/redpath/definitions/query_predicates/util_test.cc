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

#include "ecclesia/lib/redfish/redpath/definitions/query_predicates/util.h"

#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "ecclesia/lib/testing/status.h"

namespace ecclesia {
namespace {

using ::testing::Eq;
using ::testing::Gt;
using ::testing::Lt;

TEST(FuzzyStringComparisonTest, FuzzyStringCharComparison) {
  // lhs = rhs
  std::string lhs = "TestValue";
  std::string rhs = "TestValue";
  EXPECT_THAT(FuzzyStringComparison(lhs, rhs), IsOkAndHolds(Eq(0)));
  // lhs < rhs, "B" < "C"
  lhs = "TestValueB";
  rhs = "TestValueC";
  EXPECT_THAT(FuzzyStringComparison(lhs, rhs), IsOkAndHolds(Lt(0)));
  // lhs > rhs, "D" > "C"
  lhs = "TestValueD";
  EXPECT_THAT(FuzzyStringComparison(lhs, rhs), IsOkAndHolds(Gt(0)));
  // lhs < rhs, "TestValueCCC" < "TestValueC"
  lhs = "TestValueCCC";
  EXPECT_THAT(FuzzyStringComparison(lhs, rhs), IsOkAndHolds(Lt(0)));
  // lhs > rhs, "TestValue" > "TestValueC"
  lhs = "TestValue";
  EXPECT_THAT(FuzzyStringComparison(lhs, rhs), IsOkAndHolds(Gt(0)));
}

TEST(FuzzyStringComparisonTest, FuzzyStringNumericComparison) {
  // lhs == rhs
  std::string lhs = "23";
  std::string rhs = "23";
  EXPECT_THAT(FuzzyStringComparison(lhs, rhs), IsOkAndHolds(Eq(0)));
  // lhs > rhs, "24" > "23"
  lhs = "24";
  EXPECT_THAT(FuzzyStringComparison(lhs, rhs), IsOkAndHolds(Gt(0)));
  // lhs > rhs, "123" > "23". note that "23" > "123" lexicographically
  lhs = "123";
  EXPECT_THAT(FuzzyStringComparison(lhs, rhs), IsOkAndHolds(Gt(0)));
  // lhs < rhs, "22" < "23"
  lhs = "22";
  EXPECT_THAT(FuzzyStringComparison(lhs, rhs), IsOkAndHolds(Lt(0)));
  // lhs < rhs, "12" < "123". note that "12" > "123" lexicographically
  lhs = "12";
  EXPECT_THAT(FuzzyStringComparison(lhs, rhs), IsOkAndHolds(Lt(0)));
}

TEST(FuzzyStringComparisonTest, FuzzyStringMixedComparison) {
  // lhs == rhs
  std::string lhs = "Id_1_SubId_2";
  std::string rhs = "Id_1_SubId_2";
  EXPECT_THAT(FuzzyStringComparison(lhs, rhs), IsOkAndHolds(Eq(0)));
  lhs = "Id_1_0", rhs = "Id_1_0";
  EXPECT_THAT(FuzzyStringComparison(lhs, rhs), IsOkAndHolds(Eq(0)));
  // lhs > rhs, "Id_1_2" > "Id_1_1"
  lhs = "Id_1_2";
  EXPECT_THAT(FuzzyStringComparison(lhs, rhs), IsOkAndHolds(Gt(0)));
  // lhs > rhs, "Id_1_10" > "Id_1_1". "Id_1_1" is lexicographically greater.
  lhs = "Id_1_10";
  EXPECT_THAT(FuzzyStringComparison(lhs, rhs), IsOkAndHolds(Gt(0)));
  // lhs < rhs, "Id_1A" < "Id_1B"
  lhs = "Id_1A";
  rhs = "Id_1B";
  EXPECT_THAT(FuzzyStringComparison(lhs, rhs), IsOkAndHolds(Lt(0)));
  // lhs > rhs, "Id_1C" > "Id_1B"
  lhs = "Id_1C";
  EXPECT_THAT(FuzzyStringComparison(lhs, rhs), IsOkAndHolds(Gt(0)));
}

}  // namespace
}  // namespace ecclesia
