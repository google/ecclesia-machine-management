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

#include "ecclesia/lib/redfish/redpath/definitions/query_router/query_router.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "ecclesia/lib/apifs/apifs.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/redfish/dellicius/engine/file_backed_query_engine.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/passkey.h"
#include "ecclesia/lib/redfish/dellicius/engine/mock_query_engine.h"
#include "ecclesia/lib/redfish/dellicius/engine/query_engine.h"
#include "ecclesia/lib/redfish/dellicius/query/query_variables.pb.h"
#include "ecclesia/lib/redfish/dellicius/utils/id_assigner.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_engine/query_spec.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_router/default_template_variable_names.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_router/query_router_spec.pb.h"
#include "ecclesia/lib/redfish/redpath/engine/id_assigner.h"
#include "ecclesia/lib/status/test_macros.h"
#include "ecclesia/lib/testing/status.h"
#include "google/protobuf/text_format.h"

namespace ecclesia {
namespace {

using ::testing::_;
using ::testing::IsEmpty;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::Values;

constexpr absl::string_view kRootDir = "/test/";
constexpr absl::string_view kQueryResultDir = "/test/query_result/";

MATCHER_P(ContainsSystemIdAsQueryVariable, expected_id, "") {
  for (const auto &[query_id, query_vars] : arg) {
    for (const auto &var_value : query_vars.variable_values()) {
      if (var_value.name() == kNodeLocalSystemIdVariableName) {
        const auto &values = var_value.values();
        return std::find(values.begin(), values.end(), expected_id) !=
               values.end();
      }
    }
  }
  return false;
}

// Struct to hold the callback parameters to set expectations.
struct QueryRouterCallbacks {
  std::string query_id;
  QueryRouterIntf::ServerInfo server_info;

  template <typename H>
  friend H AbslHashValue(H h, const QueryRouterCallbacks &e) {
    return H::combine(std::move(h), e.query_id, e.server_info);
  }

  bool operator==(const QueryRouterCallbacks &other) const {
    return std::tie(query_id, server_info) ==
           std::tie(other.query_id, other.server_info);
  }
};

class QueryRouterTest : public testing::Test {
 protected:
  QueryRouterTest()
      : fs_(GetTestTempdirPath()), apifs_(GetTestTempdirPath("test")) {}

  void SetUp() override {
    fs_.CreateDir(kRootDir);
    fs_.CreateDir(kQueryResultDir);

    // Create Sample queries and query results
    DelliciusQuery query_a = ParseTextProtoOrDie(
        R"pb(query_id: "query_a"
             property_sets {
               properties { property: "property_a" type: STRING }
             }
        )pb");
    CreateFile(kRootDir, "query_a.textproto", query_a);

    QueryResult result_a = ParseTextProtoOrDie(R"pb(
      query_id: "query_a"
      data {
        fields {
          key: "property_a"
          value { string_value: "value_a" }
        }
      }
    )pb");
    CreateFile(kQueryResultDir, "/query_result_a.textproto", result_a);

    DelliciusQuery query_b = ParseTextProtoOrDie(
        R"pb(query_id: "query_b"
             property_sets {
               properties { property: "property_b" type: STRING }
             }
        )pb");
    CreateFile(kRootDir, "query_b.textproto", query_b);

    QueryResult result_b = ParseTextProtoOrDie(R"pb(
      query_id: "query_b"
      data {
        fields {
          key: "property_b"
          value { string_value: "value_b" }
        }
      }
    )pb");
    CreateFile(kQueryResultDir, "/query_result_b.textproto", result_b);

    DelliciusQuery query_c = ParseTextProtoOrDie(
        R"pb(query_id: "query_c"
             property_sets {
               properties { property: "property_c" type: STRING }
             }
        )pb");
    CreateFile(kRootDir, "query_c.textproto", query_c);

    QueryResult result_c = ParseTextProtoOrDie(R"pb(
      query_id: "query_c"
      data {
        fields {
          key: "property_c"
          value { string_value: "value_c" }
        }
      }
    )pb");
    CreateFile(kQueryResultDir, "/query_result_c.textproto", result_c);

    // Create Sample rule
    QueryRules query_rules = ParseTextProtoOrDie(R"pb(
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
    CreateFile(kRootDir, "query_rules.textproto", query_rules);
  }

  template <typename T>
  void CreateFile(absl::string_view dir, absl::string_view filename,
                  const T &item) {
    std::string contents;
    google::protobuf::TextFormat::PrintToString(item, &contents);
    fs_.WriteFile(absl::StrCat(dir, filename), contents);
  }

  static QueryRouter::ServerSpec GetServerSpec(
      absl::string_view server_tag,
      ecclesia::QueryEngineParams::RedfishStableIdType stable_id =
          ecclesia::QueryEngineParams::RedfishStableIdType::kRedfishLocation) {
    QueryRouter::ServerSpec server_spec;
    server_spec.server_info.server_tag = server_tag;
    server_spec.server_info.server_type =
        SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB;
    server_spec.server_info.server_class =
        SelectionSpec::SelectionClass::SERVER_CLASS_COMPUTE;
    server_spec.stable_id_type = stable_id;
    return server_spec;
  };

  TestFilesystem fs_;
  ApifsDirectory apifs_;
};

class QueryRouterSuccessTest
    : public QueryRouterTest,
      public testing::WithParamInterface<absl::string_view> {};

TEST_P(QueryRouterSuccessTest, CreateSuccess) {
  QueryRouterSpec router_spec = ParseTextProtoOrDie(absl::Substitute(
      R"pb(
        query_pattern: $1
        max_concurrent_threads: 3
        selection_specs {
          key: "query_a"
          value {
            query_selection_specs {
              select {
                server_type: SERVER_TYPE_BMCWEB
                server_tag: "server_1"
                server_tag: "server_2"
              }
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
              select { server_type: SERVER_TYPE_BMCWEB server_tag: "server_1" }
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
      apifs_.GetPath(), GetParam()));

  std::vector<QueryRouter::ServerSpec> server_specs;
  server_specs.push_back(GetServerSpec("server_1"));
  server_specs.push_back(GetServerSpec("server_2"));
  server_specs.push_back(GetServerSpec("server_3"));

  ECCLESIA_ASSIGN_OR_FAIL(
      auto query_router,
      QueryRouter::Create(
          router_spec, std::move(server_specs),
          [&](const QuerySpec &, const QueryEngineParams &params,
              std::unique_ptr<IdAssigner>)
              -> absl::StatusOr<std::unique_ptr<QueryEngineIntf>> {
            EXPECT_FALSE(params.features.enable_redfish_metrics());
            EXPECT_TRUE(params.features.fail_on_first_error());
            EXPECT_FALSE(params.features.log_redfish_traces());
            return FileBackedQueryEngine::Create(
                fs_.GetTruePath(kQueryResultDir));
          }));
  {
    absl::flat_hash_set<QueryRouterCallbacks> expected_callbacks = {
        {"query_a",
         {"server_1", SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB,
          SelectionSpec::SelectionClass::SERVER_CLASS_COMPUTE}},
        {"query_a",
         {"server_2", SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB,
          SelectionSpec::SelectionClass::SERVER_CLASS_COMPUTE}}};

    query_router->ExecuteQuery(
        {"query_a"},
        [&expected_callbacks](const QueryRouter::ServerInfo &server_info,
                              const QueryResult &result) {
          auto it = expected_callbacks.find(
              QueryRouterCallbacks{result.query_id(), server_info});
          ASSERT_NE(it, expected_callbacks.end());
          expected_callbacks.erase(it);
        });

    EXPECT_THAT(expected_callbacks, IsEmpty());
  }

  {
    absl::flat_hash_set<QueryRouterCallbacks> expected_callbacks = {
        {"query_b",
         {"server_1", SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB,
          SelectionSpec::SelectionClass::SERVER_CLASS_COMPUTE}}};

    query_router->ExecuteQuery(
        {"query_b"},
        [&expected_callbacks](const QueryRouter::ServerInfo &server_info,
                              const QueryResult &result) {
          auto it = expected_callbacks.find(
              QueryRouterCallbacks{result.query_id(), server_info});
          ASSERT_NE(it, expected_callbacks.end());
          expected_callbacks.erase(it);
        });

    EXPECT_THAT(expected_callbacks, IsEmpty());
  }

  {
    absl::flat_hash_set<QueryRouterCallbacks> expected_callbacks = {
        {"query_c",
         {"server_1", SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB,
          SelectionSpec::SelectionClass::SERVER_CLASS_COMPUTE}},
        {"query_c",
         {"server_2", SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB,
          SelectionSpec::SelectionClass::SERVER_CLASS_COMPUTE}},
        {"query_c",
         {"server_3", SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB,
          SelectionSpec::SelectionClass::SERVER_CLASS_COMPUTE}}};

    query_router->ExecuteQuery(
        {"query_c"},
        [&expected_callbacks](const QueryRouter::ServerInfo &server_info,
                              const QueryResult &result) {
          auto it = expected_callbacks.find(
              QueryRouterCallbacks{result.query_id(), server_info});
          ASSERT_NE(it, expected_callbacks.end());
          expected_callbacks.erase(it);
        });

    EXPECT_THAT(expected_callbacks, IsEmpty());
  }

  {
    absl::flat_hash_set<QueryRouterCallbacks> expected_callbacks = {
        {"query_a",
         {"server_1", SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB,
          SelectionSpec::SelectionClass::SERVER_CLASS_COMPUTE}},
        {"query_b",
         {"server_1", SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB,
          SelectionSpec::SelectionClass::SERVER_CLASS_COMPUTE}},
        {"query_c",
         {"server_1", SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB,
          SelectionSpec::SelectionClass::SERVER_CLASS_COMPUTE}},
        {"query_a",
         {"server_2", SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB,
          SelectionSpec::SelectionClass::SERVER_CLASS_COMPUTE}},
        {"query_c",
         {"server_2", SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB,
          SelectionSpec::SelectionClass::SERVER_CLASS_COMPUTE}},
        {"query_c",
         {"server_3", SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB,
          SelectionSpec::SelectionClass::SERVER_CLASS_COMPUTE}}};

    query_router->ExecuteQuery(
        {"query_a", "query_b", "query_c"},
        [&expected_callbacks](const QueryRouter::ServerInfo &server_info,
                              const QueryResult &result) {
          auto it = expected_callbacks.find(
              QueryRouterCallbacks{result.query_id(), server_info});
          ASSERT_NE(it, expected_callbacks.end());
          expected_callbacks.erase(it);
        });

    EXPECT_THAT(expected_callbacks, IsEmpty());
  }
}

INSTANTIATE_TEST_SUITE_P(CheckQueryRouterCreate, QueryRouterSuccessTest,
                         Values("PATTERN_SERIAL_ALL", "PATTERN_SERIAL_AGENT",
                                "PATTERN_PARALLEL_ALL"));

TEST_F(QueryRouterTest, CreateSuccessWithSystemIdQueryRouterTest) {
  QueryRouterSpec router_spec = ParseTextProtoOrDie(absl::Substitute(
      R"pb(
        query_pattern: PATTERN_SERIAL_ALL
        selection_specs {
          key: "query_a"
          value {
            query_selection_specs {
              select {
                server_type: SERVER_TYPE_BMCWEB
                server_tag: "server_1"
                server_tag: "server_2"
              }
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
              select { server_type: SERVER_TYPE_BMCWEB server_tag: "server_1" }
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
      apifs_.GetPath()));

  std::vector<QueryRouter::ServerSpec> server_specs;
  auto server_spec = GetServerSpec("server_1");
  server_spec.node_local_system_id = "system1";
  server_specs.push_back(std::move(server_spec));
  auto mock_qe = std::make_unique<MockQueryEngine>();

  EXPECT_CALL(*mock_qe, ExecuteRedpathQuery(
                            _, _, ContainsSystemIdAsQueryVariable("system1")))
      .Times(1);

  ECCLESIA_ASSIGN_OR_FAIL(
      auto query_router,
      QueryRouter::Create(
          router_spec, std::move(server_specs),
          [&](const QuerySpec &, const QueryEngineParams &params,
              std::unique_ptr<IdAssigner>)
              -> absl::StatusOr<std::unique_ptr<QueryEngineIntf>> {
            EXPECT_FALSE(params.features.enable_redfish_metrics());
            EXPECT_TRUE(params.features.fail_on_first_error());
            EXPECT_FALSE(params.features.log_redfish_traces());
            return std::move(mock_qe);
          }));

  query_router->ExecuteQuery({"query_a"}, {});
}

TEST_F(QueryRouterTest, DisjointServerAndQuerySpec) {
  QueryRouterSpec router_spec = ParseTextProtoOrDie(absl::Substitute(
      R"pb(
        query_pattern: PATTERN_SERIAL_ALL
        selection_specs {
          key: "query_a"
          value {
            query_selection_specs {
              select {
                server_type: SERVER_TYPE_BMCWEB
                server_tag: "server_1"
                server_tag: "server_2"
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

  std::vector<QueryRouter::ServerSpec> server_specs;
  server_specs.push_back(GetServerSpec("server_3"));
  server_specs.push_back(GetServerSpec("server_4"));

  ECCLESIA_ASSIGN_OR_FAIL(
      auto query_router,
      QueryRouter::Create(
          router_spec, std::move(server_specs),
          [&](const QuerySpec &, const QueryEngineParams &,
              std::unique_ptr<IdAssigner>)
              -> absl::StatusOr<std::unique_ptr<QueryEngineIntf>> {
            return FileBackedQueryEngine::Create(
                fs_.GetTruePath(kQueryResultDir));
          }));

  {
    absl::flat_hash_set<QueryRouterCallbacks> expected_callbacks = {};
    query_router->ExecuteQuery(
        {"query_a"},
        [&expected_callbacks](const QueryRouter::ServerInfo &server_info,
                              const QueryResult &result) {
          auto it = expected_callbacks.find(
              QueryRouterCallbacks{result.query_id(), server_info});
          ASSERT_NE(it, expected_callbacks.end());
          expected_callbacks.erase(it);
        });

    EXPECT_THAT(expected_callbacks, IsEmpty());
  }
}

TEST_F(QueryRouterTest, QueryAndServerSpecPartialIntersect) {
  QueryRouterSpec router_spec = ParseTextProtoOrDie(absl::Substitute(
      R"pb(
        query_pattern: PATTERN_SERIAL_ALL
        selection_specs {
          key: "query_a"
          value {
            query_selection_specs {
              select {
                server_type: SERVER_TYPE_BMCWEB
                server_class: SERVER_CLASS_COMPUTE
                server_tag: "server_1"
                server_tag: "server_2"
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

  std::vector<QueryRouter::ServerSpec> server_specs;
  server_specs.push_back(GetServerSpec("server_2"));
  server_specs.push_back(GetServerSpec("server_4"));

  ECCLESIA_ASSIGN_OR_FAIL(
      auto query_router,
      QueryRouter::Create(
          router_spec, std::move(server_specs),
          [&](const QuerySpec &, const QueryEngineParams &,
              std::unique_ptr<IdAssigner>)
              -> absl::StatusOr<std::unique_ptr<QueryEngineIntf>> {
            return FileBackedQueryEngine::Create(
                fs_.GetTruePath(kQueryResultDir));
          }));

  {
    absl::flat_hash_set<QueryRouterCallbacks> expected_callbacks = {
        {"query_a",
         {"server_2", SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB,
          SelectionSpec::SelectionClass::SERVER_CLASS_COMPUTE}}};

    query_router->ExecuteQuery(
        {"query_a"},
        [&expected_callbacks](const QueryRouter::ServerInfo &server_info,
                              const QueryResult &result) {
          auto it = expected_callbacks.find(
              QueryRouterCallbacks{result.query_id(), server_info});
          ASSERT_NE(it, expected_callbacks.end());
          expected_callbacks.erase(it);
        });

    EXPECT_THAT(expected_callbacks, IsEmpty());
  }
}

TEST_F(QueryRouterTest, InvalidQuerySpec) {
  QueryRouterSpec router_spec = ParseTextProtoOrDie(absl::Substitute(
      R"pb(
        query_pattern: PATTERN_SERIAL_ALL
        selection_specs {
          key: "query_a"
          value {
            query_selection_specs {
              select { server_type: SERVER_TYPE_BMCWEB server_tag: "server_1" }
              query_and_rule_path { query_path: "$0/does_not_exist.textproto" }
            }
          }
        }
      )pb",
      apifs_.GetPath()));

  std::vector<QueryRouter::ServerSpec> server_specs;
  server_specs.push_back(GetServerSpec("server_1"));

  EXPECT_THAT(QueryRouter::Create(
                  router_spec, std::move(server_specs),
                  [&](const QuerySpec &, const QueryEngineParams &,
                      std::unique_ptr<IdAssigner>)
                      -> absl::StatusOr<std::unique_ptr<QueryEngineIntf>> {
                    return FileBackedQueryEngine::Create(
                        fs_.GetTruePath(kQueryResultDir));
                  }),
              IsStatusNotFound());
}

TEST_F(QueryRouterTest, UnsupportedQueryPattern) {
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
      )pb",
      apifs_.GetPath()));

  std::vector<QueryRouter::ServerSpec> server_specs;
  server_specs.push_back(GetServerSpec("server_1"));

  EXPECT_THAT(QueryRouter::Create(
                  router_spec, std::move(server_specs),
                  [&](const QuerySpec &, const QueryEngineParams &,
                      std::unique_ptr<IdAssigner>)
                      -> absl::StatusOr<std::unique_ptr<QueryEngineIntf>> {
                    return FileBackedQueryEngine::Create(
                        fs_.GetTruePath(kQueryResultDir));
                  }),
              IsStatusFailedPrecondition());
}

class QueryRouterFailureTest
    : public QueryRouterTest,
      public testing::WithParamInterface<absl::string_view> {};

TEST_P(QueryRouterFailureTest, WithoutMaxThreadValue) {
  QueryRouterSpec router_spec = ParseTextProtoOrDie(absl::Substitute(
      R"pb(
        query_pattern: $0
        selection_specs {
          key: "query_a"
          value {
            query_selection_specs {
              select { server_type: SERVER_TYPE_BMCWEB server_tag: "server_1" }
              query_and_rule_path { query_path: "$1/does_not_exist.textproto" }
            }
          }
        }
      )pb",
      GetParam(), apifs_.GetPath()));

  std::vector<QueryRouter::ServerSpec> server_specs;
  server_specs.push_back(GetServerSpec("server_1"));
  EXPECT_THAT(QueryRouter::Create(
                  router_spec, std::move(server_specs),
                  [&](const QuerySpec &, const QueryEngineParams &,
                      std::unique_ptr<IdAssigner>)
                      -> absl::StatusOr<std::unique_ptr<QueryEngineIntf>> {
                    return FileBackedQueryEngine::Create(
                        fs_.GetTruePath(kQueryResultDir));
                  }),
              IsStatusFailedPrecondition());
}

INSTANTIATE_TEST_SUITE_P(CheckQueryRouterCreateFailure, QueryRouterFailureTest,
                         Values("PATTERN_SERIAL_AGENT",
                                "PATTERN_PARALLEL_ALL"));

TEST_F(QueryRouterTest, QueryEngineCreateFailure) {
  QueryRouterSpec router_spec = ParseTextProtoOrDie(absl::Substitute(
      R"pb(
        query_pattern: PATTERN_SERIAL_ALL
        selection_specs {
          key: "query_a"
          value {
            query_selection_specs {
              select { server_type: SERVER_TYPE_BMCWEB server_tag: "server_1" }
              query_and_rule_path { query_path: "$0/query_a.textproto" }
            }
          }
        }
      )pb",
      apifs_.GetPath()));

  std::vector<QueryRouter::ServerSpec> server_specs;
  server_specs.push_back(GetServerSpec("server_1"));

  EXPECT_THAT(QueryRouter::Create(
                  router_spec, std::move(server_specs),
                  [&](const QuerySpec &, const QueryEngineParams &,
                      std::unique_ptr<IdAssigner>)
                      -> absl::StatusOr<std::unique_ptr<QueryEngineIntf>> {
                    return absl::InternalError("Failed to create QueryEngine");
                  }),
              IsStatusInternal());
}

TEST_F(QueryRouterTest, CheckFeatureFlags) {
  QueryRouterSpec router_spec = ParseTextProtoOrDie(absl::Substitute(
      R"pb(
        query_pattern: PATTERN_SERIAL_ALL
        features {
          enable_redfish_metrics: true
          fail_on_first_error: true
          log_redfish_traces: true
        }
        selection_specs {
          key: "query_a"
          value {
            query_selection_specs {
              select { server_type: SERVER_TYPE_BMCWEB server_tag: "server_1" }
              query_and_rule_path { query_path: "$0/query_a.textproto" }
            }
          }
        }
      )pb",
      apifs_.GetPath()));

  std::vector<QueryRouter::ServerSpec> server_specs;
  server_specs.push_back(GetServerSpec("server_1"));

  EXPECT_THAT(QueryRouter::Create(
                  router_spec, std::move(server_specs),
                  [&](const QuerySpec &, const QueryEngineParams &params,
                      std::unique_ptr<IdAssigner>)
                      -> absl::StatusOr<std::unique_ptr<QueryEngineIntf>> {
                    EXPECT_TRUE(params.features.enable_redfish_metrics());
                    EXPECT_TRUE(params.features.fail_on_first_error());
                    EXPECT_TRUE(params.features.log_redfish_traces());
                    return FileBackedQueryEngine::Create(
                        fs_.GetTruePath(kQueryResultDir));
                  }),
              IsOk());
}

TEST_F(QueryRouterTest, CheckLocationStableIdConfiguration) {
  QueryRouterSpec router_spec = ParseTextProtoOrDie(absl::Substitute(
      R"pb(
        query_pattern: PATTERN_SERIAL_ALL
        selection_specs {
          key: "query_a"
          value {
            query_selection_specs {
              select { server_type: SERVER_TYPE_BMCWEB server_tag: "server_1" }
              query_and_rule_path { query_path: "$0/query_a.textproto" }
            }
          }
        }
      )pb",
      apifs_.GetPath()));

  std::vector<QueryRouter::ServerSpec> server_specs;
  server_specs.push_back(GetServerSpec("server_1"));

  EXPECT_THAT(QueryRouter::Create(
                  router_spec, std::move(server_specs),
                  [&](const QuerySpec &, const QueryEngineParams &params,
                      std::unique_ptr<IdAssigner>)
                      -> absl::StatusOr<std::unique_ptr<QueryEngineIntf>> {
                    EXPECT_EQ(params.stable_id_type,
                              ecclesia::QueryEngineParams::RedfishStableIdType::
                                  kRedfishLocation);
                    return FileBackedQueryEngine::Create(
                        fs_.GetTruePath(kQueryResultDir));
                  }),
              IsOk());
}

TEST_F(QueryRouterTest, CheckLocationDerivedStableIdConfiguration) {
  QueryRouterSpec router_spec = ParseTextProtoOrDie(absl::Substitute(
      R"pb(
        query_pattern: PATTERN_SERIAL_ALL
        selection_specs {
          key: "query_a"
          value {
            query_selection_specs {
              select { server_type: SERVER_TYPE_BMCWEB server_tag: "server_1" }
              query_and_rule_path { query_path: "$0/query_a.textproto" }
            }
          }
        }
      )pb",
      apifs_.GetPath()));

  std::vector<QueryRouter::ServerSpec> server_specs;
  server_specs.push_back(GetServerSpec(
      "server_1", ecclesia::QueryEngineParams::RedfishStableIdType::
                      kRedfishLocationDerived));

  EXPECT_THAT(QueryRouter::Create(
                  router_spec, std::move(server_specs),
                  [&](const QuerySpec &, const QueryEngineParams &params,
                      std::unique_ptr<IdAssigner>)
                      -> absl::StatusOr<std::unique_ptr<QueryEngineIntf>> {
                    EXPECT_EQ(params.stable_id_type,
                              ecclesia::QueryEngineParams::RedfishStableIdType::
                                  kRedfishLocationDerived);
                    return FileBackedQueryEngine::Create(
                        fs_.GetTruePath(kQueryResultDir));
                  }),
              IsOk());
}

TEST_F(QueryRouterTest, GetRedfishInterfaceSuccess) {
  QueryRouterSpec router_spec = ParseTextProtoOrDie(absl::Substitute(
      R"pb(
        query_pattern: PATTERN_SERIAL_ALL
        selection_specs {
          key: "query_a"
          value {
            query_selection_specs {
              select { server_type: SERVER_TYPE_BMCWEB server_tag: "server_1" }
              query_and_rule_path { query_path: "$0/query_a.textproto" }
            }
          }
        }
      )pb",
      apifs_.GetPath()));

  std::vector<QueryRouter::ServerSpec> server_specs;
  server_specs.push_back(GetServerSpec("server_1"));
  auto mock_qe = std::make_unique<MockQueryEngine>();
  NullRedfish redfish_interface;
  EXPECT_CALL(*mock_qe, GetRedfishInterface)
      .WillOnce(Return(&redfish_interface));

  ECCLESIA_ASSIGN_OR_FAIL(
      auto query_router,
      QueryRouter::Create(
          router_spec, std::move(server_specs),
          [&](const QuerySpec &, const QueryEngineParams &params,
              std::unique_ptr<IdAssigner>)
              -> absl::StatusOr<std::unique_ptr<QueryEngineIntf>> {
            EXPECT_FALSE(params.features.enable_redfish_metrics());
            EXPECT_TRUE(params.features.fail_on_first_error());
            EXPECT_FALSE(params.features.log_redfish_traces());
            return std::move(mock_qe);
          }));

  ECCLESIA_ASSIGN_OR_FAIL(
      RedfishInterface * intf,
      query_router->GetRedfishInterface(
          QueryRouter::ServerInfo{
              .server_tag = "server_1",
              .server_type = SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB,
              .server_class =
                  SelectionSpec::SelectionClass::SERVER_CLASS_COMPUTE},
          RedfishInterfacePasskeyFactory::GetPassKey()));
  ASSERT_THAT(intf, NotNull());
}

TEST_F(QueryRouterTest, GetRedfishInterfaceFailure) {
  QueryRouterSpec router_spec = ParseTextProtoOrDie(absl::Substitute(
      R"pb(
        query_pattern: PATTERN_SERIAL_ALL
        selection_specs {
          key: "query_a"
          value {
            query_selection_specs {
              select { server_type: SERVER_TYPE_BMCWEB server_tag: "server_1" }
              query_and_rule_path { query_path: "$0/query_a.textproto" }
            }
          }
        }
      )pb",
      apifs_.GetPath()));

  std::vector<QueryRouter::ServerSpec> server_specs;
  server_specs.push_back(GetServerSpec("server_1"));
  auto mock_qe = std::make_unique<MockQueryEngine>();
  EXPECT_CALL(*mock_qe, GetRedfishInterface).Times(0);

  ECCLESIA_ASSIGN_OR_FAIL(
      auto query_router,
      QueryRouter::Create(
          router_spec, std::move(server_specs),
          [&](const QuerySpec &, const QueryEngineParams &params,
              std::unique_ptr<IdAssigner>)
              -> absl::StatusOr<std::unique_ptr<QueryEngineIntf>> {
            EXPECT_FALSE(params.features.enable_redfish_metrics());
            EXPECT_TRUE(params.features.fail_on_first_error());
            EXPECT_FALSE(params.features.log_redfish_traces());
            return std::move(mock_qe);
          }));

  ASSERT_THAT(
      query_router->GetRedfishInterface(
          QueryRouter::ServerInfo{
              .server_tag = "unknown_server",
              .server_type = SelectionSpec::SelectionClass::SERVER_TYPE_BMCWEB,
          },
          RedfishInterfacePasskeyFactory::GetPassKey()),
      IsStatusNotFound());

  ASSERT_THAT(
      query_router->GetRedfishInterface(
          QueryRouter::ServerInfo{
              .server_tag = "server_1",
              .server_type =
                  SelectionSpec::SelectionClass::SERVER_TYPE_UNSPECIFIED,
          },
          RedfishInterfacePasskeyFactory::GetPassKey()),
      IsStatusNotFound());
}

}  // namespace
}  // namespace ecclesia
