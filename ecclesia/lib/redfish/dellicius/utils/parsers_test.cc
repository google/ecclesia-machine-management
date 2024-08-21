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

#include "ecclesia/lib/redfish/dellicius/utils/parsers.h"

#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/file/cc_embed_interface.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/interface.h"
#include "ecclesia/lib/redfish/dellicius/engine/query_rules.pb.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/redfish/redpath/engine/query_planner.h"
#include "ecclesia/lib/testing/status.h"

namespace ecclesia {

namespace {

using ::testing::Pair;
using ::testing::UnorderedElementsAre;

MATCHER_P(GetParamsEq, test_str, "") {
  // atleast one param is set
  if (!arg.top.has_value() && !arg.expand.has_value()) {
    return false;
  }
  std::string expected_str;
  if (arg.top.has_value()) {
    expected_str = arg.top->ToString();
  }
  if (arg.expand.has_value()) {
    if (!expected_str.empty()) {
      expected_str += "&";
    }
    expected_str += arg.expand->ToString();
  }
  if (expected_str != test_str) {
    return false;
  }
  return true;
}

TEST(ParserTest, ValidQueryRulesParseCorrectly) {
  constexpr absl::string_view kQueryRuleStr = R"pb(
    query_id_to_params_rule {
      key: "Assembly"
      value {
        redpath_prefix_with_params {
          redpath: "/Systems"
          top_configuration { num_members: 10 }
          expand_configuration { level: 1 type: BOTH }
          uri_prefix: "/tlbmc"
        }
      }
    })pb";

  const std::vector<EmbeddedFile> query_rules = {
      {.name = "", .data = kQueryRuleStr}};

  absl::StatusOr<absl::flat_hash_map<std::string, RedPathRedfishQueryParams>>
      query_to_params = ParseQueryRulesFromEmbeddedFiles(query_rules);

  GetParams test_params{
      .top = RedfishQueryParamTop(10),
      .expand = RedfishQueryParamExpand(
          {.type = RedfishQueryParamExpand::ExpandType::kBoth, .levels = 1}),
      .uri_prefix = "/tlbmc"};
  ASSERT_TRUE(test_params.expand.has_value());
  ASSERT_TRUE(test_params.top.has_value());

  EXPECT_THAT(
      query_to_params,
      IsOkAndHolds(UnorderedElementsAre(Pair(
          "Assembly",
          UnorderedElementsAre(Pair(
              "/Systems", GetParamsEq(test_params.ToString())))))));
}

TEST(ParserTest, InvalidTopConfigurationParseFail) {
  constexpr absl::string_view kQueryRuleStr = R"pb(
    query_id_to_params_rule {
      key: "Assembly"
      value {
        redpath_prefix_with_params {
          redpath: "/Systems"
          top_configuration { num_members: -1 }
          expand_configuration { level: 1 type: BOTH }
        }
      }
    })pb";

  const std::vector<EmbeddedFile> query_rules = {
      {.name = "", .data = kQueryRuleStr}};

  absl::StatusOr<absl::flat_hash_map<std::string, RedPathRedfishQueryParams>>
      query_to_params = ParseQueryRulesFromEmbeddedFiles(query_rules);

  EXPECT_THAT(query_to_params, IsStatusInternal());
}

TEST(ParserTest, MalformedQueryRulesReturnError) {
  constexpr absl::string_view kQueryRuleStr = R"pb(
    query_id_to_params_rule { key: "Assembly"
                              value { redpath_prefix_with_params {
                                redpath: "/Systems"
                                expand_configuration { level: 1 type: BOTH }
                              })pb";

  const std::vector<EmbeddedFile> query_rules = {
      {.name = "query_rules.pb", .data = kQueryRuleStr}};

  absl::StatusOr<absl::flat_hash_map<std::string, RedPathRedfishQueryParams>>
      query_to_params = ParseQueryRulesFromEmbeddedFiles(query_rules);
  EXPECT_THAT(query_to_params, IsStatusInternal());
}

TEST(ParseQueryRuleParams, ParseQueryRuleParamsCorrectly) {
  QueryRules::RedPathPrefixSetWithQueryParams rule = ParseTextProtoOrDie(
      R"pb(redpath_prefix_with_params {
             redpath: "/Systems[*]/Memory"
             expand_configuration { level: 1 type: NO_LINKS }
           }
           redpath_prefix_with_params {
             redpath: "/Systems[*]/Processors"
             expand_configuration { level: 2 type: ONLY_LINKS }
           }
           redpath_prefix_with_params {
             redpath: "/Chassis[*]/Sensors"
             expand_configuration { level: 3 type: BOTH }
           }
           redpath_prefix_with_params {
             redpath: "/Undefined"
             expand_configuration { level: 4 }
           }
      )pb");

  EXPECT_THAT(
      ParseQueryRuleParams(std::move(rule)),
      UnorderedElementsAre(
          Pair("/Systems[*]/Memory",
               GetParamsEq(
                   RedfishQueryParamExpand(
                       {.type = RedfishQueryParamExpand::ExpandType::kNotLinks,
                        .levels = 1})
                       .ToString())),
          Pair("/Systems[*]/Processors",
               GetParamsEq(
                   RedfishQueryParamExpand(
                       {.type = RedfishQueryParamExpand::ExpandType::kLinks,
                        .levels = 2})
                       .ToString())),
          Pair("/Chassis[*]/Sensors",
               GetParamsEq(
                   RedfishQueryParamExpand(
                       {.type = RedfishQueryParamExpand::ExpandType::kBoth,
                        .levels = 3})
                       .ToString()))));
}

TEST(ParseQueryRuleParams, CanCreateRedPathRules) {
  QueryRules::RedPathPrefixSetWithQueryParams rule = ParseTextProtoOrDie(
      R"pb(redpath_prefix_with_params {
             redpath: "/Systems[*]/Memory"
             expand_configuration { level: 1 type: NO_LINKS }
             subscribe: true
           }
           redpath_prefix_with_params {
             redpath: "/Undefined"
             expand_configuration { level: 4 }
           }
      )pb");

  RedPathRules redpath_rules = CreateRedPathRules(std::move(rule));
  EXPECT_THAT(
      redpath_rules.redpath_to_query_params,
      UnorderedElementsAre(
          Pair("/Systems[*]/Memory",
               GetParamsEq(
                   RedfishQueryParamExpand(
                       {.type = RedfishQueryParamExpand::ExpandType::kNotLinks,
                        .levels = 1})
                       .ToString()))));
  EXPECT_THAT(redpath_rules.redpaths_to_subscribe,
              UnorderedElementsAre("/Systems[*]/Memory"));
}

}  // namespace

}  // namespace ecclesia
