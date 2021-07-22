/*
 * Copyright 2021 Google LLC
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

#include "ecclesia/lib/strings/string_view.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace ecclesia {
namespace {

TEST(StringViewLiteralTest, EmptyLiteral) {
  constexpr absl::string_view sv = StringViewLiteral("");
  EXPECT_TRUE(sv.empty());
}

TEST(StringViewLiteralTest, NormalString) {
  constexpr absl::string_view sv = StringViewLiteral("abcd1234");
  EXPECT_EQ(sv, "abcd1234");
  EXPECT_EQ(sv.size(), 8);
}

TEST(StringViewLiteralTest, StringWithNuls) {
  constexpr absl::string_view sv = StringViewLiteral("a\0b\0c\0");
  const char array[] = "a\0b\0c\0";
  EXPECT_EQ(sv, std::string(array, 6));
  EXPECT_EQ(sv.size(), 6);
}

TEST(StringViewLiteralTest, StringAllNuls) {
  constexpr absl::string_view sv = StringViewLiteral("\0\0\0\0\0");
  std::string s(5, '\0');
  EXPECT_EQ(sv, s);
  EXPECT_EQ(sv.size(), 5);
}

}  // namespace
}  // namespace ecclesia
