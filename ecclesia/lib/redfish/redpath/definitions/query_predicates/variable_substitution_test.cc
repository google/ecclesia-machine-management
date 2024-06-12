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

#include "ecclesia/lib/redfish/redpath/definitions/query_predicates/variable_substitution.h"

#include <string>

#include "gtest/gtest.h"
#include "absl/status/statusor.h"
#include "ecclesia/lib/redfish/dellicius/query/query_variables.pb.h"

namespace ecclesia {
namespace {

TEST(RedpathPredicateVariables, SubstituteSingleVariable) {
  std::string predicate = "Reading>$Threshold";
  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  ecclesia::QueryVariables::VariableValue val1;
  val1.set_name("Threshold");
  *val1.add_values() = "60";
  *args1.add_variable_values() = val1;
  auto result = SubstituteVariables(predicate, args1);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.value(), "Reading>60");
}

TEST(RedpathPredicateVariables, SubstituteDoubleVariables) {
  std::string predicate = "Reading>$Threshold and Type=$Type";
  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  ecclesia::QueryVariables::VariableValue val1;
  ecclesia::QueryVariables::VariableValue val2;
  val1.set_name("Threshold");
  *val1.add_values() = "60";
  val2.set_name("Type");
  *val2.add_values() = "test";
  *args1.add_variable_values() = val1;
  *args1.add_variable_values() = val2;
  auto result = SubstituteVariables(predicate, args1);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.value(), "Reading>60 and Type=test");
}

TEST(RedpathPredicateVariables, SubstituteVariableExisting) {
  std::string predicate = "Reading>42 and Type=$Type";
  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  ecclesia::QueryVariables::VariableValue val1;
  val1.set_name("Type");
  *val1.add_values() = "test";
  *args1.add_variable_values() = val1;
  auto result = SubstituteVariables(predicate, args1);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.value(), "Reading>42 and Type=test");
}

TEST(RedpathPredicateVariables, SubstituteVariableMultiValue) {
  std::string predicate = "Type=$Type";
  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  ecclesia::QueryVariables::VariableValue val1;
  val1.set_name("Type");
  *val1.add_values() = "type1";
  *val1.add_values() = "type2";
  *args1.add_variable_values() = val1;
  auto result = SubstituteVariables(predicate, args1);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.value(), "(Type=type1 or Type=type2)");
}

TEST(RedpathPredicateVariables, SubstituteVariableMultiValueExisting) {
  std::string predicate = "Type=$Type and Reading<42";
  ecclesia::QueryVariables args1 = ecclesia::QueryVariables();
  ecclesia::QueryVariables::VariableValue val1;
  val1.set_name("Type");
  *val1.add_values() = "type1";
  *val1.add_values() = "type2";
  *args1.add_variable_values() = val1;
  auto result = SubstituteVariables(predicate, args1);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.value(), "(Type=type1 or Type=type2) and Reading<42");
}

TEST(RedpathPredicateVariables, SubstituteVariableEmptyValueSelectAll) {
  std::string predicate1 = "Type=$Type";
  auto result1 = SubstituteVariables(predicate1, ecclesia::QueryVariables());
  EXPECT_TRUE(result1.ok());
  EXPECT_EQ(result1.value(), "*");
}

TEST(RedpathPredicateVariables, SubstituteVariableEmptyVar) {
  std::string predicate1 = "Type=$Type and Reading<42";
  std::string predicate2 = "Reading<42 and Type=$Type";
  auto result1 = SubstituteVariables(predicate1, ecclesia::QueryVariables());
  EXPECT_TRUE(result1.ok());
  EXPECT_EQ(result1.value(), "Reading<42");
  auto result2 = SubstituteVariables(predicate2, ecclesia::QueryVariables());
  EXPECT_TRUE(result2.ok());
  EXPECT_EQ(result2.value(), "Reading<42");
}

TEST(RedpathPredicateVariables, SubstituteVariableEmptyVarInThree) {
  std::string predicate1 = "Reading<$Var and Type=Fan or Units=RPM";
  auto result = SubstituteVariables(predicate1, ecclesia::QueryVariables());
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.value(), "Type=Fan or Units=RPM");
}

TEST(RedpathPredicateVariables, SubstituteVariableTwoEmptyVarsInThree) {
  std::string predicate1 = "Reading<$Var1 and Type=$Var2 and Units=RPM";
  auto result = SubstituteVariables(predicate1, ecclesia::QueryVariables());
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.value(), "Units=RPM");
}

TEST(RedpathPredicateVariables, SubstituteVariableAllEmptyVarsInThree) {
  std::string predicate1 = "Reading<$Var1 and Type=$Var2 and Units=$Var3";
  auto result = SubstituteVariables(predicate1, ecclesia::QueryVariables());
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.value(), "*");
}

TEST(RedpathPredicateVariables, SubstituteVariableEmptyVarInGroup) {
  std::string predicate1 = "(Reading<60 and Type=$Var1) and Units=RPM";
  auto result = SubstituteVariables(predicate1, ecclesia::QueryVariables());
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.value(), "Reading<60 and Units=RPM");
}

TEST(RedpathPredicateVariables, SubstituteVariableEmptyVarGroup) {
  std::string predicate1 = "(Reading<$Var1 and Type=$Var2) and Units=RPM";
  auto result = SubstituteVariables(predicate1, ecclesia::QueryVariables());
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.value(), "Units=RPM");
}

TEST(RedpathPredicateVariables, SubstituteVariableEmptyVarGroupRhs) {
  std::string predicate1 = "Units=RPM and (Reading<$Var1 and Type=$Var2)";
  auto result = SubstituteVariables(predicate1, ecclesia::QueryVariables());
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.value(), "Units=RPM");
}

}  // namespace
}  // namespace ecclesia
