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

#include "ecclesia/lib/redfish/dellicius/utils/for_each.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_set.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"

namespace ecclesia {
namespace {

using ::ecclesia::ParseTextProtoOrDie;

TEST(ForEachDataSetTest, SmokeTest) {
  std::vector<ecclesia::DelliciusQueryResult> test_result = {
      ParseTextProtoOrDie(
          R"pb(query_id: "test_query_0"
               subquery_output_by_id {
                 key: "key"
                 value {
                   data_sets {
                     devpath: "/phys/dimm0"
                     properties { name: "name" string_value: "dimm0" }
                   }
                   data_sets {
                     devpath: "/phys/dimm1"
                     properties { name: "name" string_value: "dimm1" }
                   }
                 }
               })pb"),
      ParseTextProtoOrDie(
          R"pb(query_id: "test_query_1"
               subquery_output_by_id {
                 key: "key"
                 value {
                   data_sets {
                     devpath: "/phys/dimm2"
                     properties { name: "name" string_value: "dimm2" }
                   }
                 }
               })pb")};

  absl::flat_hash_set<std::string> not_visited = {"/phys/dimm0", "/phys/dimm1",
                                                  "/phys/dimm2"};

  ForEachDataSet(test_result, [&](const ecclesia::SubqueryDataSet &data) {
    not_visited.erase(data.devpath());
  });

  EXPECT_TRUE(not_visited.empty());
}

}  // namespace
}  // namespace ecclesia
