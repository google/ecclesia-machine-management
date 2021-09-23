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

#include "ecclesia/magent/redfish/core/json_helper.h"

#include <string>

#include "gtest/gtest.h"
#include "absl/status/statusor.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {
namespace {

const std::string kTestJson = R"json({
"dummy": 1,
"Oem": {
  "Google": {
    "Key": "value"
  }
}
})json";

TEST(JsonHelper, JsonDrillDown) {
  nlohmann::json json = nlohmann::json::parse(kTestJson, nullptr, false);
  auto value = JsonDrillDown(json, {"Oem", "Google", "Key",});
  EXPECT_TRUE(value.ok());
  EXPECT_EQ((*value)->get<std::string>(), "value");
}

TEST(JsonHelper, JsonDrillDownHalfWay) {
  nlohmann::json json = nlohmann::json::parse(kTestJson, nullptr, false);
  auto key_value = JsonDrillDown(json, {"Oem", "Google",});
  EXPECT_TRUE(key_value.ok());
  std::string value = (**key_value)["Key"].get<std::string>();
  EXPECT_EQ(value, "value");
}

TEST(JsonHelper, JsonDrillDownInvalidKey) {
  nlohmann::json json = nlohmann::json::parse(kTestJson, nullptr, false);
  // Guugle is not Google.
  auto value = JsonDrillDown(json, {"Oem", "Guugle", "Key"});
  EXPECT_FALSE(value.ok());
}

TEST(JsonHelper, JsonDrillDownOverDrill) {
  nlohmann::json json = nlohmann::json::parse(kTestJson, nullptr, false);
  auto data = JsonDrillDown(json, {"Oem", "Google", "Key", "???"});
  EXPECT_FALSE(data.ok());
}

TEST(JsonHelper, JsonDrillDownNoDrill) {
  nlohmann::json json = nlohmann::json::parse(kTestJson, nullptr, false);
  auto data = JsonDrillDown(json, {});
  EXPECT_EQ(*data, &json);
}

}  // namespace
}  // namespace ecclesia
