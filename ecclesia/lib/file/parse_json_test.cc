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

#include "ecclesia/lib/file/parse_json.h"

#include <array>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "ecclesia/lib/file/cc_embed_interface.h"
#include "ecclesia/lib/file/json_files.h"
#include "ecclesia/lib/testing/status.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {
namespace {

TEST(ParseEmbeddedJson, Works) {
  ASSERT_EQ(ecclesia_testdata::kJsonFiles.size(), 2);
  ASSERT_EQ(ecclesia_testdata::kJsonFiles[0].name, "test_data/json.json");
  nlohmann::json expected_contents;
  expected_contents["A"]["1"] = nlohmann::json::object();
  expected_contents["A"]["2"] = nlohmann::json::array({"item1", "item2"});
  expected_contents["B"] = nlohmann::json::object();

  EXPECT_THAT(ParseJsonValueFromEmbeddedFile("test_data/json.json",
                                             ecclesia_testdata::kJsonFiles),
              IsOkAndHolds(expected_contents));
}

TEST(ParseEmbeddedJson, ParseNonJsonFileFails) {
  // Ensure utility fails for invalid Json data (.txt is to avoid linter errors)
  EXPECT_THAT(ParseJsonValueFromEmbeddedFile("test_data/json_bad.txt",
                                             ecclesia_testdata::kJsonFiles),
              IsStatusInternal());
}

TEST(ParseEmbeddedJson, ParseNonexistentFileFails) {
  // Ensure utility fails for nonexistent file.
  EXPECT_THAT(ParseJsonValueFromEmbeddedFile("test_data/invalid.nonexistent",
                                             ecclesia_testdata::kJsonFiles),
              IsStatusNotFound());
}

}  // namespace
}  // namespace ecclesia
