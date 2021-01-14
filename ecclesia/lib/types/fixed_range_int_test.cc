/*
 * Copyright 2020 Google LLC
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

#include "ecclesia/lib/types/fixed_range_int.h"

#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/types/optional.h"

namespace ecclesia {
namespace {

using ::testing::Pair;
using ::testing::UnorderedElementsAre;

class MyIntClass : public FixedRangeInteger<MyIntClass, int, 3, 42> {
 public:
  explicit constexpr MyIntClass(BaseType value) : BaseType(value) {}
};

TEST(FixedRangeIntegerTest, RangeCheck) {
  EXPECT_EQ(MyIntClass::TryMake(2), absl::nullopt);
  EXPECT_EQ(MyIntClass::TryMake(3)->value(), 3);
  EXPECT_EQ(MyIntClass::TryMake(42)->value(), 42);
  EXPECT_EQ(MyIntClass::TryMake(43), absl::nullopt);
}

TEST(FixedRangeIntegerTest, Clamp) {
  EXPECT_EQ(MyIntClass::Clamp(2).value(), 3);
  EXPECT_EQ(MyIntClass::Clamp(43).value(), 42);
  EXPECT_EQ(MyIntClass::Clamp(20).value(), 20);
}

TEST(FixedRangeIntegerTest, CopyAndMove) {
  MyIntClass base = MyIntClass::Make<17>();
  EXPECT_EQ(base.value(), 17);
  MyIntClass copyed(base);
  EXPECT_EQ(copyed.value(), 17);
  MyIntClass cassigned = MyIntClass::Make<23>();
  EXPECT_EQ(cassigned.value(), 23);
  cassigned = base;
  EXPECT_EQ(cassigned.value(), 17);
  MyIntClass moved(std::move(copyed));
  EXPECT_EQ(moved.value(), 17);
  MyIntClass massigned = MyIntClass::Make<27>();
  EXPECT_EQ(massigned.value(), 27);
  massigned = std::move(cassigned);
  EXPECT_EQ(massigned.value(), 17);
}

TEST(SmbusLocationTest, TestComparator) {
  EXPECT_EQ(MyIntClass::Make<3>(), MyIntClass::Make<3>());
  EXPECT_NE(MyIntClass::Make<3>(), MyIntClass::Make<42>());
  EXPECT_GE(MyIntClass::Make<3>(), MyIntClass::Make<3>());
  EXPECT_GE(MyIntClass::Make<42>(), MyIntClass::Make<3>());
  EXPECT_GT(MyIntClass::Make<42>(), MyIntClass::Make<3>());
  EXPECT_LE(MyIntClass::Make<3>(), MyIntClass::Make<3>());
  EXPECT_LE(MyIntClass::Make<3>(), MyIntClass::Make<42>());
  EXPECT_LT(MyIntClass::Make<3>(), MyIntClass::Make<42>());
}

TEST(FixedRangeIntegerTest, IsHashable) {
  absl::flat_hash_map<MyIntClass, std::string> test_map;
  test_map[MyIntClass::Make<3>()] = "three";
  test_map[MyIntClass::Make<17>()] = "seventeen";
  test_map[MyIntClass::Make<3>()] = "Three";
  EXPECT_THAT(test_map,
              UnorderedElementsAre(Pair(MyIntClass::Make<3>(), "Three"),
                                   Pair(MyIntClass::Make<17>(), "seventeen")));
}

}  // namespace
}  // namespace ecclesia
