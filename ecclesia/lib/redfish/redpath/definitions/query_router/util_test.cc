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

#include "ecclesia/lib/redfish/redpath/definitions/query_router/util.h"

#include <optional>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "absl/time/time.h"
#include "ecclesia/lib/apifs/apifs.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/redfish/dellicius/engine/query_rules.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_engine/query_spec.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_router/query_router_spec.pb.h"
#include "ecclesia/lib/status/test_macros.h"
#include "ecclesia/lib/testing/proto.h"
#include "ecclesia/lib/testing/status.h"
#include "google/protobuf/text_format.h"

namespace ecclesia {
namespace {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::MockFunction;
using ::testing::Pair;
using ::testing::Return;
using ::testing::TestWithParam;
using ::testing::UnorderedElementsAre;
using ::testing::ValuesIn;

constexpr absl::string_view kRootDir = "/test/";
constexpr int kQueryATimeoutSecs = 10;
constexpr int kQueryATimeoutNanos = 500;

// Custom matcher to match QueryInfo struct.
MATCHER_P(QueryInfoEq, query_info, "") {
  return ExplainMatchResult(EqualsProto(query_info.query), arg.query,
                            result_listener) &&
         ExplainMatchResult(EqualsProto(query_info.rule), arg.rule,
                            result_listener) &&
         (query_info.timeout.has_value()
              ? ExplainMatchResult(Eq(*query_info.timeout), arg.timeout,
                                   result_listener)
              : true);
}

class GetQuerySpecTest : public testing::Test {
 protected:
  GetQuerySpecTest()
      : fs_(GetTestTempdirPath()), apifs_(GetTestTempdirPath("test")) {}

  void SetUp() override {
    fs_.CreateDir(kRootDir);

    // Create Sample queries
    query_a_ = ParseTextProtoOrDie(
        R"pb(query_id: "query_a"
             property_sets {
               properties { property: "property_a" type: STRING }
             }
        )pb");
    CreateFile("query_a.textproto", query_a_);

    query_a_other_ = ParseTextProtoOrDie(
        R"pb(query_id: "query_a"
             property_sets {
               properties { property: "property_a_other" type: BOOLEAN }
             }
        )pb");
    CreateFile("query_a_other.textproto", query_a_other_);

    query_b_ = ParseTextProtoOrDie(
        R"pb(query_id: "query_b"
             property_sets {
               properties { property: "property_b" type: STRING }
             }
        )pb");
    CreateFile("query_b.textproto", query_b_);

    query_c_ = ParseTextProtoOrDie(
        R"pb(query_id: "query_c"
             property_sets {
               properties { property: "property_c" type: STRING }
             }
        )pb");
    CreateFile("query_c.textproto", query_c_);

    // Create Sample rule
    query_rules_ = ParseTextProtoOrDie(R"pb(
      query_id_to_params_rule {
        key: "query_a"
        value {
          redpath_prefix_with_params {
            expand_configuration { level: 1 type: ONLY_LINKS }
          }
        }
      }
      query_id_to_params_rule {
        key: "query_b"
        value {
          redpath_prefix_with_params {
            expand_configuration { level: 1 type: BOTH }
          }
        }
      }
    )pb");

    CreateFile("query_rules.textproto", query_rules_);

    query_rules_other_ = ParseTextProtoOrDie(R"pb(
      query_id_to_params_rule {
        key: "query_a"
        value {
          redpath_prefix_with_params {
            expand_configuration { level: 5 type: BOTH }
          }
        }
      }
    )pb");

    CreateFile("query_rules_other.textproto", query_rules_other_);
  }

  template <typename T>
  void CreateFile(absl::string_view filename, const T &item) {
    std::string contents;
    google::protobuf::TextFormat::PrintToString(item, &contents);
    fs_.WriteFile(absl::StrCat(kRootDir, filename), contents);
  }

  TestFilesystem fs_;
  ApifsDirectory apifs_;
  DelliciusQuery query_a_;
  DelliciusQuery query_a_other_;
  DelliciusQuery query_b_;
  DelliciusQuery query_c_;
  QueryRules query_rules_;
  QueryRules query_rules_other_;
};

TEST_F(GetQuerySpecTest, RouterSpecWithServerType) {
  QueryRouterSpec router_spec = ParseTextProtoOrDie(absl::Substitute(
      R"pb(
        selection_specs {
          key: "query_a"
          value {
            query_selection_specs {
              select {
                server_type: SERVER_TYPE_BMCWEB
                server_tag: "server_1"
                server_tag: "server_2"
                server_tag: "server_3"
              }
              query_and_rule_path {
                query_path: "$0/query_a.textproto"
                rule_path: "$0/query_rules.textproto"
              }
              timeout { seconds: $1 nanos: $2 }
            }
          }
        }
        selection_specs {
          key: "query_b"
          value {
            query_selection_specs {
              select {
                server_type: SERVER_TYPE_BMCWEB
                server_tag: "server_1"
                server_tag: "server_2"
              }
              query_and_rule_path {
                query_path: "$0/query_b.textproto"
                rule_path: "$0/query_rules.textproto"
              }
            }
          }
        }
        selection_specs {
          key: "query_c"
          value {
            query_selection_specs {
              select { server_type: SERVER_TYPE_BMCWEB }
              query_and_rule_path { query_path: "$0/query_c.textproto" }
            }
          }
        }
      )pb",
      apifs_.GetPath(), kQueryATimeoutSecs, kQueryATimeoutNanos));

  // All queries are applicable for server_1
  ECCLESIA_ASSIGN_OR_FAIL(
      QuerySpec query_spec,
      GetQuerySpec(router_spec, "server_1",
                   SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB));
  EXPECT_THAT(
      query_spec.query_id_to_info,
      UnorderedElementsAre(
          Pair("query_a",
               QueryInfoEq(QuerySpec::QueryInfo{
                   .query = query_a_,
                   .rule = query_rules_.query_id_to_params_rule().at("query_a"),
                   .timeout = absl::Seconds(kQueryATimeoutSecs) +
                              absl::Nanoseconds(kQueryATimeoutNanos)})),
          Pair("query_b", QueryInfoEq(QuerySpec::QueryInfo{
                              .query = query_b_,
                              .rule = query_rules_.query_id_to_params_rule().at(
                                  "query_b")})),
          Pair("query_c", QueryInfoEq(QuerySpec::QueryInfo{.query = query_c_,
                                                           .rule = {}}))));

  // Only query_a and query_c are applicable for server_3
  ECCLESIA_ASSIGN_OR_FAIL(
      query_spec,
      GetQuerySpec(router_spec, "server_3",
                   SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB));

  EXPECT_THAT(
      query_spec.query_id_to_info,
      UnorderedElementsAre(
          Pair("query_a", QueryInfoEq(QuerySpec::QueryInfo{
                              .query = query_a_,
                              .rule = query_rules_.query_id_to_params_rule().at(
                                  "query_a")})),
          Pair("query_c", QueryInfoEq(QuerySpec::QueryInfo{.query = query_c_,
                                                           .rule = {}}))));

  // Only query_c is applicable for server_4
  ECCLESIA_ASSIGN_OR_FAIL(
      query_spec,
      GetQuerySpec(router_spec, "server_4",
                   SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB));

  EXPECT_THAT(
      query_spec.query_id_to_info,
      ElementsAre(Pair("query_c", QueryInfoEq(QuerySpec::QueryInfo{
                                      .query = query_c_, .rule = {}}))));
}

TEST_F(GetQuerySpecTest, RouterSpecWithServerTagsOnly) {
  QueryRouterSpec router_spec = ParseTextProtoOrDie(absl::Substitute(
      R"pb(
        selection_specs {
          key: "query_a"
          value {
            query_selection_specs {
              select { server_tag: "server_1" server_tag: "server_2" }
              query_and_rule_path {
                query_path: "$0/query_a.textproto"
                rule_path: "$0/query_rules.textproto"
              }
            }
          }
        }
        selection_specs {
          key: "query_b"
          value {
            query_selection_specs {
              select { server_tag: "server_1" server_tag: "server_3" }
              query_and_rule_path {
                query_path: "$0/query_b.textproto"
                rule_path: "$0/query_rules.textproto"
              }
            }
          }
        }
      )pb",
      apifs_.GetPath()));

  // Both query_a and query_b are applicable for server_1
  ECCLESIA_ASSIGN_OR_FAIL(
      QuerySpec query_spec,
      GetQuerySpec(router_spec, "server_1",
                   SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB));

  EXPECT_THAT(
      query_spec.query_id_to_info,
      UnorderedElementsAre(
          Pair("query_a", QueryInfoEq(QuerySpec::QueryInfo{
                              .query = query_a_,
                              .rule = query_rules_.query_id_to_params_rule().at(
                                  "query_a")})),
          Pair("query_b", QueryInfoEq(QuerySpec::QueryInfo{
                              .query = query_b_,
                              .rule = query_rules_.query_id_to_params_rule().at(
                                  "query_b")}))));

  // Only query_a is applicable for server_2
  ECCLESIA_ASSIGN_OR_FAIL(
      query_spec,
      GetQuerySpec(router_spec, "server_2",
                   SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB));

  EXPECT_THAT(
      query_spec.query_id_to_info,
      UnorderedElementsAre(Pair(
          "query_a",
          QueryInfoEq(QuerySpec::QueryInfo{
              .query = query_a_,
              .rule = query_rules_.query_id_to_params_rule().at("query_a")}))));

  // Only query_b is applicable for server_3
  ECCLESIA_ASSIGN_OR_FAIL(
      query_spec,
      GetQuerySpec(router_spec, "server_3",
                   SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB));

  EXPECT_THAT(
      query_spec.query_id_to_info,
      UnorderedElementsAre(Pair(
          "query_b",
          QueryInfoEq(QuerySpec::QueryInfo{
              .query = query_b_,
              .rule = query_rules_.query_id_to_params_rule().at("query_b")}))));
}

TEST_F(GetQuerySpecTest, RouterSpecWithServerClassOnly) {
  QueryRouterSpec router_spec = ParseTextProtoOrDie(absl::Substitute(
      R"pb(
        selection_specs {
          key: "query_a"
          value {
            query_selection_specs {
              select { server_class: [ SERVER_CLASS_COMPUTE ] }
              query_and_rule_path {
                query_path: "$0/query_a.textproto"
                rule_path: "$0/query_rules.textproto"
              }
            }
          }
        }
        selection_specs {
          key: "query_b"
          value {
            query_selection_specs {
              select { server_class: [ SERVER_CLASS_COMPUTE ] }
              query_and_rule_path {
                query_path: "$0/query_b.textproto"
                rule_path: "$0/query_rules.textproto"
              }
            }
          }
        }
      )pb",
      apifs_.GetPath()));

  // Both query_a and query_b are applicable for server_1
  ECCLESIA_ASSIGN_OR_FAIL(
      QuerySpec query_spec,
      GetQuerySpec(router_spec, "server_1",
                   SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB,
                   SelectionSpec::SelectionClass::SERVER_CLASS_COMPUTE));

  EXPECT_THAT(
      query_spec.query_id_to_info,
      UnorderedElementsAre(
          Pair("query_a", QueryInfoEq(QuerySpec::QueryInfo{
                              .query = query_a_,
                              .rule = query_rules_.query_id_to_params_rule().at(
                                  "query_a")})),
          Pair("query_b", QueryInfoEq(QuerySpec::QueryInfo{
                              .query = query_b_,
                              .rule = query_rules_.query_id_to_params_rule().at(
                                  "query_b")}))));

  // Both query_a and query_b are applicable for server_2
  ECCLESIA_ASSIGN_OR_FAIL(
      query_spec,
      GetQuerySpec(router_spec, "server_2",
                   SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB,
                   SelectionSpec::SelectionClass::SERVER_CLASS_COMPUTE));

  EXPECT_THAT(
      query_spec.query_id_to_info,
      UnorderedElementsAre(
          Pair("query_a", QueryInfoEq(QuerySpec::QueryInfo{
                              .query = query_a_,
                              .rule = query_rules_.query_id_to_params_rule().at(
                                  "query_a")})),
          Pair("query_b", QueryInfoEq(QuerySpec::QueryInfo{
                              .query = query_b_,
                              .rule = query_rules_.query_id_to_params_rule().at(
                                  "query_b")}))));

  // Both query_a and query_b are applicable for server_3
  ECCLESIA_ASSIGN_OR_FAIL(
      query_spec,
      GetQuerySpec(router_spec, "server_3",
                   SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB,
                   SelectionSpec::SelectionClass::SERVER_CLASS_COMPUTE));

  EXPECT_THAT(
      query_spec.query_id_to_info,
      UnorderedElementsAre(
          Pair("query_a", QueryInfoEq(QuerySpec::QueryInfo{
                              .query = query_a_,
                              .rule = query_rules_.query_id_to_params_rule().at(
                                  "query_a")})),
          Pair("query_b", QueryInfoEq(QuerySpec::QueryInfo{
                              .query = query_b_,
                              .rule = query_rules_.query_id_to_params_rule().at(
                                  "query_b")}))));
}

TEST_F(GetQuerySpecTest, RouterSpecWithServerSpecificQueries) {
  QueryRouterSpec router_spec = ParseTextProtoOrDie(absl::Substitute(
      R"pb(
        selection_specs {
          key: "query_a"
          value {
            query_selection_specs {
              select { server_type: SERVER_TYPE_BMCWEB server_tag: "server_1" }
              query_and_rule_path {
                query_path: "$0/query_a.textproto"
                rule_path: "$0/query_rules.textproto"
              }
            }
            query_selection_specs {
              select { server_type: SERVER_TYPE_BMCWEB server_tag: "server_2" }
              query_and_rule_path {
                query_path: "$0/query_a_other.textproto"
                rule_path: "$0/query_rules_other.textproto"
              }
            }
          }
        }
      )pb",
      apifs_.GetPath()));

  // For server_1, query_a should point to query_a.textproto and
  // query_rules.textproto
  ECCLESIA_ASSIGN_OR_FAIL(
      QuerySpec query_spec,
      GetQuerySpec(router_spec, "server_1",
                   SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB));

  EXPECT_THAT(
      query_spec.query_id_to_info,
      UnorderedElementsAre(Pair(
          "query_a",
          QueryInfoEq(QuerySpec::QueryInfo{
              .query = query_a_,
              .rule = query_rules_.query_id_to_params_rule().at("query_a")}))));

  // For server_2, query_a should point to query_a_other.textproto and
  // query_rules_other.textproto
  ECCLESIA_ASSIGN_OR_FAIL(
      query_spec,
      GetQuerySpec(router_spec, "server_2",
                   SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB));

  EXPECT_THAT(query_spec.query_id_to_info,
              UnorderedElementsAre(Pair(
                  "query_a",
                  QueryInfoEq(QuerySpec::QueryInfo{
                      .query = query_a_other_,
                      .rule = query_rules_other_.query_id_to_params_rule().at(
                          "query_a")}))));
}

TEST_F(GetQuerySpecTest, RouterSpecWithEmbeddedQueries) {
  QueryRouterSpec router_spec = ParseTextProtoOrDie(absl::Substitute(
      R"pb(
        selection_specs {
          key: "query_a"
          value {
            query_selection_specs {
              select { server_type: SERVER_TYPE_BMCWEB server_tag: "server_1" }
              query_and_rule {
                query {
                  query_id: "query_a"
                  property_sets {
                    properties { property: "property_a" type: STRING }
                  }
                }
                rule {
                  redpath_prefix_with_params {
                    expand_configuration { level: 1 type: ONLY_LINKS }
                  }
                }
              }
              timeout { seconds: $0 nanos: $1 }
            }
            query_selection_specs {
              select { server_type: SERVER_TYPE_BMCWEB server_tag: "server_2" }
              query_and_rule {
                query {
                  query_id: "query_a"
                  property_sets {
                    properties { property: "property_a_other" type: BOOLEAN }
                  }
                }
                rule {
                  redpath_prefix_with_params {
                    expand_configuration { level: 5 type: BOTH }
                  }
                }
              }
            }
          }
        }
      )pb",
      kQueryATimeoutSecs, kQueryATimeoutNanos));

  // For server_1, query_a should point to query_a and query_a rule
  ECCLESIA_ASSIGN_OR_FAIL(
      QuerySpec query_spec,
      GetQuerySpec(router_spec, "server_1",
                   SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB));

  EXPECT_THAT(
      query_spec.query_id_to_info,
      UnorderedElementsAre(
          Pair("query_a",
               QueryInfoEq(QuerySpec::QueryInfo{
                   .query = query_a_,
                   .rule = query_rules_.query_id_to_params_rule().at("query_a"),
                   .timeout = absl::Seconds(kQueryATimeoutSecs) +
                              absl::Nanoseconds(kQueryATimeoutNanos)}))));

  // For server_2, query_a should point to query_a_other and query_a_other rule
  ECCLESIA_ASSIGN_OR_FAIL(
      query_spec,
      GetQuerySpec(router_spec, "server_2",
                   SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB));

  EXPECT_THAT(query_spec.query_id_to_info,
              UnorderedElementsAre(Pair(
                  "query_a",
                  QueryInfoEq(QuerySpec::QueryInfo{
                      .query = query_a_other_,
                      .rule = query_rules_other_.query_id_to_params_rule().at(
                          "query_a")}))));
}

TEST_F(GetQuerySpecTest, RouterSpecWithNoSelectionSpec) {
  QueryRouterSpec router_spec = ParseTextProtoOrDie(absl::Substitute(
      R"pb(
        selection_specs {
          key: "query_a"
          value {
            query_selection_specs {
              query_and_rule_path {
                query_path: "$0/query_a.textproto"
                rule_path: "$0/query_rules.textproto"
              }
            }
          }
        }
      )pb",
      apifs_.GetPath()));

  EXPECT_THAT(GetQuerySpec(router_spec, "server_1",
                           SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB),
              IsStatusFailedPrecondition());
}

TEST_F(GetQuerySpecTest, RouterSpecWithEmptySelectionSpec) {
  QueryRouterSpec router_spec = ParseTextProtoOrDie(absl::Substitute(
      R"pb(
        selection_specs {
          key: "query_a"
          value {
            query_selection_specs {
              select {}
              query_and_rule_path {
                query_path: "$0/query_a.textproto"
                rule_path: "$0/query_rules.textproto"
              }
            }
          }
        }
      )pb",
      apifs_.GetPath()));

  EXPECT_THAT(GetQuerySpec(router_spec, "server_1",
                           SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB,
                           SelectionSpec::SelectionClass::SERVER_CLASS_COMPUTE),
              IsStatusFailedPrecondition());
}

TEST_F(GetQuerySpecTest, RouterSpecWithIncorrectQueryId) {
  // query_b textproto is set for query_a spec.
  QueryRouterSpec router_spec = ParseTextProtoOrDie(absl::Substitute(
      R"pb(
        selection_specs {
          key: "query_a"
          value {
            query_selection_specs {
              select { server_type: SERVER_TYPE_BMCWEB server_tag: "server_1" }
              query_and_rule_path { query_path: "$0/query_b.textproto" }
            }
          }
        }
      )pb",
      apifs_.GetPath()));

  EXPECT_THAT(GetQuerySpec(router_spec, "server_1",
                           SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB),
              IsStatusFailedPrecondition());
}

TEST_F(GetQuerySpecTest, RouterSpecWithUnspecifiedServerType) {
  QueryRouterSpec router_spec = ParseTextProtoOrDie(absl::Substitute(
      R"pb(
        selection_specs {
          key: "query_a"
          value {
            query_selection_specs {
              select {
                server_type: SERVER_TYPE_UNSPECIFIED
                server_tag: "server_1"
              }
              query_and_rule_path {
                query_path: "$0/query_a.textproto"
                rule_path: "$0/query_rules.textproto"
              }
            }
          }
        }
      )pb",
      apifs_.GetPath()));

  EXPECT_THAT(GetQuerySpec(router_spec, "server_1",
                           SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB),
              IsStatusFailedPrecondition());
}

TEST_F(GetQuerySpecTest, RouterSpecWithUnspecifiedServerClass) {
  QueryRouterSpec router_spec = ParseTextProtoOrDie(absl::Substitute(
      R"pb(
        selection_specs {
          key: "query_a"
          value {
            query_selection_specs {
              select {
                server_type: SERVER_TYPE_BMCWEB
                server_tag: "server_1"
                server_class: [ SERVER_CLASS_UNSPECIFIED ]
              }
              query_and_rule_path {
                query_path: "$0/query_a.textproto"
                rule_path: "$0/query_rules.textproto"
              }
            }
          }
        }
      )pb",
      apifs_.GetPath()));

  EXPECT_THAT(GetQuerySpec(router_spec, "server_1",
                           SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB,
                           SelectionSpec::SelectionClass::SERVER_CLASS_COMPUTE),
              IsStatusFailedPrecondition());
}

TEST_F(GetQuerySpecTest, QueryFilesDoesNotExist) {
  QueryRouterSpec router_spec = ParseTextProtoOrDie(absl::Substitute(
      R"pb(
        selection_specs {
          key: "query_a"
          value {
            query_selection_specs {
              select { server_type: SERVER_TYPE_BMCWEB server_tag: "server_1" }
              query_and_rule_path { query_path: "$0/does_not_exist.textproto" }
            }
          }
        }
        selection_specs {
          key: "query_b"
          value {
            query_selection_specs {
              select { server_tag: "server_1" }
              query_and_rule_path {
                query_path: "$0/query_b.textproto"
                rule_path: "$0/does_not_exist.textproto"
              }
            }
          }
        }
      )pb",
      apifs_.GetPath()));

  EXPECT_THAT(GetQuerySpec(router_spec, "server_1",
                           SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB),
              IsStatusNotFound());
}

TEST_F(GetQuerySpecTest, RouterSpecWithInvalidQueryFile) {
  fs_.WriteFile(absl::StrCat(kRootDir, "invalid_query.textproto"), "deadbeef");

  QueryRouterSpec router_spec = ParseTextProtoOrDie(absl::Substitute(
      R"pb(
        selection_specs {
          key: "query_a"
          value {
            query_selection_specs {
              select { server_type: SERVER_TYPE_BMCWEB server_tag: "server_1" }
              query_and_rule_path { query_path: "$0/invalid_query.textproto" }
            }
          }
        }
      )pb",
      apifs_.GetPath()));

  EXPECT_THAT(GetQuerySpec(router_spec, "server_1",
                           SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB),
              IsStatusFailedPrecondition());
}

TEST_F(GetQuerySpecTest, RouterSpecWithoutQueryFile) {
  QueryRouterSpec router_spec = ParseTextProtoOrDie(absl::Substitute(
      R"pb(
        selection_specs {
          key: "query_a"
          value {
            query_selection_specs {
              select { server_tag: "server_1" }
              query_and_rule_path { rule_path: "$0/query_rules.textproto" }
            }
          }
        }
      )pb",
      apifs_.GetPath()));

  EXPECT_THAT(GetQuerySpec(router_spec, "server_1",
                           SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB),
              IsStatusFailedPrecondition());
}

TEST_F(GetQuerySpecTest, EmptyRouterSpecTest) {
  ECCLESIA_ASSIGN_OR_FAIL(
      QuerySpec query_spec,
      GetQuerySpec({}, "server_1",
                   SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB));

  EXPECT_TRUE(query_spec.query_id_to_info.empty());
}

TEST(GetStableIdTypeTest, GetStableIdTypeCorrectly) {
  QueryRouterSpec router_spec = ParseTextProtoOrDie(
      R"pb(
        selection_specs {
          key: "query_a"
          value {
            query_selection_specs {
              select {
                server_type: SERVER_TYPE_BMCWEB
                server_tag: "server_1"
                server_tag: "server_2"
              }
              query_and_rule_path { query_path: "query_a.textproto" }
            }
          }
        }
        stable_id_config: {
          policies: {
            select { server_type: SERVER_TYPE_BMCWEB server_tag: "server_1" }
            stable_id_type: STABLE_ID_REDFISH_LOCATION
          }
          policies: {
            select { server_type: SERVER_TYPE_BMCWEB server_tag: "server_2" }
            stable_id_type: STABLE_ID_TOPOLOGY_DERIVED
          }
          policies: {
            select {
              server_type: SERVER_TYPE_BMCWEB
              server_class: SERVER_CLASS_COMPUTE
            }
            stable_id_type: STABLE_ID_REDFISH_LOCATION
          }
        }
        default_stable_id_type: STABLE_ID_TOPOLOGY_DERIVED
      )pb");

  EXPECT_THAT(
      GetStableIdTypeFromRouterSpec(
          router_spec, "server_1",
          SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB,
          SelectionSpec::SelectionClass::SERVER_CLASS_COMPUTE),
      ecclesia::QueryRouterSpec::StableIdConfig::STABLE_ID_REDFISH_LOCATION);
  EXPECT_THAT(
      GetStableIdTypeFromRouterSpec(
          router_spec, "server_2",
          SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB,
          SelectionSpec::SelectionClass::SERVER_CLASS_UNSPECIFIED),
      ecclesia::QueryRouterSpec::StableIdConfig::STABLE_ID_TOPOLOGY_DERIVED);
  EXPECT_THAT(
      GetStableIdTypeFromRouterSpec(
          router_spec, "", SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB,
          SelectionSpec::SelectionClass::SERVER_CLASS_COMPUTE),
      ecclesia::QueryRouterSpec::StableIdConfig::STABLE_ID_REDFISH_LOCATION);
  // When no config matches, the type should be based on the default.
  EXPECT_THAT(
      GetStableIdTypeFromRouterSpec(
          router_spec, "",
          SelectionSpec::SelectionClass::SERVER_TYPE_UNSPECIFIED,
          SelectionSpec::SelectionClass::SERVER_CLASS_UNSPECIFIED),
      ecclesia::QueryRouterSpec::StableIdConfig::STABLE_ID_TOPOLOGY_DERIVED);
}

TEST(GetStableIdTypeTest, GetTopologyConfigNameCorrectly) {
  QueryRouterSpec router_spec = ParseTextProtoOrDie(
      R"pb(
        selection_specs {
          key: "query_a"
          value {
            query_selection_specs {
              select {
                server_type: SERVER_TYPE_BMCWEB
                server_tag: "server_1"
                server_tag: "server_2"
              }
              query_and_rule_path { query_path: "query_a.textproto" }
            }
          }
        }
        stable_id_config: {
          policies: {
            select { server_type: SERVER_TYPE_BMCWEB server_tag: "server_1" }
            topology_config_path: "topology_config_1"
          }
          policies: {
            select { server_type: SERVER_TYPE_BMCWEB server_tag: "server_2" }
            topology_config_path: "topology_config_2"
          }
          policies: {
            select {
              server_type: SERVER_TYPE_BMCWEB
              server_class: SERVER_CLASS_COMPUTE
            }
            topology_config_path: "topology_config_3"
          }
        }
        default_stable_id_type: STABLE_ID_TOPOLOGY_DERIVED
      )pb");

  ASSERT_EQ(GetTopologyConfigNameFromRouterSpec(
                router_spec, "server_1",
                SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB,
                SelectionSpec::SelectionClass::SERVER_CLASS_COMPUTE),
            "topology_config_1");
  ASSERT_EQ(GetTopologyConfigNameFromRouterSpec(
                router_spec, "server_2",
                SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB,
                SelectionSpec::SelectionClass::SERVER_CLASS_UNSPECIFIED),
            "topology_config_2");
  ASSERT_EQ(
      GetTopologyConfigNameFromRouterSpec(
          router_spec, "", SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB,
          SelectionSpec::SelectionClass::SERVER_CLASS_COMPUTE),
      "topology_config_3");
  // When no config matches, the type should be based on the default.
  ASSERT_EQ(GetTopologyConfigNameFromRouterSpec(
                router_spec, "",
                SelectionSpec::SelectionClass::SERVER_TYPE_UNSPECIFIED,
                SelectionSpec::SelectionClass::SERVER_CLASS_UNSPECIFIED),
            std::nullopt);
}

}  // namespace
}  // namespace ecclesia
