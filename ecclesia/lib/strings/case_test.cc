/*
 * Copyright 2022 Google LLC
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

#include "ecclesia/lib/strings/case.h"

#include <optional>

#include "gtest/gtest.h"

namespace ecclesia {
namespace {

TEST(StrCaseConversionTest, CamelcaseToSnakecase) {
  EXPECT_EQ(CamelcaseToSnakecase(""), "");
  EXPECT_EQ(CamelcaseToSnakecase("Single"), "single");
  EXPECT_EQ(CamelcaseToSnakecase("ThisStringToSnakecase"),
            "this_string_to_snakecase");
}

TEST(StrCaseConversionTest, SnakeCaseToCamelCase) {
  EXPECT_EQ(SnakecaseToCamelcase(""), "");
  EXPECT_EQ(SnakecaseToCamelcase("single"), "Single");
  EXPECT_EQ(SnakecaseToCamelcase("this_string_to_camelcase"),
            "ThisStringToCamelcase");
}

TEST(StrCaseConversionTest, RejectInvalidInputFormats) {
  EXPECT_EQ(CamelcaseToSnakecase("already_snakecase"), std::nullopt);
  EXPECT_EQ(CamelcaseToSnakecase("This!sABadString"), std::nullopt);
  EXPECT_EQ(CamelcaseToSnakecase("1StringWith2Numbers"), std::nullopt);
  EXPECT_EQ(SnakecaseToCamelcase("AlreadyCamelcase"), std::nullopt);
  EXPECT_EQ(SnakecaseToCamelcase("this_!s_a_bad_string"), std::nullopt);
  EXPECT_EQ(SnakecaseToCamelcase("a_string_with1_number"), std::nullopt);
}

}  // namespace
}  // namespace ecclesia
