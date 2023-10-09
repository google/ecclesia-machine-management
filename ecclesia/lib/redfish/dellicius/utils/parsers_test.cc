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
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/file/cc_embed_interface.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/interface.h"
#include "ecclesia/lib/redfish/dellicius/engine/query_rules.pb.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/testing/status.h"

namespace ecclesia {

namespace {

using ::testing::Pair;
using ::testing::UnorderedElementsAre;

MATCHER_P(GetParamsEq, test_str, "") {
  return arg.expand.has_value() && arg.expand->ToString() == test_str;
}

TEST(ParserTest, ValidQueryRulesParseCorrectly) {
  constexpr absl::string_view kQueryRuleStr = R"pb(
    query_id_to_params_rule {
      key: "Assembly"
      value {
        redpath_prefix_with_params {
          redpath: "/Systems"
          expand_configuration { level: 1 type: BOTH }
        }
      }
    })pb";

  const std::vector<EmbeddedFile> query_rules = {
      {.name = "", .data = kQueryRuleStr}};

  absl::StatusOr<absl::flat_hash_map<std::string, RedPathRedfishQueryParams>>
      query_to_params = ParseQueryRulesFromEmbeddedFiles(query_rules);

  GetParams test_params{
      .expand = RedfishQueryParamExpand(
          {.type = RedfishQueryParamExpand::kBoth, .levels = 1})};
  ASSERT_TRUE(test_params.expand.has_value());

  EXPECT_THAT(
      query_to_params,
      IsOkAndHolds(UnorderedElementsAre(Pair(
          "Assembly",
          UnorderedElementsAre(Pair(
              "/Systems", GetParamsEq(test_params.expand->ToString())))))));
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

}  // namespace

}  // namespace ecclesia
