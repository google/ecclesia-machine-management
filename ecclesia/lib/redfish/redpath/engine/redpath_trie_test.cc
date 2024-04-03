/*
 * Copyright 2024 Google LLC
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

#include "ecclesia/lib/redfish/redpath/engine/redpath_trie.h"

#include <memory>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/testing/status.h"

namespace ecclesia {

namespace {

using testing::UnorderedElementsAreArray;

// MATCHER comparing flat_hash_set<vector<string>>
MATCHER_P(FlatHashSetOfStringVectorsEq, expected, "") {
  if (arg.size() != expected.size()) {
    return false;
  }

  absl::flat_hash_set<std::vector<std::string>> copy_arg(arg);
  for (const auto& expected_vec : expected) {
    auto it = copy_arg.find(expected_vec);
    if (it == copy_arg.end()) {
      return false;
    }
    copy_arg.erase(it);
  }

  return copy_arg.empty();
}

TEST(RedPathTrieTest, RedPathTrieIsBuiltCorrectly) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(
        query_id: "TrieValidation"
        subquery {
          subquery_id: "Systems"
          redpath: "/Systems[*]"
          properties { property: "Id" type: STRING }
        }
        subquery {
          subquery_id: "Processors"
          root_subquery_ids: "Systems"
          redpath: "/Processors"
          properties { property: "Id" type: STRING }
        }
        subquery {
          subquery_id: "Memory"
          root_subquery_ids: "Systems"
          redpath: "/Memory"
          properties { property: "Id" type: STRING }
        }
        subquery {
          subquery_id: "Chassis"
          redpath: "/Chassis[Id=1]"
          properties { property: "Id" type: STRING }
        }
        subquery {
          subquery_id: "Sensors"
          root_subquery_ids: "Chassis"
          redpath: "/Sensors"
          properties { property: "Id" type: STRING }
        }
      )pb");

  // Expected Trie Structure
  //                 <Root>
  //                  / \
  //            Systems  Chassis
  //                /     \
  //               *      Id=1
  //              / \         \
  //    Processors  Memory   Sensors

  RedPathTrieBuilder redpath_trie_builder(&query);
  absl::StatusOr<std::unique_ptr<RedPathTrieNode>> redpath_trie =
      redpath_trie_builder.CreateRedPathTrie();
  EXPECT_THAT(redpath_trie, IsOk());
  EXPECT_TRUE(*redpath_trie != nullptr);

  // Validate Root node has /Systems as child node.
  auto systems_node = (*redpath_trie)
                          ->expression_to_trie_node.find(RedPathExpression(
                              RedPathExpression::Type::kNodeName, "Systems"));
  EXPECT_TRUE(systems_node != (*redpath_trie)->expression_to_trie_node.end());

  // Validate /Systems node has `*` as child node and the node marks the end
  // of a RedPath expression by having subquery_id `Systems`.
  auto systems_wildcard_node =
      systems_node->second->expression_to_trie_node.find(
          RedPathExpression(RedPathExpression::Type::kPredicate, "*"));
  EXPECT_TRUE(systems_wildcard_node !=
              systems_node->second->expression_to_trie_node.end());
  EXPECT_THAT(systems_wildcard_node->second->subquery_id, "Systems");

  // Validate /Systems[*] node has `Processors` and `Memory` as child nodes.
  auto processors_node =
      systems_wildcard_node->second->expression_to_trie_node.find(
          RedPathExpression(RedPathExpression::Type::kNodeName, "Processors"));
  EXPECT_TRUE(processors_node !=
              systems_wildcard_node->second->expression_to_trie_node.end());
  EXPECT_THAT(processors_node->second->subquery_id, "Processors");

  auto memory_node =
      systems_wildcard_node->second->expression_to_trie_node.find(
          RedPathExpression(RedPathExpression::Type::kNodeName, "Memory"));
  EXPECT_TRUE(memory_node !=
              systems_wildcard_node->second->expression_to_trie_node.end());
  EXPECT_THAT(memory_node->second->subquery_id, "Memory");

  // Validate root node as `Chassis` as child node.
  auto chassis_node = (*redpath_trie)
                          ->expression_to_trie_node.find(RedPathExpression(
                              RedPathExpression::Type::kNodeName, "Chassis"));
  EXPECT_TRUE(chassis_node != (*redpath_trie)->expression_to_trie_node.end());

  // Validate /Chassis node has `Id=1` as child node.
  auto chassis_child_node = chassis_node->second->expression_to_trie_node.find(
      RedPathExpression(RedPathExpression::Type::kPredicate, "Id=1"));
  EXPECT_TRUE(chassis_child_node !=
              chassis_node->second->expression_to_trie_node.end());
  EXPECT_THAT(chassis_child_node->second->subquery_id, "Chassis");

  // Validate /Chassis[Id=1] has `Sensors` as child node.
  auto sensors_node = chassis_child_node->second->expression_to_trie_node.find(
      RedPathExpression(RedPathExpression::Type::kNodeName, "Sensors"));
  EXPECT_TRUE(sensors_node !=
              chassis_child_node->second->expression_to_trie_node.end());
  EXPECT_THAT(sensors_node->second->subquery_id, "Sensors");
}

TEST(RedPathTrieTest, RedPathTrieDoesNotBuildOnMalformedPath) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(
        query_id: "Processor"
        subquery {
          subquery_id: "Processor"
          redpath: "/Systems[*/Processors[ProcessorType=CPU]"
          properties { property: "Id" type: STRING }
        }
      )pb");
  EXPECT_THAT(RedPathTrieBuilder(&query).CreateRedPathTrie(),
              IsStatusInvalidArgument());
}

TEST(RedPathTrieTest, RedPathTrieDoesNotBuildOnSubqueryLooop) {
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

  RedPathTrieBuilder redpath_trie_builder(&query);
  EXPECT_THAT(redpath_trie_builder.CreateRedPathTrie(), IsStatusInternal());
}

TEST(RedPathTrieTest, RedPathTrieBuildsExecutionSequenceCorrectly) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(
        query_id: "Processor"
        subquery {
          subquery_id: "Processor"
          redpath: "/Systems[*]/Processors[ProcessorType=CPU]"
          properties { property: "Id" type: STRING }
        }
        subquery {
          subquery_id: "ProcessorMetrics"
          root_subquery_ids: "Processor"
          redpath: "/Metrics"
          properties { property: "CorrectableOtherErrorCount" type: INT64 }
        }
        subquery {
          subquery_id: "Core"
          root_subquery_ids: "Processor"
          redpath: "/SubProcessors[*]"
          properties { property: "Id" type: STRING }
        }
        subquery {
          subquery_id: "Thread"
          root_subquery_ids: "Core"
          redpath: "/SubProcessors[*]"
          properties { property: "Id" type: STRING }
        }
      )pb");

  RedPathTrieBuilder redpath_trie_builder(&query);
  ASSERT_THAT(redpath_trie_builder.CreateRedPathTrie(), IsOk());

  // Check subquery execution sequence is ordered correctly.
  absl::StatusOr<const absl::flat_hash_set<std::vector<std::string>>*>
      actual_subquery_sequences = redpath_trie_builder.GetSubquerySequences();
  EXPECT_THAT(actual_subquery_sequences, IsOk());
  EXPECT_TRUE(*actual_subquery_sequences != nullptr);

  const absl::flat_hash_set<std::vector<std::string>>
      expected_subquery_sequences = {{"Processor", "ProcessorMetrics"},
                                     {"Processor", "Core", "Thread"}};

  EXPECT_THAT(**actual_subquery_sequences,
              FlatHashSetOfStringVectorsEq(expected_subquery_sequences));
}

TEST(RedPathTrieTest, RedPathTrieToStringIsAsExpected) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(
        query_id: "Processor"
        subquery {
          subquery_id: "Processor"
          redpath: "/Systems[*]/Processors[ProcessorType=CPU]"
          properties { property: "Id" type: STRING }
        }
      )pb");

  RedPathTrieBuilder redpath_trie_builder(&query);
  absl::StatusOr<std::unique_ptr<RedPathTrieNode>> redpath_trie =
      redpath_trie_builder.CreateRedPathTrie();
  ASSERT_THAT(redpath_trie, IsOk());
  ASSERT_TRUE(*redpath_trie != nullptr);

  std::string expected_redpath_trie_string =
      "RedPathPrefix: \n"
      "RedPathPrefix: /Systems\n"
      "RedPathPrefix: /Systems[*]\n"
      "RedPathPrefix: /Systems[*]/Processors\n"
      "RedPathPrefix: /Systems[*]/Processors[ProcessorType=CPU], Subquery: "
      "Processor\n";

  EXPECT_THAT(absl::StrSplit((*redpath_trie)->ToString(), '\n'),
              UnorderedElementsAreArray(
                  absl::StrSplit(expected_redpath_trie_string, '\n')));
}

TEST(RedPathTrieTest, RedPathTrieIsBuiltJsonAndUriCorrectly) {
  DelliciusQuery query = ParseTextProtoOrDie(
      R"pb(
        query_id: "SensorsSubTreeTest"
        subquery {
          subquery_id: "Sensors"
          uri: "/redfish/v1/Chassis/chassis/Sensors/indus_cpu1_pwmon"
          properties { property: "Name" type: STRING }
        }
      )pb");

  // Expected Trie Structure
  //                 <Root>
  //                  /
  //            /redfish/v1/Chassis/chassis/Sensors/indus_cpu1_pwmon

  RedPathTrieBuilder redpath_trie_builder(&query);
  absl::StatusOr<std::unique_ptr<RedPathTrieNode>> redpath_trie =
      redpath_trie_builder.CreateRedPathTrie();
  EXPECT_THAT(redpath_trie, IsOk());
  EXPECT_TRUE(*redpath_trie != nullptr);

  // Validate Root node has uri path as child node and the node marks the
  // end of a RedPath expression by having subquery_id `Sensors`.
  auto sensors_uri_node =
      (*redpath_trie)
          ->expression_to_trie_node.find(RedPathExpression(
              RedPathExpression::Type::kNodeNameUriPointer,
              "/redfish/v1/Chassis/chassis/Sensors/indus_cpu1_pwmon"));
  EXPECT_TRUE(sensors_uri_node !=
              (*redpath_trie)->expression_to_trie_node.end());
  EXPECT_THAT(sensors_uri_node->second->subquery_id, "Sensors");
}

}  // namespace

}  // namespace ecclesia
