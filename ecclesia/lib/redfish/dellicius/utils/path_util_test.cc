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

#include "ecclesia/lib/redfish/dellicius/utils/path_util.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/statusor.h"
#include "ecclesia/lib/redfish/testing/json_mockup.h"

namespace ecclesia {

namespace {

TEST(PathUtilTest, CheckNodeNameSplitsAsExpected) {
  // Nested nodes
  {
    std::vector<std::string> result =
        SplitNodeNameForNestedNodes(" Thresholds.UpperCritical.@odata.id ");
    std::vector<std::string> expected_nodes = {"Thresholds", "UpperCritical",
                                               "@odata.id"};
    EXPECT_EQ(result, expected_nodes);
  }
  // No nesting
  {
    std::vector<std::string> result = SplitNodeNameForNestedNodes("Thresholds");
    std::vector<std::string> expected_nodes = {"Thresholds"};
    EXPECT_EQ(result, expected_nodes);
  }
  // Empty
  {
    std::vector<std::string> result = SplitNodeNameForNestedNodes(" ");
    EXPECT_TRUE(result.empty());
  }
}

TEST(PathUtilTest, CheckNodeNameCorrectlyResolvesToJsonObj) {
  // No Nesting
  {
    auto mock_interface = NewJsonMockupInterface(R"json(
      {
        "Reading": 90
      }
    )json");
    auto var = mock_interface->GetRoot();
    absl::StatusOr<nlohmann::json> json_out =
        ResolveNodeNameToJsonObj(var, "Reading");
    EXPECT_TRUE(json_out.ok());
    EXPECT_EQ(json_out->get<double>(), 90);
  }

  // Multiple nested objects
  {
    auto mock_interface = NewJsonMockupInterface(R"json(
      {
        "Thresholds": {
          "UpperCritical": {
              "Reading": 90
          }
        }
      }
    )json");
    auto var = mock_interface->GetRoot();
    absl::StatusOr<nlohmann::json> json_out =
        ResolveNodeNameToJsonObj(var, "Thresholds.UpperCritical.Reading");
    EXPECT_TRUE(json_out.ok());
    EXPECT_EQ(json_out->get<double>(), 90);
  }

  // Null Redfish Object
  {
    auto mock_interface = NewJsonMockupInterface(R"json({})json");
    auto var = mock_interface->GetRoot();
    absl::StatusOr<nlohmann::json> json_out =
        ResolveNodeNameToJsonObj(var, "Thresholds.UpperCritical.Reading");
    EXPECT_TRUE(!json_out.ok());
  }

  // Partial resolution also gives null object.
  {
    auto mock_interface = NewJsonMockupInterface(R"json(
      {
        "Thresholds": {
          "UpperCritical": {
          }
        }
      }
    )json");
    auto var = mock_interface->GetRoot();
    absl::StatusOr<nlohmann::json> json_out =
        ResolveNodeNameToJsonObj(var, "Thresholds.UpperCritical.Reading");
    EXPECT_TRUE(!json_out.ok());
  }
}

}  // namespace

}  // namespace ecclesia
