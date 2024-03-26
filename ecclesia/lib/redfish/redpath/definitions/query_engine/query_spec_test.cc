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

#include "ecclesia/lib/redfish/redpath/definitions/query_engine/query_spec.h"

#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "ecclesia/lib/apifs/apifs.h"
#include "ecclesia/lib/file/cc_embed_interface.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/status/test_macros.h"
#include "ecclesia/lib/testing/proto.h"
#include "ecclesia/lib/testing/status.h"
#include "ecclesia/lib/time/clock_fake.h"
#include "google/protobuf/text_format.h"

namespace ecclesia {
namespace {

using ::testing::Pair;
using ::testing::UnorderedElementsAre;

// Custom matcher to match QueryInfo struct.
MATCHER_P(QueryInfoEq, query_info, "") {
  return ExplainMatchResult(EqualsProto(query_info.query), arg.query,
                            result_listener) &&
         ExplainMatchResult(EqualsProto(query_info.rule), arg.rule,
                            result_listener);
}

TEST(QuerySpecTest, ConvertFromQueryContextPass) {
  DelliciusQuery query_a = ParseTextProtoOrDie(
      R"pb(query_id: "query_a"
           property_sets { properties { property: "property_1" type: STRING } }
      )pb");

  DelliciusQuery query_b = ParseTextProtoOrDie(
      R"pb(query_id: "query_b"
           property_sets { properties { property: "property_1" type: STRING } }
      )pb");

  QueryRules query_rules = ParseTextProtoOrDie(
      R"pb(query_id_to_params_rule {
             key: "query_a"
             value {
               redpath_prefix_with_params {
                 expand_configuration { level: 1 type: BOTH }
               }
             }
           }
      )pb");

  std::string query_a_contents;
  std::string query_b_contents;
  std::string rule_contents;
  google::protobuf::TextFormat::PrintToString(query_a, &query_a_contents);
  google::protobuf::TextFormat::PrintToString(query_b, &query_b_contents);
  google::protobuf::TextFormat::PrintToString(query_rules, &rule_contents);

  std::vector<EmbeddedFile> queries = {
      EmbeddedFile{.name = "query_a.textproto", .data = query_a_contents},
      EmbeddedFile{.name = "query_b.textproto", .data = query_b_contents},
  };

  std::vector<EmbeddedFile> rules = {
      EmbeddedFile{.name = "query_rules.textproto", .data = rule_contents},
  };

  FakeClock clock;

  ECCLESIA_ASSIGN_OR_FAIL(
      QuerySpec spec,
      QuerySpec::FromQueryContext(
          {.query_files = queries, .query_rules = rules, .clock = &clock}));
  EXPECT_THAT(
      spec.query_id_to_info,
      UnorderedElementsAre(
          Pair("query_a",
               QueryInfoEq(QuerySpec::QueryInfo{
                   .query = std::move(query_a),
                   .rule = std::move(
                       query_rules.mutable_query_id_to_params_rule()->at(
                           "query_a"))})),
          Pair("query_b", QueryInfoEq(QuerySpec::QueryInfo{
                              .query = std::move(query_b)}))));
  EXPECT_EQ(spec.clock, &clock);
}

TEST(QuerySpecTest, ConvertFromQueryContextDuplicateQueries) {
  DelliciusQuery query_a = ParseTextProtoOrDie(
      R"pb(query_id: "query_a"
           property_sets { properties { property: "property_1" type: STRING } }
      )pb");

  std::string query_a_contents;
  google::protobuf::TextFormat::PrintToString(query_a, &query_a_contents);

  std::vector<EmbeddedFile> queries = {
      EmbeddedFile{.name = "query_a.textproto", .data = query_a_contents},
      EmbeddedFile{.name = "query_b.textproto", .data = query_a_contents},
  };

  EXPECT_THAT(QuerySpec::FromQueryContext({.query_files = queries}),
              IsStatusInternal());
}

TEST(QuerySpecTest, ConvertFromQueryContextUnableToReadQuery) {
  std::vector<EmbeddedFile> queries = {
      EmbeddedFile{.name = "query_a.textproto", .data = "UNKNOWN"}};

  EXPECT_THAT(QuerySpec::FromQueryContext({.query_files = queries}),
              IsStatusInternal());
}

TEST(QuerySpecTest, ConvertFromQueryContextUnableToReadQueryRule) {
  DelliciusQuery query_a = ParseTextProtoOrDie(
      R"pb(query_id: "query_a"
           property_sets { properties { property: "property_1" type: STRING } }
      )pb");

  std::string query_a_contents;
  google::protobuf::TextFormat::PrintToString(query_a, &query_a_contents);

  std::vector<EmbeddedFile> queries = {
      EmbeddedFile{.name = "query_a.textproto", .data = query_a_contents},
  };

  std::vector<EmbeddedFile> rules = {
      EmbeddedFile{.name = "query_rules.textproto", .data = "UNKNOWN"},
  };

  EXPECT_THAT(QuerySpec::FromQueryContext(
                  {.query_files = queries, .query_rules = rules}),
              IsStatusInternal());
}

class QuerySpecConvertTest : public testing::Test {
 protected:
  QuerySpecConvertTest()
      : fs_(GetTestTempdirPath()), apifs_(GetTestTempdirPath("test")) {
    fs_.CreateDir("/tmp/test");
  }

  TestFilesystem fs_;
  ApifsDirectory apifs_;
};

TEST_F(QuerySpecConvertTest, ConvertFromQueryFilesPass) {
  DelliciusQuery query_a = ParseTextProtoOrDie(
      R"pb(query_id: "query_a"
           property_sets { properties { property: "property_1" type: STRING } }
      )pb");

  DelliciusQuery query_b = ParseTextProtoOrDie(
      R"pb(query_id: "query_b"
           property_sets { properties { property: "property_1" type: STRING } }
      )pb");

  QueryRules query_rules = ParseTextProtoOrDie(
      R"pb(query_id_to_params_rule {
             key: "query_a"
             value {
               redpath_prefix_with_params {
                 expand_configuration { level: 1 type: BOTH }
               }
             }
           }
      )pb");

  std::string query_a_contents;
  std::string query_b_contents;
  std::string rule_contents;
  google::protobuf::TextFormat::PrintToString(query_a, &query_a_contents);
  google::protobuf::TextFormat::PrintToString(query_b, &query_b_contents);
  google::protobuf::TextFormat::PrintToString(query_rules, &rule_contents);

  fs_.CreateFile("/tmp/test/query_a.textproto", query_a_contents);
  fs_.CreateFile("/tmp/test/query_b.textproto", query_b_contents);
  fs_.CreateFile("/tmp/test/query_rule.textproto", rule_contents);

  ECCLESIA_ASSIGN_OR_FAIL(
      QuerySpec spec, QuerySpec::FromQueryFiles(
                          {fs_.GetTruePath("/tmp/test/query_a.textproto"),
                           fs_.GetTruePath("/tmp/test/query_b.textproto")},
                          {fs_.GetTruePath("/tmp/test/query_rule.textproto")}));

  EXPECT_THAT(
      spec.query_id_to_info,
      UnorderedElementsAre(
          Pair("query_a",
               QueryInfoEq(QuerySpec::QueryInfo{
                   .query = std::move(query_a),
                   .rule = std::move(
                       query_rules.mutable_query_id_to_params_rule()->at(
                           "query_a"))})),
          Pair("query_b", QueryInfoEq(QuerySpec::QueryInfo{
                              .query = std::move(query_b)}))));
  EXPECT_NE(spec.clock, nullptr);
}

TEST_F(QuerySpecConvertTest, ConvertFromQueryFilesDuplicateQueries) {
  DelliciusQuery query_a = ParseTextProtoOrDie(
      R"pb(query_id: "query_a"
           property_sets { properties { property: "property_1" type: STRING } }
      )pb");

  QueryRules query_rules = ParseTextProtoOrDie(
      R"pb(query_id_to_params_rule {
             key: "query_a"
             value {
               redpath_prefix_with_params {
                 expand_configuration { level: 1 type: BOTH }
               }
             }
           }
      )pb");

  std::string query_a_contents;
  std::string rule_contents;
  google::protobuf::TextFormat::PrintToString(query_a, &query_a_contents);
  google::protobuf::TextFormat::PrintToString(query_rules, &rule_contents);

  fs_.CreateFile("/tmp/test/query_a.textproto", query_a_contents);
  fs_.CreateFile("/tmp/test/query_b.textproto", query_a_contents);
  fs_.CreateFile("/tmp/test/query_rule.textproto", rule_contents);

  EXPECT_THAT(QuerySpec::FromQueryFiles(
                  {fs_.GetTruePath("/tmp/test/query_a.textproto"),
                   fs_.GetTruePath("/tmp/test/query_b.textproto")},
                  {fs_.GetTruePath("/tmp/test/query_rule.textproto")}),
              IsStatusInternal());
}

TEST_F(QuerySpecConvertTest, ConvertFromQueryFilesUnableToReadQuery) {
  fs_.CreateFile("/tmp/test/query_a.textproto", "UNKNOWN");

  EXPECT_THAT(QuerySpec::FromQueryFiles(
                  {fs_.GetTruePath("/tmp/test/query_a.textproto")}, {}),
              IsStatusInternal());
}

TEST_F(QuerySpecConvertTest, ConvertFromQueryFilesUnableToReadQueryRule) {
  DelliciusQuery query_a = ParseTextProtoOrDie(
      R"pb(query_id: "query_a"
           property_sets { properties { property: "property_1" type: STRING } }
      )pb");

  std::string query_a_contents;
  google::protobuf::TextFormat::PrintToString(query_a, &query_a_contents);

  fs_.CreateFile("/tmp/test/query_a.textproto", query_a_contents);
  fs_.CreateFile("/tmp/test/query_rule.textproto", "UNKNOWN");

  EXPECT_THAT(QuerySpec::FromQueryFiles(
                  {fs_.GetTruePath("/tmp/test/query_a.textproto")},
                  {fs_.GetTruePath("/tmp/test/query_rule.textproto")}),
              IsStatusInternal());
}

}  // namespace
}  // namespace ecclesia
