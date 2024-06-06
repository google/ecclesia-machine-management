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

#include "ecclesia/lib/redfish/redpath/definitions/query_predicates/predicate_object.h"

#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/statusor.h"
#include "ecclesia/lib/testing/status.h"

namespace ecclesia {
namespace {

TEST(RedfishVariant, SingleExpressionPredicate) {
  std::string predicate1 = "Prop1<=42";
  absl::StatusOr<PredicateObject> object = CreatePredicateObject(predicate1);
  ASSERT_TRUE(object.ok());
  EXPECT_EQ((*object).child_predicates.size(), 0);
  EXPECT_EQ((*object).logical_operators.size(), 0);
  absl::StatusOr<std::string> assembled_predicate =
      PredicateObjectToString(*object);
  EXPECT_EQ(*assembled_predicate, predicate1);
}

TEST(RedfishVariant, LogicalOperatorPredicate) {
  std::string predicate1 = "Prop1<=42 or Prop2>10";
  absl::StatusOr<PredicateObject> object = CreatePredicateObject(predicate1);
  ASSERT_TRUE(object.ok());
  EXPECT_EQ((*object).child_predicates.size(), 2);
  EXPECT_EQ((*object).logical_operators.size(), 1);
  absl::StatusOr<std::string> assembled_predicate =
      PredicateObjectToString(*object);
  EXPECT_EQ(*assembled_predicate, predicate1);
}

TEST(RedfishVariant, DoubleLogicalOperatorPredicate) {
  std::string predicate1 = "Prop1<=42 or Prop2>10 and Prop3=test";
  absl::StatusOr<PredicateObject> object = CreatePredicateObject(predicate1);
  ASSERT_TRUE(object.ok());
  EXPECT_EQ((*object).child_predicates.size(), 3);
  EXPECT_EQ((*object).logical_operators.size(), 2);
  absl::StatusOr<std::string> assembled_predicate =
      PredicateObjectToString(*object);
  EXPECT_EQ(*assembled_predicate, predicate1);
}

TEST(RedfishVariant, InvalidPredicate) {
  // Invalid operator (wrong equality)
  EXPECT_THAT(CreatePredicateObject("Prop1==42"), IsStatusInvalidArgument());
  // Invalid operator
  EXPECT_THAT(CreatePredicateObject("Prop1>>42"), IsStatusInvalidArgument());
  // Spaces on left
  EXPECT_THAT(CreatePredicateObject("Bad Property>42"),
              IsStatusInvalidArgument());

  // Special characters in operands
  std::string predicate5 = "Property2=4>2";
  EXPECT_THAT(CreatePredicateObject(predicate5), IsStatusInvalidArgument());
  // One side of a logical exp is bad. Try both sides.
  std::string predicate7 = "Prop<erty1=42";
  EXPECT_THAT(CreatePredicateObject(predicate7), IsStatusInvalidArgument());
  std::string predicate8 = "Prop<erty1=42 or Prop1>42";
  EXPECT_THAT(CreatePredicateObject(predicate8), IsStatusInvalidArgument());
  std::string predicate9 = "Property1=42 or Prop1>>42";
  EXPECT_THAT(CreatePredicateObject(predicate9), IsStatusInvalidArgument());
}

// Currently existence checks are not supported by the predicate objects.
TEST(RedfishVariant, PredicateExistenceCheck) {
  std::string predicate1 = "Prop1";
  absl::StatusOr<PredicateObject> object = CreatePredicateObject(predicate1);
  ASSERT_TRUE(object.ok());
  absl::StatusOr<std::string> assembled_predicate =
      PredicateObjectToString(*object);
  ASSERT_TRUE(assembled_predicate.ok());
  EXPECT_EQ(*assembled_predicate, predicate1);
  std::string predicate2 = "!Prop1";
  absl::StatusOr<PredicateObject> object2 = CreatePredicateObject(predicate2);
  ASSERT_TRUE(object2.ok());
  absl::StatusOr<std::string> assembled_predicate2 =
      PredicateObjectToString(*object2);
  ASSERT_TRUE(assembled_predicate2.ok());
  EXPECT_EQ(*assembled_predicate2, predicate2);
  std::string predicate3 = "Prop1.SubProp";
  absl::StatusOr<PredicateObject> object3 = CreatePredicateObject(predicate3);
  ASSERT_TRUE(object3.ok());
  absl::StatusOr<std::string> assembled_predicate3 =
      PredicateObjectToString(*object3);
  ASSERT_TRUE(assembled_predicate3.ok());
  EXPECT_EQ(*assembled_predicate3, predicate3);
  std::string predicate4 = "!Prop1.SubProp";
  absl::StatusOr<PredicateObject> object4 = CreatePredicateObject(predicate4);
  ASSERT_TRUE(object4.ok());
  absl::StatusOr<std::string> assembled_predicate4 =
      PredicateObjectToString(*object4);
  ASSERT_TRUE(assembled_predicate4.ok());
  EXPECT_EQ(*assembled_predicate4, predicate4);
}

// Currently existence checks are not supported by the predicate objects.
TEST(RedfishVariant, PredicateExistenceWithRelexp) {
  std::string predicate1 = "Prop1 and Prop1>42";
  absl::StatusOr<PredicateObject> object = CreatePredicateObject(predicate1);
  ASSERT_TRUE(object.ok());
  absl::StatusOr<std::string> assembled_predicate =
      PredicateObjectToString(*object);
  ASSERT_TRUE(assembled_predicate.ok());
  EXPECT_EQ(*assembled_predicate, predicate1);
}

TEST(RedfishVariant, PredicateExistenceCheckInvalid) {
  std::string predicate1 = "prop";
  EXPECT_THAT(CreatePredicateObject(predicate1), IsStatusInvalidArgument());
  std::string predicate2 = "!prop1";
  EXPECT_THAT(CreatePredicateObject(predicate2), IsStatusInvalidArgument());
  std::string predicate3 = "Prop1.sub_prop";
  EXPECT_THAT(CreatePredicateObject(predicate3), IsStatusInvalidArgument());
}

TEST(RedfishVariant, PredicateParsingParens) {
  std::string predicate1 = "(Prop1<=42 or Prop2>10) and Prop3=test";
  absl::StatusOr<PredicateObject> object = CreatePredicateObject(predicate1);
  ASSERT_TRUE(object.ok());
  EXPECT_EQ((*object).child_predicates.size(), 2);
  EXPECT_EQ((*object).logical_operators.size(), 1);
  absl::StatusOr<std::string> assembled_predicate =
      PredicateObjectToString(*object);
  EXPECT_EQ(*assembled_predicate, predicate1);
}

TEST(RedfishVariant, PredicateParsingEscapedSpace) {
  std::string predicate1 = "Prop1=hello\\ world and Prop3=test";
  absl::StatusOr<PredicateObject> object = CreatePredicateObject(predicate1);
  ASSERT_TRUE(object.ok()) << object.status();
  EXPECT_EQ((*object).child_predicates.size(), 2);
  EXPECT_EQ((*object).logical_operators.size(), 1);
  absl::StatusOr<std::string> assembled_predicate =
      PredicateObjectToString(*object);
  EXPECT_EQ(*assembled_predicate, predicate1);
}

TEST(RedfishVariant, PredicateParsingRemovesExtraParens) {
  std::string predicate1 = "(Prop1<=42 or (Prop2>10)) and Prop3=test";
  absl::StatusOr<PredicateObject> object = CreatePredicateObject(predicate1);
  ASSERT_TRUE(object.ok());
  EXPECT_EQ((*object).child_predicates.size(), 2);
  EXPECT_EQ((*object).logical_operators.size(), 1);
  absl::StatusOr<std::string> assembled_predicate =
      PredicateObjectToString(*object);
  ASSERT_TRUE(assembled_predicate.ok());
  EXPECT_EQ(*assembled_predicate, "(Prop1<=42 or Prop2>10) and Prop3=test");
}

TEST(RedfishVariant, PredicateParsingBadParens) {
  std::string predicate1 = "(Prop1<=42 or Prop2>10)) and Prop3=test'";
  EXPECT_THAT(CreatePredicateObject(predicate1), IsStatusInvalidArgument());
  std::string predicate2 = "((Prop1<=42 or Prop2>10) and Prop3=test";
  EXPECT_THAT(CreatePredicateObject(predicate2), IsStatusInvalidArgument());
  std::string predicate3 = "(Prop1<=42 or Prop2>10 and Prop3=test";
  EXPECT_THAT(CreatePredicateObject(predicate3), IsStatusInvalidArgument());
  std::string predicate4 = "()Prop1<=42 or Prop2>10 and Prop3=test";
  EXPECT_THAT(CreatePredicateObject(predicate4), IsStatusInvalidArgument());
}

}  // namespace
}  // namespace ecclesia
