/*
 * Copyright 2022 Google LLC
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

#include "ecclesia/lib/redfish/transport/struct_proto_conversion.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "google/protobuf/struct.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/testing/proto.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {
namespace {

TEST(StructProtoConversionTest, StructToJson) {
  google::protobuf::Struct test_struct =
      ParseTextAsProtoOrDie<google::protobuf::Struct>(R"pb(
        fields {
          key: "test_null"
          value: { null_value: NULL_VALUE }
        }
        fields {
          key: "test_bool"
          value: { bool_value: false }
        }
        fields {
          key: "test_list"
          value: {
            list_value: {
              values: { number_value: 1.0 }
              values: { string_value: "test" }
            }
          }
        }
        fields {
          key: "test_num"
          value: { number_value: 1.0 }
        }
        fields {
          key: "test_string"
          value: { string_value: "test" }
        }
        fields {
          key: "test_struct"
          value: {
            struct_value: {
              fields {
                key: "test_sublist"
                value: {
                  list_value: {
                    values: { number_value: 1.0 }
                    values: { string_value: "test" }
                  }
                }
              }
            }
          }
        }
      )pb");
  nlohmann::json test_json = StructToJson(test_struct);
  nlohmann::json expected_json = nlohmann::json::parse(R"json({
    "test_null": null,
    "test_bool": false,
    "test_list": [1.0, "test"],
    "test_num": 1.0,
    "test_string": "test",
    "test_struct":{
      "test_sublist": [1.0, "test"]
    }
  })json");
  EXPECT_EQ(test_json, expected_json);
}

TEST(StructProtoConversionTest, JsonToStruct) {
  nlohmann::json test_json = nlohmann::json::parse(R"json({
  "test_null": null,
    "test_bool": false,
    "test_list": [2.0, "test2", false],
    "test_num": 1.0,
    "test_string": "test",
    "test_struct":{
      "test_num": 3.0
    }
  })json");
  google::protobuf::Struct test_struct;
  test_struct = JsonToStruct(test_json);
  google::protobuf::Struct expected_struct =
      ParseTextAsProtoOrDie<google::protobuf::Struct>(R"pb(
        fields {
          key: "test_null"
          value: { null_value: NULL_VALUE }
        }
        fields {
          key: "test_bool"
          value: { bool_value: false }
        }
        fields {
          key: "test_list"
          value: {
            list_value: {
              values: { number_value: 2.0 }
              values: { string_value: "test2" }
              values: { bool_value: false }
            }
          }
        }
        fields {
          key: "test_num"
          value: { number_value: 1.0 }
        }
        fields {
          key: "test_string"
          value: { string_value: "test" }
        }
        fields {
          key: "test_struct"
          value: {
            struct_value: {
              fields {
                key: "test_num"
                value: { number_value: 3.0 }
              }
            }
          }
        }
      )pb");
  EXPECT_THAT(test_struct, EqualsProto(expected_struct));
}

TEST(StructProtoConversionTest, EmptyStructToJson) {
  google::protobuf::Struct test_struct;
  nlohmann::json test_json = StructToJson(test_struct);
  nlohmann::json expected_json = nlohmann::json::parse(R"json({
  })json");
  EXPECT_EQ(test_json, expected_json);
}

TEST(StructProtoConversionTest, EmptyJsonToStruct) {
  nlohmann::json test_json = nlohmann::json::parse(R"json({
  })json");
  google::protobuf::Struct test_struct = JsonToStruct(test_json);
  google::protobuf::Struct expected_struct =
      ParseTextAsProtoOrDie<google::protobuf::Struct>(R"pb(
      )pb");
  EXPECT_THAT(test_struct, EqualsProto(expected_struct));
}

TEST(StructProtoConversionTest, StructToJsonNumberTest) {
  google::protobuf::Struct test_struct =
      ParseTextAsProtoOrDie<google::protobuf::Struct>(R"pb(
        fields {
          key: "test_integer"
          value: { number_value: 1234.0 }
        }
        fields {
          key: "test_double"
          value: { number_value: 1234.5 }
        }
      )pb");

  nlohmann::json json_int = StructToJson(test_struct)["test_integer"];
  EXPECT_TRUE(json_int.is_number_integer());
  EXPECT_FALSE(json_int.is_number_float());
  EXPECT_EQ(json_int, 1234);

  nlohmann::json json_double = StructToJson(test_struct)["test_double"];
  EXPECT_TRUE(json_double.is_number_float());
  EXPECT_FALSE(json_double.is_number_integer());
  EXPECT_DOUBLE_EQ(json_double, 1234.5);
}

}  // namespace
}  // namespace ecclesia
