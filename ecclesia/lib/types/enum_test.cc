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

#include "ecclesia/lib/types/enum.h"

#include <cstdint>

#include "gtest/gtest.h"

namespace ecclesia {
namespace {

enum class ByteEnum : uint8_t {
  kZero = 0,
  kOne = 1,
  kMax = 255,
};

TEST(ToUnderlyingTypeTest, CastHasRightType) {
  // We deliberately use a non-constexpr array with brace initialization here
  // because narrowing conversions are not allowed in this context. This should
  // only compile if ToUnderlyingType actually returns a uint8_t.
  uint8_t values[] = {ToUnderlyingType(ByteEnum::kZero),
                      ToUnderlyingType(ByteEnum::kOne),
                      ToUnderlyingType(ByteEnum::kMax)};

  // Check the actual values.
  EXPECT_EQ(values[0], 0);
  EXPECT_EQ(values[1], 1);
  EXPECT_EQ(values[2], 255);
}

}  // namespace
}  // namespace ecclesia
