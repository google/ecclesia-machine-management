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

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/functional/function_ref.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "single_include/nlohmann/json.hpp"

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

class TestRedfishObject : public RedfishObject {
 public:
  TestRedfishObject(nlohmann::json content, std::string odata_type)
      : content_(std::move(content)), odata_type_(std::move(odata_type)) {}

  RedfishVariant operator[](absl::string_view node_name) const override {
    return RedfishVariant(absl::UnimplementedError(""));
  }
  RedfishVariant Get(absl::string_view node_name,
                     GetParams params) const override {
    if (node_name == "@odata.type") {
      class TypeVariantImpl : public RedfishVariant::ImplIntf {
       public:
        explicit TypeVariantImpl(std::string val) : val_(std::move(val)) {}
        std::unique_ptr<RedfishObject> AsObject() const override {
          return nullptr;
        }
        std::unique_ptr<RedfishIterable> AsIterable(
            RedfishVariant::IterableMode mode,
            GetParams params) const override {
          return nullptr;
        }
        std::optional<ecclesia::RedfishTransport::bytes> AsRaw()
            const override {
          return std::nullopt;
        }
        bool GetValue(std::string* val) const override {
          *val = val_;
          return true;
        }
        bool GetValue(int32_t* val) const override { return false; }
        bool GetValue(int64_t* val) const override { return false; }
        bool GetValue(double* val) const override { return false; }
        bool GetValue(bool* val) const override { return false; }
        bool GetValue(absl::Time* val) const override { return false; }
        std::string DebugString() const override { return val_; }
        CacheState IsFresh() const override { return CacheState::kUnknown; }

       private:
        std::string val_;
      };
      return RedfishVariant(std::make_unique<TypeVariantImpl>(odata_type_));
    }
    return RedfishVariant(absl::UnimplementedError(""));
  }
  std::optional<std::string> GetUriString() const override { return ""; }
  absl::StatusOr<std::unique_ptr<RedfishObject>> EnsureFreshPayload(
      GetParams params) override {
    return absl::UnimplementedError("");
  }
  nlohmann::json GetContentAsJson() const override { return content_; }
  std::string DebugString() const override { return ""; }
  void ForEachProperty(
      absl::FunctionRef<RedfishIterReturnValue(
          absl::string_view key, RedfishVariant value)> /*unused*/) override {}

 private:
  nlohmann::json content_;
  std::string odata_type_;
};

TEST(RedpathNormalizerTest, RedpathNormalizerImplDefaultSetsIsRootChassis) {
  auto normalizer = BuildDefaultRedpathNormalizer();

  DelliciusQuery::Subquery subquery;
  // Create JSON that represents a root chassis: no ServiceLabel or
  // PartLocationContext
  nlohmann::json content = nlohmann::json::parse(R"json(
    {
      "Name": "root_chassis"
    }
  )json");
  TestRedfishObject redfish_object(content, "#Chassis.v1_17_0.Chassis");

  RedpathNormalizerOptions options;
  ecclesia::QueryResultData data_set;
  auto status =
      normalizer->Normalize(redfish_object, subquery, data_set, options,
                            /*return_error_on_empty_dataset=*/false);
  ASSERT_TRUE(status.ok());

  // The identifier should be created and is_root should be true
  ASSERT_TRUE(data_set.fields().contains("_id_"));
  EXPECT_TRUE(data_set.fields().at("_id_").identifier().is_root());
}

}  // namespace
}  // namespace ecclesia
