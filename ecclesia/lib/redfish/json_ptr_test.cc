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

#include "ecclesia/lib/redfish/json_ptr.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {
namespace {

using testing::Eq;

// Sample JSON taken from https://datatracker.ietf.org/doc/html/rfc6901
// Note that backslash characters are escaped by the json::parse function,
// so "i\\\\j" is stored as a key "i\\j" and "k\\\"l" as "k\"l".
constexpr char kSampleJson[] = R"json({
    "foo": ["bar", "baz"],
    "": 0,
    "a/b": 1,
    "c%d": 2,
    "e^f": 3,
    "g|h": 4,
    "i\\\\j": 5,
    "k\\\"l": 6,
    " ": 7,
    "m~n": 8
  })json";

TEST(JsonPtrTest, WholeDocument) {
  nlohmann::json starting_json =
      nlohmann::json::parse(kSampleJson, nullptr, /*allow_exceptions=*/false);
  EXPECT_THAT(HandleJsonPtr(starting_json, ""), Eq(starting_json));
}

TEST(JsonPtrTest, Foo) {
  nlohmann::json starting_json =
      nlohmann::json::parse(kSampleJson, nullptr, /*allow_exceptions=*/false);
  EXPECT_THAT(HandleJsonPtr(starting_json, "/foo"),
              Eq(nlohmann::json::parse(R"json(["bar", "baz"])json", nullptr,
                                       /*allow_exceptions=*/false)));
}

TEST(JsonPtrTest, FooAccess) {
  nlohmann::json starting_json =
      nlohmann::json::parse(kSampleJson, nullptr, /*allow_exceptions=*/false);
  EXPECT_THAT(HandleJsonPtr(starting_json, "/foo/0"),
              Eq(nlohmann::json::parse(R"json("bar")json", nullptr,
                                       /*allow_exceptions=*/false)));
  EXPECT_THAT(HandleJsonPtr(starting_json, "/foo/1"),
              Eq(nlohmann::json::parse(R"json("baz")json", nullptr,
                                       /*allow_exceptions=*/false)));
}

TEST(JsonPtrTest, Empty) {
  nlohmann::json starting_json =
      nlohmann::json::parse(kSampleJson, nullptr, /*allow_exceptions=*/false);
  EXPECT_THAT(HandleJsonPtr(starting_json, "/"),
              Eq(nlohmann::json::parse("0", nullptr,
                                       /*allow_exceptions=*/false)));
}

TEST(JsonPtrTest, ABEscaped) {
  nlohmann::json starting_json =
      nlohmann::json::parse(kSampleJson, nullptr, /*allow_exceptions=*/false);
  EXPECT_THAT(HandleJsonPtr(starting_json, "/a~1b"),
              Eq(nlohmann::json::parse("1", nullptr,
                                       /*allow_exceptions=*/false)));
}

TEST(JsonPtrTest, CDEscaped) {
  nlohmann::json starting_json =
      nlohmann::json::parse(kSampleJson, nullptr, /*allow_exceptions=*/false);
  EXPECT_THAT(HandleJsonPtr(starting_json, "/c%d"),
              Eq(nlohmann::json::parse("2", nullptr,
                                       /*allow_exceptions=*/false)));
}

TEST(JsonPtrTest, EF) {
  nlohmann::json starting_json =
      nlohmann::json::parse(kSampleJson, nullptr, /*allow_exceptions=*/false);
  EXPECT_THAT(HandleJsonPtr(starting_json, "/e^f"),
              Eq(nlohmann::json::parse("3", nullptr,
                                       /*allow_exceptions=*/false)));
}

TEST(JsonPtrTest, GH) {
  nlohmann::json starting_json =
      nlohmann::json::parse(kSampleJson, nullptr, /*allow_exceptions=*/false);
  EXPECT_THAT(HandleJsonPtr(starting_json, "/g|h"),
              Eq(nlohmann::json::parse("4", nullptr,
                                       /*allow_exceptions=*/false)));
}

TEST(JsonPtrTest, IJ) {
  nlohmann::json starting_json =
      nlohmann::json::parse(kSampleJson, nullptr, /*allow_exceptions=*/false);
  // The JSON parser eats one of the \ characters of each pair from kSampleJson.
  EXPECT_THAT(HandleJsonPtr(starting_json, R"json(/i\\j)json"),
              Eq(nlohmann::json::parse("5", nullptr,
                                       /*allow_exceptions=*/false)));
}

TEST(JsonPtrTest, KL) {
  nlohmann::json starting_json =
      nlohmann::json::parse(kSampleJson, nullptr, /*allow_exceptions=*/false);
  // The JSON parser eats one of the \ characters from kSampleJson.
  EXPECT_THAT(HandleJsonPtr(starting_json, R"json(/k\"l)json"),
              Eq(nlohmann::json::parse("6", nullptr,
                                       /*allow_exceptions=*/false)));
}

TEST(JsonPtrTest, Space) {
  nlohmann::json starting_json =
      nlohmann::json::parse(kSampleJson, nullptr, /*allow_exceptions=*/false);
  EXPECT_THAT(HandleJsonPtr(starting_json, "/ "),
              Eq(nlohmann::json::parse("7", nullptr,
                                       /*allow_exceptions=*/false)));
}

TEST(JsonPtrTest, MNEscaped) {
  nlohmann::json starting_json =
      nlohmann::json::parse(kSampleJson, nullptr, /*allow_exceptions=*/false);
  EXPECT_THAT(HandleJsonPtr(starting_json, "/m~0n"),
              Eq(nlohmann::json::parse("8", nullptr,
                                       /*allow_exceptions=*/false)));
}

TEST(JsonPtrTest, NotFound) {
  nlohmann::json starting_json =
      nlohmann::json::parse(kSampleJson, nullptr, /*allow_exceptions=*/false);
  EXPECT_TRUE(HandleJsonPtr(starting_json, "/something").is_discarded());
}

TEST(JsonPtrTest, InvalidPointer) {
  nlohmann::json starting_json =
      nlohmann::json::parse(kSampleJson, nullptr, /*allow_exceptions=*/false);
  EXPECT_TRUE(HandleJsonPtr(starting_json, "noslash").is_discarded());
}

}  // namespace
}  // namespace ecclesia
