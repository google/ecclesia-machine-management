/*
 * Copyright 2026 Google LLC
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

#include "ecclesia/lib/redfish/result.h"

#include <sstream>
#include <string>

#include "gtest/gtest.h"

namespace ecclesia {
namespace {

TEST(ResultTest, EqualityAndInequality) {
  Result<int> r1{"/phys", 42};
  Result<int> r2{"/phys", 42};
  Result<int> r3{"/phys", 43};
  Result<int> r4{"/phys/other", 42};

  // Test operator `==`.
  EXPECT_TRUE(r1 == r2);
  EXPECT_FALSE(r1 == r3);
  EXPECT_FALSE(r1 == r4);

  // Test operator `!=`.
  EXPECT_FALSE(r1 != r2);
  EXPECT_TRUE(r1 != r3);
  EXPECT_TRUE(r1 != r4);
}

TEST(ResultTest, OutputStreamOperator) {
  Result<int> r{"/phys", 42};
  std::stringstream ss;
  ss << r;
  EXPECT_EQ(ss.str(), "{ devpath: \"/phys\" value: 42 }");
}

}  // namespace
}  // namespace ecclesia
