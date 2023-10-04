/*
 * Copyright 2023 Google LLC
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

#include "ecclesia/lib/redfish/dellicius/utils/join.h"

#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"

namespace ecclesia {

namespace {

using ::testing::UnorderedPointwise;

TEST(JoinTest, TestSubqueryJoin) {
  DelliciusQuery query = ParseTextAsProtoOrDie<DelliciusQuery>(R"pb(
    query_id: "SensorCollectorWithChassisLinks"
    subquery {
      subquery_id: "ChassisAssembly"
      redpath: "/Chassis[*]"
      properties { property: "Name" type: STRING }
    }
    subquery {
      subquery_id: "ChassisMakeModel"
      redpath: "/Chassis[*]"
      properties { property: "Name" type: STRING }
    }
    subquery {
      subquery_id: "FanSensorCollector"
      root_subquery_ids: "ChassisAssembly"
      redpath: "/Sensors[ReadingType=Rotational]"
      properties { property: "Name" type: STRING }
    }
    subquery {
      subquery_id: "ThermalSensorCollector"
      root_subquery_ids: "ChassisAssembly"
      root_subquery_ids: "ChassisMakeModel"
      redpath: "/Sensors[ReadingType=Temperature]"
      properties { property: "Name" type: STRING }
    }
    subquery {
      subquery_id: "SensorRelatedItem"
      root_subquery_ids: "ThermalSensorCollector"
      redpath: "/RelatedItem"
      properties { property: "Name" type: STRING }
    }
  )pb");

  absl::flat_hash_set<std::vector<std::string>> all_joined_subqueries;

  EXPECT_TRUE(JoinSubqueries(query, all_joined_subqueries).ok());

  absl::flat_hash_set<std::vector<std::string>> all_joined_subqueries_expected =
      {{"ChassisAssembly", "FanSensorCollector"},
       {"ChassisAssembly", "ThermalSensorCollector", "SensorRelatedItem"},
       {"ChassisMakeModel", "ThermalSensorCollector", "SensorRelatedItem"}};

  EXPECT_THAT(
      all_joined_subqueries,
      UnorderedPointwise(testing::Eq(), all_joined_subqueries_expected));
}

TEST(JoinTest, TestSubqueryLoopDetectedError) {
  DelliciusQuery query = ParseTextAsProtoOrDie<DelliciusQuery>(R"pb(
    query_id: "SensorCollectorWithChassisLinks"
    subquery {
      subquery_id: "ChassisAssembly"
      redpath: "/Chassis[*]"
      properties { property: "Name" type: STRING }
    }

    subquery {
      subquery_id: "ThermalSensorCollector"
      root_subquery_ids: "ChassisAssembly"
      root_subquery_ids: "SensorRelatedItem"
      redpath: "/Sensors[ReadingType=Temperature]"
      properties { property: "Name" type: STRING }
    }
    subquery {
      subquery_id: "SensorRelatedItem"
      root_subquery_ids: "ThermalSensorCollector"
      redpath: "/RelatedItem"
      properties { property: "Name" type: STRING }
    }
  )pb");

  absl::flat_hash_set<std::vector<std::string>> all_joined_subqueries;

  EXPECT_EQ(JoinSubqueries(query, all_joined_subqueries).code(),
            absl::StatusCode::kInternal);
}

}  // namespace

}  // namespace ecclesia
