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

#include "ecclesia/lib/redfish/redpath/engine/normalizer.h"

#include <memory>
#include <optional>
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/functional/function_ref.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/testing/status.h"
#include "single_include/nlohmann/json.hpp"
#include "ecclesia/lib/testing/proto.h"

namespace ecclesia {
namespace {

// Test Redfish Object class with mockable Get().
class MockableGetRedfishObject : public RedfishObject {
 public:
  MockableGetRedfishObject() = default;
  MockableGetRedfishObject(const MockableGetRedfishObject &) = delete;
  RedfishVariant operator[](const std::string &node_name) const override {
    return RedfishVariant(
        absl::UnimplementedError("TestRedfishObject [] unsupported"));
  }
  nlohmann::json GetContentAsJson() const override { return object_; }
  MOCK_METHOD(RedfishVariant, Get,
              (const std::string &node_name, GetParams params),
              (const, override));
  std::string DebugString() const override {
    return std::string(object_.dump());
  }
  std::optional<std::string> GetUriString() const override { return ""; }
  absl::StatusOr<std::unique_ptr<RedfishObject>> EnsureFreshPayload(
      GetParams params) override {
    return std::make_unique<MockableGetRedfishObject>();
  }
  void ForEachProperty(
      absl::FunctionRef<ecclesia::RedfishIterReturnValue(
          absl::string_view key, RedfishVariant value)> /*unused*/) override {}

 private:
  nlohmann::json object_ = nlohmann::json::parse(R"json(
      {
        "A1": "fan0",
        "A2": false,
        "A3": 7.5,
        "A4": 5,
        "A5": "2022-02-08T02:02:11Z",
        "A6": ["X", "Y", "Z"],
        "A7": [false, true],
        "A8": [7.5, 8.5],
        "A9": [6, 7],
        "A10": ["2022-02-07T02:02:11Z", "2022-02-09T02:02:11Z"],
        "A11": "2022-02-07T02:02:11Z",
        "A12": 5,
        "A13": [5,6],
        "A14": 6
      }
    )json");
};

class NormalizerImplTest : public testing::Test {
 protected:
  explicit NormalizerImplTest() = default;

  std::unique_ptr<Normalizer> normalizer_;
};

TEST_F(NormalizerImplTest, TestNormalizerRegular) {
  DelliciusQuery query_sensor = ParseTextProtoOrDie(
      R"pb(subquery {
             subquery_id: "Sensors"
             properties { property: "A1" type: STRING }
             properties { property: "A2" type: BOOLEAN }
             properties { property: "A3" type: DOUBLE }
             properties { property: "A4" type: INT64 }
             properties { property: "A5" type: DATE_TIME_OFFSET }
             properties {
               property: "A6"
               type: STRING
               property_element_type: COLLECTION_PRIMITIVE
             }
             properties {
               property: "A7"
               type: BOOLEAN
               property_element_type: COLLECTION_PRIMITIVE
             }
             properties {
               property: "A8"
               type: DOUBLE
               property_element_type: COLLECTION_PRIMITIVE
             }
             properties {
               property: "A9"
               type: INT64
               property_element_type: COLLECTION_PRIMITIVE
             }
             properties {
               property: "A10"
               type: DATE_TIME_OFFSET
               property_element_type: COLLECTION_PRIMITIVE
             }
           })pb");

  ecclesia::QueryResultData expected_pb = ParseTextProtoOrDie(R"pb(
    fields {
      key: "A1"
      value { string_value: "fan0" }
    }
    fields {
      key: "A2"
      value { bool_value: false }
    }
    fields {
      key: "A3"
      value { double_value: 7.5 }
    }
    fields {
      key: "A4"
      value { int_value: 5 }
    }
    fields {
      key: "A5"
      value { timestamp_value: { seconds: 1644285731 } }
    }
    fields {
      key: "A6"
      value {
        list_value {
          values { string_value: "X" }
          values { string_value: "Y" }
          values { string_value: "Z" }
        }
      }
    }
    fields {
      key: "A7"
      value {
        list_value {
          values { bool_value: false }
          values { bool_value: true }
        }
      }
    }
    fields {
      key: "A8"
      value {
        list_value {
          values { double_value: 7.5 }
          values { double_value: 8.5 }
        }
      }
    }
    fields {
      key: "A9"
      value {
        list_value {
          values { int_value: 6 }
          values { int_value: 7 }
        }
      }
    }
    fields {
      key: "A10"
      value {
        list_value {
          values { timestamp_value { seconds: 1644199331 } }
          values { timestamp_value { seconds: 1644372131 } }
        }
      }
    }
  )pb");
  MockableGetRedfishObject obj;

  ASSERT_GT(query_sensor.subquery_size(), 0);
  normalizer_ = std::make_unique<NormalizerImpl>();
  EXPECT_THAT(normalizer_->Normalize(obj, query_sensor.subquery(0)),
              IsOkAndHolds(EqualsProto(expected_pb)));
}

TEST_F(NormalizerImplTest, TestNormalizerPropertyNameAvailable) {
  DelliciusQuery query_sensor = ParseTextProtoOrDie(
      R"pb(subquery {
             subquery_id: "Sensors"
             properties { name: "fan0" property: "A1" type: STRING }
           })pb");

  ecclesia::QueryResultData expected_pb = ParseTextProtoOrDie(R"pb(
    fields {
      key: "fan0"
      value { string_value: "fan0" }
    }
  )pb");
  MockableGetRedfishObject obj;

  ASSERT_GT(query_sensor.subquery_size(), 0);
  normalizer_ = std::make_unique<NormalizerImpl>();
  EXPECT_THAT(normalizer_->Normalize(obj, query_sensor.subquery(0)),
              IsOkAndHolds(EqualsProto(expected_pb)));
}

TEST_F(NormalizerImplTest, TestNormalizerPropertyNotValid) {
  DelliciusQuery query_sensor = ParseTextProtoOrDie(
      R"pb(subquery {
             subquery_id: "Sensors"
             properties { property: "A1" type: UNDEFINED }
           })pb");

  ecclesia::QueryResultData expected_pb = ParseTextProtoOrDie(R"pb(
  )pb");
  MockableGetRedfishObject obj;

  ASSERT_GT(query_sensor.subquery_size(), 0);
  normalizer_ = std::make_unique<NormalizerImpl>();
  EXPECT_THAT(normalizer_->Normalize(obj, query_sensor.subquery(0)),
              IsOkAndHolds(EqualsProto(expected_pb)));
}

TEST_F(NormalizerImplTest, TestNormalizerPropertyTimestampNotValid) {
  DelliciusQuery query_sensor = ParseTextProtoOrDie(
      R"pb(subquery {
             subquery_id: "Sensors"
             properties { property: "A12" type: DATE_TIME_OFFSET }
           })pb");

  ecclesia::QueryResultData expected_pb = ParseTextProtoOrDie(R"pb(
  )pb");
  MockableGetRedfishObject obj;

  ASSERT_GT(query_sensor.subquery_size(), 0);
  normalizer_ = std::make_unique<NormalizerImpl>();
  EXPECT_THAT(normalizer_->Normalize(obj, query_sensor.subquery(0)),
              IsOkAndHolds(EqualsProto(expected_pb)));
}

TEST_F(NormalizerImplTest, TestNormalizerPropertyTimestampArrayNotValid) {
  DelliciusQuery query_sensor = ParseTextProtoOrDie(
      R"pb(subquery {
             subquery_id: "Sensors"
             properties {
               property: "A13"
               type: DATE_TIME_OFFSET
               property_element_type: COLLECTION_PRIMITIVE
             }
           })pb");

  ecclesia::QueryResultData expected_pb = ParseTextProtoOrDie(R"pb(
  )pb");
  MockableGetRedfishObject obj;

  ASSERT_GT(query_sensor.subquery_size(), 0);
  normalizer_ = std::make_unique<NormalizerImpl>();
  EXPECT_THAT(normalizer_->Normalize(obj, query_sensor.subquery(0)),
              IsOkAndHolds(EqualsProto(expected_pb)));
}

TEST_F(NormalizerImplTest, TestNormalizerPropertyArrayNotValid) {
  DelliciusQuery query_sensor = ParseTextProtoOrDie(
      R"pb(subquery {
             subquery_id: "Sensors"
             properties {
               property: "A14"
               type: DATE_TIME_OFFSET
               property_element_type: COLLECTION_PRIMITIVE
             }
           })pb");

  ecclesia::QueryResultData expected_pb = ParseTextProtoOrDie(R"pb(
  )pb");
  MockableGetRedfishObject obj;

  ASSERT_GT(query_sensor.subquery_size(), 0);
  normalizer_ = std::make_unique<NormalizerImpl>();
  EXPECT_THAT(normalizer_->Normalize(obj, query_sensor.subquery(0)),
              IsOkAndHolds(EqualsProto(expected_pb)));
}
}  // namespace
}  // namespace ecclesia
