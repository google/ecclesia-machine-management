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

#include "ecclesia/lib/strings/regex.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace ecclesia {
namespace {

TEST(RegexFullMatchTest, RegularMatch) {
  auto maybe_nums = RegexFullMatch<int, int, int>(
      "a1bc23def456", RE2(R"re(a(\d+)bc(\d+)def(\d+))re"));
  ASSERT_TRUE(maybe_nums.has_value());
  EXPECT_EQ(std::get<0>(*maybe_nums), 1);
  EXPECT_EQ(std::get<1>(*maybe_nums), 23);
  EXPECT_EQ(std::get<2>(*maybe_nums), 456);

  auto maybe_words = RegexFullMatch<std::string, std::string>(
      "1a2bc", RE2(R"re(1(\w+)2(\w+))re"));
  ASSERT_TRUE(maybe_words.has_value());
  EXPECT_EQ(std::get<0>(*maybe_words), "a");
  EXPECT_EQ(std::get<1>(*maybe_words), "bc");

  std::string pcie_func_uri =
      "/redfish/v1/Systems/Indus/PCIeDevices/0000:18:00/PCIeFunctions/3";
  std::string pcie_func_uri_pattern =
      R"re(\/redfish\/v1\/Systems\/(\w+)\/PCIeDevices\/([\w:]+)\/PCIeFunctions\/(\d+))re";

  auto maybe_nums_words = RegexFullMatch<std::string, std::string, int>(
      pcie_func_uri, RE2(pcie_func_uri_pattern));
  ASSERT_TRUE(maybe_nums_words.has_value());
  EXPECT_EQ(std::get<0>(*maybe_nums_words), "Indus");
  EXPECT_EQ(std::get<1>(*maybe_nums_words), "0000:18:00");
  EXPECT_EQ(std::get<2>(*maybe_nums_words), 3);
}

TEST(RegexFullMatchTest, NestedGroup) {
  auto maybe_nested_groups = RegexFullMatch<std::string, int, std::string>(
      "123abc", RE2(R"re(((\d+)(\w+)))re"));
  ASSERT_TRUE(maybe_nested_groups.has_value());
  EXPECT_EQ(std::get<0>(*maybe_nested_groups), "123abc");
  EXPECT_EQ(std::get<1>(*maybe_nested_groups), 123);
  EXPECT_EQ(std::get<2>(*maybe_nested_groups), "abc");
}

TEST(RegexFullMatchTest, NonCapturingGroup) {
  auto maybe_nums =
      RegexFullMatch<int, int>("a1b2c3", RE2(R"re(a(\d)b(?:\d)c(\d))re"));
  ASSERT_TRUE(maybe_nums.has_value());
  EXPECT_EQ(std::get<0>(*maybe_nums), 1);
  EXPECT_EQ(std::get<1>(*maybe_nums), 3);
}

}  // namespace
}  // namespace ecclesia
