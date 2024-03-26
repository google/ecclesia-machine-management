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

#include "ecclesia/lib/redfish/dellicius/engine/file_backed_query_engine.h"

#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/apifs/apifs.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/status/test_macros.h"
#include "ecclesia/lib/testing/proto.h"
#include "ecclesia/lib/testing/status.h"
#include "google/protobuf/text_format.h"

namespace ecclesia {
namespace {

constexpr absl::string_view kRootDir = "/test/";

class FileBackedQueryEngineTest : public testing::Test {
 protected:
  FileBackedQueryEngineTest()
      : fs_(GetTestTempdirPath()), apifs_(GetTestTempdirPath("test")) {}

  void SetUp() override {
    fs_.CreateDir(kRootDir);

    // Create sample query results
    result_a_ = ParseTextProtoOrDie(R"pb(query_id: "query_a"
                                         data {
                                           fields {
                                             key: "sensor_value"
                                             value { double_value: 100.0 }
                                           }
                                         }
    )pb");

    result_b_ = ParseTextProtoOrDie(R"pb(query_id: "query_b"
                                         data {
                                           fields {
                                             key: "name"
                                             value { string_value: "test" }
                                           }
                                         }
    )pb");
  }

  void CreateQueryResultFile(absl::string_view filename,
                             const QueryResult &result) {
    std::string contents;
    google::protobuf::TextFormat::PrintToString(result, &contents);
    fs_.WriteFile(absl::StrCat(kRootDir, filename), contents);
  }

  TestFilesystem fs_;
  ApifsDirectory apifs_;
  QueryResult result_a_;
  QueryResult result_b_;
};

TEST_F(FileBackedQueryEngineTest, RetrieveResultsNoCache) {
  CreateQueryResultFile("query_a.textproto", result_a_);
  CreateQueryResultFile("query_b.textproto", result_b_);

  ECCLESIA_ASSIGN_OR_FAIL(auto query_engine,
                          FileBackedQueryEngine::Create(apifs_.GetPath()));

  QueryIdToResult query_id_to_result = ParseTextProtoOrDie(R"pb(
    results {
      key: "query_a"
      value {
        query_id: "query_a"
        data {
          fields {
            key: "sensor_value"
            value { double_value: 100 }
          }
        }
      }
    }
  )pb");
  ASSERT_THAT(query_engine->ExecuteRedpathQuery({"query_a"}),
              EqualsProto(query_id_to_result));

  query_id_to_result = ParseTextProtoOrDie(R"pb(
    results {
      key: "query_b"
      value {
        query_id: "query_b"
        data {
          fields {
            key: "name"
            value { string_value: "test" }
          }
        }
      }
    }
  )pb");
  ASSERT_THAT(query_engine->ExecuteRedpathQuery({"query_b"}),
              EqualsProto(query_id_to_result));

  query_id_to_result = ParseTextProtoOrDie(R"pb(
    results {
      key: "query_a"
      value {
        query_id: "query_a"
        data {
          fields {
            key: "sensor_value"
            value { double_value: 100 }
          }
        }
      }
    }
    results {
      key: "query_b"
      value {
        query_id: "query_b"
        data {
          fields {
            key: "name"
            value { string_value: "test" }
          }
        }
      }
    }
  )pb");
  ASSERT_THAT(query_engine->ExecuteRedpathQuery({"query_a", "query_b"}),
              EqualsProto(query_id_to_result));
}

TEST_F(FileBackedQueryEngineTest, QueryResultDoesNotExist) {
  CreateQueryResultFile("query_a.textproto", result_a_);

  ECCLESIA_ASSIGN_OR_FAIL(auto query_engine,
                          FileBackedQueryEngine::Create(apifs_.GetPath()));

  QueryIdToResult query_id_to_result = ParseTextProtoOrDie(
      R"pb(
        results {
          key: "new_query"
          value { status { errors: "No file for query id 'new_query' found" } }
        }
      )pb");

  ASSERT_THAT(query_engine->ExecuteRedpathQuery({"new_query"}),
              EqualsProto(query_id_to_result));
}

TEST_F(FileBackedQueryEngineTest, ValueChangeAcrossCalls) {
  CreateQueryResultFile("query_result.textproto", result_a_);

  ECCLESIA_ASSIGN_OR_FAIL(auto query_engine,
                          FileBackedQueryEngine::Create(apifs_.GetPath()));

  QueryIdToResult query_id_to_result = ParseTextProtoOrDie(R"pb(
    results {
      key: "query_a"
      value {
        query_id: "query_a"
        data {
          fields {
            key: "sensor_value"
            value { double_value: 100 }
          }
        }
      }
    }
  )pb");
  ASSERT_THAT(query_engine->ExecuteRedpathQuery({"query_a"}),
              EqualsProto(query_id_to_result));

  // Now change `sensor_value` and rewrite to the file
  QueryResultDataBuilder builder(result_a_.mutable_data());
  builder["sensor_value"] = static_cast<double>(120);
  CreateQueryResultFile("query_result.textproto", result_a_);

  // Ensure that the value is changed in the next `ExecuteRedpathQuery` call
  query_id_to_result = ParseTextProtoOrDie(R"pb(
    results {
      key: "query_a"
      value {
        query_id: "query_a"
        data {
          fields {
            key: "sensor_value"
            value { double_value: 120 }
          }
        }
      }
    }
  )pb");

  ASSERT_THAT(query_engine->ExecuteRedpathQuery({"query_a"}),
              EqualsProto(query_id_to_result));
}

TEST_F(FileBackedQueryEngineTest, InfiniteCacheTest) {
  CreateQueryResultFile("query_result.textproto", result_a_);

  ECCLESIA_ASSIGN_OR_FAIL(
      auto query_engine,
      FileBackedQueryEngine::Create(apifs_.GetPath(),
                                    FileBackedQueryEngine::Cache::kInfinite));

  QueryIdToResult query_id_to_result = ParseTextProtoOrDie(R"pb(
    results {
      key: "query_a"
      value {
        query_id: "query_a"
        data {
          fields {
            key: "sensor_value"
            value { double_value: 100 }
          }
        }
      }
    }
  )pb");
  ASSERT_THAT(query_engine->ExecuteRedpathQuery({"query_a"}),
              EqualsProto(query_id_to_result));

  // Now change `sensor_value` and rewrite to the file
  QueryResultDataBuilder builder(result_a_.mutable_data());
  builder["sensor_value"] = static_cast<double>(120);
  CreateQueryResultFile("query_result.textproto", result_a_);

  // Ensure that the value has not changed in the next `ExecuteRedpathQuery`
  // call
  ASSERT_THAT(query_engine->ExecuteRedpathQuery({"query_a"}),
              EqualsProto(query_id_to_result));
}

TEST_F(FileBackedQueryEngineTest, PathDoesNotExist) {
  ASSERT_THAT(FileBackedQueryEngine::Create("/does/not/exist"),
              IsStatusNotFound());
}

TEST_F(FileBackedQueryEngineTest, DirectoryIsEmpty) {
  ASSERT_THAT(FileBackedQueryEngine::Create(apifs_.GetPath()),
              IsStatusInternal());
}

TEST_F(FileBackedQueryEngineTest, DuplicateQueriesInDirectory) {
  CreateQueryResultFile("query_a.textproto", result_a_);
  CreateQueryResultFile("query_b.textproto", result_a_);

  ASSERT_THAT(FileBackedQueryEngine::Create(apifs_.GetPath()),
              IsStatusInternal());
}

TEST_F(FileBackedQueryEngineTest, InvalidQueryResultData) {
  fs_.WriteFile(absl::StrCat(kRootDir, "query_a.textproto"), "invalid_data");

  ASSERT_THAT(FileBackedQueryEngine::Create(apifs_.GetPath()),
              IsStatusInternal());
}

TEST_F(FileBackedQueryEngineTest, InvalidQueryResultDataAfterInitialization) {
  CreateQueryResultFile("query_result.textproto", result_a_);

  ECCLESIA_ASSIGN_OR_FAIL(auto query_engine,
                          FileBackedQueryEngine::Create(apifs_.GetPath()));

  QueryIdToResult query_id_to_result = ParseTextProtoOrDie(R"pb(
    results {
      key: "query_a"
      value {
        query_id: "query_a"
        data {
          fields {
            key: "sensor_value"
            value { double_value: 100 }
          }
        }
      }
    }
  )pb");
  ASSERT_THAT(query_engine->ExecuteRedpathQuery({"query_a"}),
              EqualsProto(query_id_to_result));

  // Now write invalid data to the file
  fs_.WriteFile(absl::StrCat(kRootDir, "query_result.textproto"),
                "invalid_data");

  query_id_to_result = ParseTextProtoOrDie(absl::StrFormat(
      R"pb(
        results {
          key: "query_a"
          value {
            status { errors: "Unable to read query result from file: %s" }
          }
        }
      )pb",
      absl::StrCat(apifs_.GetPath(), "/query_result.textproto")));

  ASSERT_THAT(query_engine->ExecuteRedpathQuery({"query_a"}),
              EqualsProto(query_id_to_result));
}

}  // namespace
}  // namespace ecclesia
