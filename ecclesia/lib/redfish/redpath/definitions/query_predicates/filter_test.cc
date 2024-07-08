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
#include "absl/strings/string_view.h"
#include "ecclesia/lib/status/test_macros.h"
#include "ecclesia/lib/testing/status.h"
namespace ecclesia {
namespace {

TEST(RedfishVariant, RedfishQueryParamFilter) {
  ECCLESIA_ASSIGN_OR_FAIL(std::string filter1,
                          BuildFilterFromRedpathPredicate("Prop1<=42"));
  EXPECT_EQ(filter1, "Prop1%20le%2042");
  // Spaces between logical operator.
  ECCLESIA_ASSIGN_OR_FAIL(std::string filter2, BuildFilterFromRedpathPredicate(
                                                   "Prop1>42 and Prop1<84"));
  EXPECT_EQ(filter2, "Prop1%20gt%2042%20and%20Prop1%20lt%2084");
}

TEST(RedfishVariant, RedfishQueryParamFilterWithGrouping) {
  // Spaces between logical operator.
  ECCLESIA_ASSIGN_OR_FAIL(std::string filter1,
                          BuildFilterFromRedpathPredicate(
                              "Prop1>42 and (Prop1<84 or Prop3=Status.test)"));
  EXPECT_EQ(filter1,
            "Prop1%20gt%2042%20and%20%28Prop1%20lt%2084%20or%20Prop3%20eq%20%"
            "27Status.test%27%29");
}

TEST(RedfishVariant, RedfishQueryParamFilterPredicateList) {
  std::vector<std::string> predicates = {"Prop1<=42", "Prop1!=42"};
  ECCLESIA_ASSIGN_OR_FAIL(std::string filter1,
                          BuildFilterFromRedpathPredicateList(predicates));
  EXPECT_EQ(filter1, "Prop1%20le%2042%20or%20Prop1%20ne%2042");
}

TEST(RedfishVariant, RedfishQueryParamFilterStringNoQuote) {
  ECCLESIA_ASSIGN_OR_FAIL(
      std::string filter1,
      BuildFilterFromRedpathPredicate("Prop1=this\\ is\\ a\\ test"));
  EXPECT_EQ(filter1, "Prop1%20eq%20%27this%20is%20a%20test%27");
}

TEST(RedfishVariant, RedfishQueryParamFilterBoolean) {
  ECCLESIA_ASSIGN_OR_FAIL(std::string filter1,
                          BuildFilterFromRedpathPredicate("Prop1=true"));
  EXPECT_EQ(filter1, "Prop1%20eq%20true");
}

TEST(RedfishVariant, RedfishQueryParamFilterSpecialCharacters) {
  ECCLESIA_ASSIGN_OR_FAIL(std::string filter1,
                          BuildFilterFromRedpathPredicate(
                              "Created>=2024-01-22T00:41:38.000000+00:00"));
  EXPECT_EQ(filter1,
            "Created%20ge%20%272024-01-22T00%3A41%3A38.000000%2B00%3A00%27");
}

TEST(RedfishVariant, RedfishQueryParamFilterPeriodReplacement) {
  // Simple 1 expression predicate
  std::vector<std::string> predicates = {"Status.Health=OK.test"};
  ECCLESIA_ASSIGN_OR_FAIL(std::string filter1,
                          BuildFilterFromRedpathPredicateList(predicates));
  EXPECT_EQ(filter1, "Status%2FHealth%20eq%20%27OK.test%27");
  // 2 expression predicate
  ECCLESIA_ASSIGN_OR_FAIL(
      std::string filter2,
      BuildFilterFromRedpathPredicate(
          "Status.Health=OK.test or Status.Something>=6.8"));
  EXPECT_EQ(filter2,
            "Status%2FHealth%20eq%20%27OK.test%27%20or%20Status%2F"
            "Something%20ge%206.8");
}

TEST(RedfishVariant, RedfishQueryParamFilterExistence) {
  for (absl::string_view predicate :
       {"Prop1", "!Prop1", "Prop1.SubProp", "!Prop1.SubProp",
        "Prop1.SubProp or Status.Something>=6.8"}) {
    EXPECT_THAT(BuildFilterFromRedpathPredicate(predicate),
                IsStatusInvalidArgument());
  }
}

TEST(RedfishVariant, RedfishQueryParamFilterWithFuzzyOps) {
  ECCLESIA_ASSIGN_OR_FAIL(std::string filter1,
                          BuildFilterFromRedpathPredicate("Prop1<~=42"));
  EXPECT_EQ(filter1, "Prop1%20le%2042");

  ECCLESIA_ASSIGN_OR_FAIL(std::string filter2,
                          BuildFilterFromRedpathPredicate(
                              "Prop1~>42 and Prop1<~84 and Prop2~>=100"));
  EXPECT_EQ(filter2,
            "Prop1%20gt%2042%20and%20Prop1%20lt%2084%20and%20Prop2%20ge%20100");
}

}  // namespace
}  // namespace ecclesia
