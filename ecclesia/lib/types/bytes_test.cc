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

#include "ecclesia/lib/types/bytes.h"

#include "gtest/gtest.h"

namespace ecclesia {
namespace {

TEST(CapacityTest, SmallValues) {
  EXPECT_EQ(BytesToMetricPrefix(0, 1000), "0");
  EXPECT_EQ(BytesToMetricPrefix(1, 1000), "1");
  EXPECT_EQ(BytesToMetricPrefix(128, 1000), "128");
  EXPECT_EQ(BytesToMetricPrefix(1000, 1024), "1000");
}

TEST(CapacityTest, PowersOfX) {
  EXPECT_EQ(BytesToMetricPrefix(1000, 1000), "1k");
  EXPECT_EQ(BytesToMetricPrefix(1000000, 1000), "1M");
  EXPECT_EQ(BytesToMetricPrefix(1000000000, 1000), "1G");
  EXPECT_EQ(BytesToMetricPrefix(1000000000000, 1000), "1T");
  EXPECT_EQ(BytesToMetricPrefix(1000000000000000, 1000), "1000T");
  EXPECT_EQ(BytesToMetricPrefix(1024, 1024), "1k");
  EXPECT_EQ(BytesToMetricPrefix(1048576, 1024), "1M");
  EXPECT_EQ(BytesToMetricPrefix(1073741824, 1024), "1G");
  EXPECT_EQ(BytesToMetricPrefix(1099511627776, 1024), "1T");
  EXPECT_EQ(BytesToMetricPrefix(1125899906842624, 1024), "1024T");
}

TEST(CapacityTest, EvenValues) {
  EXPECT_EQ(BytesToMetricPrefix(8000, 1000), "8k");
  EXPECT_EQ(BytesToMetricPrefix(32000000, 1000), "32M");
  EXPECT_EQ(BytesToMetricPrefix(128000000000, 1000), "128G");
  EXPECT_EQ(BytesToMetricPrefix(512000000000000, 1000), "512T");
  EXPECT_EQ(BytesToMetricPrefix(8192, 1024), "8k");
  EXPECT_EQ(BytesToMetricPrefix(33554432, 1024), "32M");
  EXPECT_EQ(BytesToMetricPrefix(137438953472, 1024), "128G");
  EXPECT_EQ(BytesToMetricPrefix(562949953421312, 1024), "512T");
}

TEST(CapacityTest, UnevenValues) {
  EXPECT_EQ(BytesToMetricPrefix(1001, 1000), "1001");
  EXPECT_EQ(BytesToMetricPrefix(1001000, 1000), "1001k");
  EXPECT_EQ(BytesToMetricPrefix(1000001000, 1000), "1000001k");
  EXPECT_EQ(BytesToMetricPrefix(1001000000, 1000), "1001M");
  EXPECT_EQ(BytesToMetricPrefix(1025, 1024), "1025");
  EXPECT_EQ(BytesToMetricPrefix(1049600, 1024), "1025k");
  EXPECT_EQ(BytesToMetricPrefix(1073742848, 1024), "1048577k");
  EXPECT_EQ(BytesToMetricPrefix(1074790400, 1024), "1025M");
}

}  // namespace
}  // namespace ecclesia
