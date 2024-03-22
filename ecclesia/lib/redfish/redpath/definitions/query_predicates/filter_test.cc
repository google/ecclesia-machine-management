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

#include "ecclesia/lib/redfish/redpath/definitions/query_predicates/filter.h"

#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/statusor.h"
#include "ecclesia/lib/testing/status.h"

namespace ecclesia {
namespace {

TEST(RedfishVariant, RedfishQueryParamFilter) {
  std::string predicate1 = "Prop1<=42";
  absl::StatusOr<std::string> filter1 =
                       BuildFilterFromRedpathPredicate(predicate1);
  ASSERT_TRUE(filter1.ok());
  EXPECT_EQ(*filter1, "Prop1%20le%2042");
  // Spaces between logical operator.
  std::string predicate2 = "Prop1>42 and Prop1<84";
  absl::StatusOr<std::string> filter2 =
                       BuildFilterFromRedpathPredicate(predicate2);
  ASSERT_TRUE(filter2.ok());
  EXPECT_EQ(*filter2, "Prop1%20gt%2042%20and%20Prop1%20lt%2084");
}

TEST(RedfishVariant, RedfishQueryParamFilterInvalid) {
  // Invalid operator (wrong equality)
  EXPECT_THAT(BuildFilterFromRedpathPredicate("Prop1==42"),
              IsStatusInvalidArgument());
  // Invalid operator
  EXPECT_THAT(BuildFilterFromRedpathPredicate("Prop1>>42"),
              IsStatusInvalidArgument());
  // Spaces on left
  EXPECT_THAT(BuildFilterFromRedpathPredicate("Bad Property>42"),
              IsStatusInvalidArgument());

  // Special characters in operands
  std::string predicate4 = "Prop<erty1=42";
  std::string predicate5 = "Property2=4>2";
  EXPECT_THAT(BuildFilterFromRedpathPredicateList({predicate4, predicate5}),
              IsStatusInvalidArgument());
  // One side of a logical exp is bad. Try both sides.
  std::string predicate6 = "Property2=42";
  std::string predicate7 = "Prop<erty1=42";
  EXPECT_THAT(BuildFilterFromRedpathPredicateList({predicate6, predicate7}),
              IsStatusInvalidArgument());
  EXPECT_THAT(BuildFilterFromRedpathPredicateList({predicate7, predicate6}),
              IsStatusInvalidArgument());
}

TEST(RedfishVariant, RedfishQueryParamFilterPredicateList) {
  std::string predicate1 = "Prop1<=42";
  std::string predicate2 = "Prop1!=42";
  std::vector<std::string> predicates = {predicate1, predicate2};
  absl::StatusOr<std::string> filter1 =
                       BuildFilterFromRedpathPredicateList(predicates);
  ASSERT_TRUE(filter1.ok());
  EXPECT_EQ(*filter1, "Prop1%20le%2042%20or%20Prop1%20ne%2042");
}

TEST(RedfishVariant, RedfishQueryParamFilterStringQuote) {
  std::string predicate1 = "Prop1='this is a test'";
  absl::StatusOr<std::string> filter1 =
                       BuildFilterFromRedpathPredicate(predicate1);
  ASSERT_TRUE(filter1.ok());
  EXPECT_EQ(*filter1, "Prop1%20eq%20%27this%20is%20a%20test%27");
}

TEST(RedfishVariant, RedfishQueryParamFilterStringNoQuote) {
  std::string predicate1 = "Prop1=this is a test";
  absl::StatusOr<std::string> filter1 =
                       BuildFilterFromRedpathPredicate(predicate1);
  ASSERT_TRUE(filter1.ok());
  EXPECT_EQ(*filter1, "Prop1%20eq%20%27this%20is%20a%20test%27");
}

TEST(RedfishVariant, RedfishQueryParamFilterBoolean) {
  std::string predicate1 = "Prop1=true";
  absl::StatusOr<std::string> filter1 =
                       BuildFilterFromRedpathPredicate(predicate1);
  ASSERT_TRUE(filter1.ok());
  EXPECT_EQ(*filter1, "Prop1%20eq%20true");
}

TEST(RedfishVariant, RedfishQueryParamFilterSpecialCharacters) {
  std::string predicate1 = "Created>=2024-01-22T00:41:38.000000+00:00";
  absl::StatusOr<std::string> filter1 =
                       BuildFilterFromRedpathPredicate(predicate1);
  ASSERT_TRUE(filter1.ok());
  EXPECT_EQ(*filter1,
            "Created%20ge%20%272024-01-22T00%3A41%3A38.000000%2B00%3A00%27");
}

TEST(RedfishVariant, RedfishQueryParamFilterPeriodReplacement) {
  // Simple 1 expression predicate
  std::string predicate1 = "Status.Health=OK.test";
  std::vector<std::string> predicates = {predicate1};
  absl::StatusOr<std::string> filter1 =
                       BuildFilterFromRedpathPredicateList(predicates);
  ASSERT_TRUE(filter1.ok());
  EXPECT_EQ(*filter1, "Status%2FHealth%20eq%20%27OK.test%27");
  // 2 expression predicate
  std::string predicate2 = "Status.Health=OK.test or Status.Something>=6.8";
  absl::StatusOr<std::string> filter2 =
                       BuildFilterFromRedpathPredicate(predicate2);
  ASSERT_TRUE(filter2.ok());
  EXPECT_EQ(*filter2,
            "Status%2FHealth%20eq%20%27OK.test%27%20or%20Status%2F"
            "Something%20ge%206.8");
}

TEST(RedfishVariant, RedfishQueryParamFilterExistence) {
  std::string predicate1 = "Prop1";
  EXPECT_THAT(BuildFilterFromRedpathPredicate(predicate1),
              IsStatusInvalidArgument());
  std::string predicate2 = "!Prop1";
  EXPECT_THAT(BuildFilterFromRedpathPredicate(predicate2),
              IsStatusInvalidArgument());
  std::string predicate3 = "Prop1.sub_prop";
  EXPECT_THAT(BuildFilterFromRedpathPredicate(predicate3),
              IsStatusInvalidArgument());
  std::string predicate4 = "!Prop1.sub_prop";
  EXPECT_THAT(BuildFilterFromRedpathPredicate(predicate4),
              IsStatusInvalidArgument());
}

}  // namespace
}  // namespace ecclesia
