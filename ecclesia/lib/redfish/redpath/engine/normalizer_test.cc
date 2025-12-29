/*
 * Copyright 2025 Google LLC
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

#include "ecclesia/lib/redfish/redpath/engine/normalizer.h"

#include <memory>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"

namespace ecclesia {
namespace {

using ::testing::_;
using ::testing::Return;

class MockNormalizerImpl : public RedpathNormalizer::ImplInterface {
 public:
  MOCK_METHOD(absl::Status, Normalize,
              (const RedfishObject& redfish_object,
               const DelliciusQuery::Subquery& query,
               ecclesia::QueryResultData& data_set,
               const RedpathNormalizerOptions& options),
              (override));
  MOCK_METHOD(absl::Status, Normalize,
              (const DelliciusQuery& query, ecclesia::QueryResult& query_result,
               const RedpathNormalizerOptions& options),
              (override));
};

TEST(RedpathNormalizerTest, NoNormalizersAdded) {
  RedpathNormalizer normalizer;
  DummyRedfishObject dummy_redfish_object;
  DelliciusQuery::Subquery subquery;
  RedpathNormalizerOptions options;
  auto status = normalizer.Normalize(dummy_redfish_object, subquery, options);
  EXPECT_EQ(status.status().code(), absl::StatusCode::kNotFound);
}

TEST(RedpathNormalizerTest, SingleNormalizerSuccess) {
  RedpathNormalizer normalizer;
  auto mock_impl = std::make_unique<MockNormalizerImpl>();
  EXPECT_CALL(*mock_impl, Normalize(_, _, _, _))
      .WillOnce([](const RedfishObject& redfish_object,
                   const DelliciusQuery::Subquery& query,
                   ecclesia::QueryResultData& data_set,
                   const RedpathNormalizerOptions& options) {
        (*data_set.mutable_fields())["test_query"].set_string_value(
            "test_value");
        return absl::OkStatus();
      });
  normalizer.AddRedpathNormalizer(std::move(mock_impl));

  DummyRedfishObject dummy_redfish_object;
  DelliciusQuery::Subquery subquery;
  RedpathNormalizerOptions options;
  auto status = normalizer.Normalize(dummy_redfish_object, subquery, options);
  EXPECT_EQ(status.status().code(), absl::StatusCode::kOk);
  EXPECT_EQ(status->fields().at("test_query").string_value(), "test_value");
}

TEST(RedpathNormalizerTest, SingleNormalizerFailure) {
  RedpathNormalizer normalizer;
  auto mock_impl = std::make_unique<MockNormalizerImpl>();
  EXPECT_CALL(*mock_impl, Normalize(_, _, _, _))
      .WillOnce(Return(absl::InternalError("Test error")));
  normalizer.AddRedpathNormalizer(std::move(mock_impl));

  DummyRedfishObject dummy_redfish_object;
  DelliciusQuery::Subquery subquery;
  RedpathNormalizerOptions options;
  auto status = normalizer.Normalize(dummy_redfish_object, subquery, options);
  EXPECT_EQ(status.status().code(), absl::StatusCode::kInternal);
}

TEST(RedpathNormalizerTest, ChainedNormalizersSuccess) {
  RedpathNormalizer normalizer;
  auto mock_impl1 = std::make_unique<MockNormalizerImpl>();
  EXPECT_CALL(*mock_impl1, Normalize(_, _, _, _))
      .WillOnce([](const RedfishObject& redfish_object,
                   const DelliciusQuery::Subquery& query,
                   ecclesia::QueryResultData& data_set,
                   const RedpathNormalizerOptions& options) {
        return absl::OkStatus();
      });
  normalizer.AddRedpathNormalizer(std::move(mock_impl1));

  auto mock_impl2 = std::make_unique<MockNormalizerImpl>();
  EXPECT_CALL(*mock_impl2, Normalize(_, _, _, _))
      .WillOnce([](const RedfishObject& redfish_object,
                   const DelliciusQuery::Subquery& query,
                   ecclesia::QueryResultData& data_set,
                   const RedpathNormalizerOptions& options) {
        (*data_set.mutable_fields())["new_field"].set_string_value("new_value");
        return absl::OkStatus();
      });
  normalizer.AddRedpathNormalizer(std::move(mock_impl2));

  DummyRedfishObject dummy_redfish_object;
  DelliciusQuery::Subquery subquery;
  RedpathNormalizerOptions options;
  auto status = normalizer.Normalize(dummy_redfish_object, subquery, options);
  EXPECT_EQ(status.status().code(), absl::StatusCode::kOk);
  EXPECT_EQ(status->fields().at("new_field").string_value(), "new_value");
}

TEST(RedpathNormalizerTest, ChainedNormalizersFailure) {
  RedpathNormalizer normalizer;
  auto mock_impl1 = std::make_unique<MockNormalizerImpl>();
  EXPECT_CALL(*mock_impl1, Normalize(_, _, _, _))
      .WillOnce(Return(absl::OkStatus()));
  normalizer.AddRedpathNormalizer(std::move(mock_impl1));

  auto mock_impl2 = std::make_unique<MockNormalizerImpl>();
  EXPECT_CALL(*mock_impl2, Normalize(_, _, _, _))
      .WillOnce(Return(absl::InternalError("Chain failed")));
  normalizer.AddRedpathNormalizer(std::move(mock_impl2));

  DummyRedfishObject dummy_redfish_object;
  DelliciusQuery::Subquery subquery;
  RedpathNormalizerOptions options;
  auto status = normalizer.Normalize(dummy_redfish_object, subquery, options);
  EXPECT_EQ(status.status().code(), absl::StatusCode::kInternal);
}

TEST(RedpathNormalizerTest, NormalizeWithDataSetSuccess) {
  RedpathNormalizer normalizer;
  auto mock_impl = std::make_unique<MockNormalizerImpl>();
  EXPECT_CALL(*mock_impl, Normalize(_, _, _, _))
      .WillOnce([](const RedfishObject& redfish_object,
                   const DelliciusQuery::Subquery& query,
                   ecclesia::QueryResultData& data_set,
                   const RedpathNormalizerOptions& options) {
        (*data_set.mutable_fields())["added_field"].set_string_value("added");
        return absl::OkStatus();
      });
  normalizer.AddRedpathNormalizer(std::move(mock_impl));

  DummyRedfishObject dummy_redfish_object;
  DelliciusQuery::Subquery subquery;
  RedpathNormalizerOptions options;
  ecclesia::QueryResultData data_set;
  auto status = normalizer.Normalize(dummy_redfish_object, subquery, data_set,
                                     options, false);
  EXPECT_EQ(status.code(), absl::StatusCode::kOk);
  EXPECT_EQ(data_set.fields().at("added_field").string_value(), "added");
}

TEST(RedpathNormalizerTest, NormalizeDataSetOnly) {
  RedpathNormalizer normalizer;
  auto mock_impl = std::make_unique<MockNormalizerImpl>();
  EXPECT_CALL(*mock_impl, Normalize(_, _, _, _))
      .WillOnce([](const RedfishObject& redfish_object,
                   const DelliciusQuery::Subquery& query,
                   ecclesia::QueryResultData& data_set,
                   const RedpathNormalizerOptions& options) {
        (*data_set.mutable_fields())["set_only_field"].set_bool_value(true);
        return absl::OkStatus();
      });
  normalizer.AddRedpathNormalizer(std::move(mock_impl));

  RedpathNormalizerOptions options;
  ecclesia::QueryResultData data_set;
  auto status = normalizer.Normalize(data_set, options);
  EXPECT_EQ(status.code(), absl::StatusCode::kOk);
  EXPECT_EQ(data_set.fields().at("set_only_field").bool_value(), true);
}

TEST(RedpathNormalizerTest, NormalizeDataSetOnlyFailOnNonEmptyDataset) {
  RedpathNormalizer normalizer;
  auto mock_impl = std::make_unique<MockNormalizerImpl>();
  EXPECT_CALL(*mock_impl, Normalize(_, _, _, _))
      .WillOnce([](const RedfishObject& redfish_object,
                   const DelliciusQuery::Subquery& query,
                   ecclesia::QueryResultData& data_set,
                   const RedpathNormalizerOptions& options) {
        data_set.mutable_fields()->clear();
        return absl::OkStatus();
      });
  normalizer.AddRedpathNormalizer(std::move(mock_impl));

  RedpathNormalizerOptions options;
  ecclesia::QueryResultData data_set;
  (*data_set.mutable_fields())["set_only_field"].set_bool_value(true);
  auto status = normalizer.Normalize(data_set, options);
  EXPECT_EQ(status.code(), absl::StatusCode::kNotFound);
}

TEST(RedpathNormalizerTest, NormalizeDataSetOnlySuccessOnEmptyDataset) {
  RedpathNormalizer normalizer;
  auto mock_impl = std::make_unique<MockNormalizerImpl>();
  EXPECT_CALL(*mock_impl, Normalize(_, _, _, _))
      .WillOnce([](const RedfishObject& redfish_object,
                   const DelliciusQuery::Subquery& query,
                   ecclesia::QueryResultData& data_set,
                   const RedpathNormalizerOptions& options) {
        return absl::OkStatus();
      });
  normalizer.AddRedpathNormalizer(std::move(mock_impl));

  RedpathNormalizerOptions options;
  ecclesia::QueryResultData data_set;
  auto status = normalizer.Normalize(data_set, options);
  EXPECT_EQ(status.code(), absl::StatusCode::kOk);
  EXPECT_EQ(data_set.fields_size(), 0);
}

TEST(RedpathNormalizerTest, EmptyDatasetErrorFlagTrue) {
  RedpathNormalizer normalizer;
  auto mock_impl = std::make_unique<MockNormalizerImpl>();
  EXPECT_CALL(*mock_impl, Normalize(_, _, _, _))
      .WillOnce(Return(absl::OkStatus()));  // Does not add any fields
  normalizer.AddRedpathNormalizer(std::move(mock_impl));

  DummyRedfishObject dummy_redfish_object;
  DelliciusQuery::Subquery subquery;
  RedpathNormalizerOptions options;
  auto status = normalizer.Normalize(dummy_redfish_object, subquery, options);
  EXPECT_EQ(status.status().code(), absl::StatusCode::kNotFound);
}

TEST(RedpathNormalizerTest, EmptyDatasetErrorFlagFalse) {
  RedpathNormalizer normalizer;
  auto mock_impl = std::make_unique<MockNormalizerImpl>();
  EXPECT_CALL(*mock_impl, Normalize(_, _, _, _))
      .WillOnce(Return(absl::OkStatus()));  // Does not add any fields
  normalizer.AddRedpathNormalizer(std::move(mock_impl));

  DummyRedfishObject dummy_redfish_object;
  DelliciusQuery::Subquery subquery;
  RedpathNormalizerOptions options;
  ecclesia::QueryResultData data_set;
  auto status = normalizer.Normalize(dummy_redfish_object, subquery, data_set,
                                     options, false);
  EXPECT_EQ(status.code(), absl::StatusCode::kOk);
  EXPECT_EQ(data_set.fields_size(), 0);
}

}  // namespace
}  // namespace ecclesia
