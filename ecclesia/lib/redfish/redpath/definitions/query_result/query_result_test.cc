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

#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/redfish/property.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/status/test_macros.h"
#include "ecclesia/lib/testing/proto.h"
#include "ecclesia/lib/testing/status.h"
#include "ecclesia/lib/time/proto.h"

namespace ecclesia {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

DEFINE_REDFISH_PROPERTY(TestPropertyBool, bool, "bool_val");
DEFINE_REDFISH_PROPERTY(TestPropertyDouble, double, "double_val");
DEFINE_REDFISH_PROPERTY(TestPropertyInt, int, "int_val");
DEFINE_REDFISH_PROPERTY(TestPropertyString, std::string, "str_val");

// Redfish properties that -- if not defined in the injected `QueryValue` -- can
// be used to test handling of requesting non-existent values.
DEFINE_REDFISH_PROPERTY(TestPropertyBoolInvalid, bool, "invalid_bool_val");
DEFINE_REDFISH_PROPERTY(TestPropertyDoubleInvalid, double,
                        "invalid_double_val");
DEFINE_REDFISH_PROPERTY(TestPropertyIntInvalid, int, "invalid_int_val");
DEFINE_REDFISH_PROPERTY(TestPropertyStringInvalid, std::string,
                        "invalid_str_val");

class QueryValueBuilderTest : public testing::Test {
 protected:
  QueryValueBuilderTest() : builder_(&value_) {
    EXPECT_EQ(value_.kind_case(), QueryValue::KindCase::KIND_NOT_SET);
  }

  QueryValue value_;
  QueryValueBuilder builder_;
};

TEST_F(QueryValueBuilderTest, BooleanTest) {
  builder_ = true;
  ASSERT_EQ(value_.kind_case(), QueryValue::KindCase::kBoolValue);
  ASSERT_EQ(value_.bool_value(), true);

  builder_ = false;
  ASSERT_EQ(value_.kind_case(), QueryValue::KindCase::kBoolValue);
  ASSERT_EQ(value_.bool_value(), false);
}

TEST_F(QueryValueBuilderTest, IntegerTest) {
  builder_ = static_cast<int64_t>(10);
  ASSERT_EQ(value_.kind_case(), QueryValue::KindCase::kIntValue);
  ASSERT_EQ(value_.int_value(), 10);
}

TEST_F(QueryValueBuilderTest, DoubleTest) {
  builder_ = static_cast<double>(3.14);
  ASSERT_EQ(value_.kind_case(), QueryValue::KindCase::kDoubleValue);
  ASSERT_DOUBLE_EQ(value_.double_value(), 3.14);
}

TEST_F(QueryValueBuilderTest, TimestampTest) {
  google::protobuf::Timestamp ts = ParseTextProtoOrDie(R"pb(
    seconds: 10
    nanos: 1000
  )pb");
  builder_ = std::move(ts);
  ASSERT_EQ(value_.kind_case(), QueryValue::KindCase::kTimestampValue);
  ASSERT_THAT(value_.timestamp_value(), EqualsProto("seconds: 10 nanos: 1000"));
}

// To test cl/625768839
TEST_F(QueryValueBuilderTest, OldDateTimeOffsetTest) {
  std::string time_str("2024-04-16T05:31:39.068778+00:00");
  absl::Time time;
  absl::ParseTime("%Y-%m-%dT%H:%M:%S%Z", time_str, &time, nullptr);
  absl::StatusOr<google::protobuf::Timestamp> timestamp =
      AbslTimeToProtoTime(time);
  google::protobuf::Timestamp ts = timestamp.value();
  builder_ = std::move(ts);
  ASSERT_EQ(value_.kind_case(), QueryValue::KindCase::kTimestampValue);
  // No fractional seconds in the timestamp
  ASSERT_THAT(value_.timestamp_value(),
              EqualsProto("seconds: 1713245499 nanos: 0"));
}

// To test cl/625768839
TEST_F(QueryValueBuilderTest, NewDateTimeOffsetTest) {
  std::string time_str("2024-04-16T05:31:39.068778+00:00");
  absl::Time time;
  absl::ParseTime(absl::RFC3339_full, time_str, &time, nullptr);
  absl::StatusOr<google::protobuf::Timestamp> timestamp =
      AbslTimeToProtoTime(time);
  google::protobuf::Timestamp ts = timestamp.value();
  builder_ = std::move(ts);
  ASSERT_EQ(value_.kind_case(), QueryValue::KindCase::kTimestampValue);
  // Fractional seconds are retained in the timestamp
  ASSERT_THAT(value_.timestamp_value(),
              EqualsProto("seconds: 1713245499 nanos: 68778000"));
}

TEST_F(QueryValueBuilderTest, StringTest) {
  builder_ = "testing";
  ASSERT_EQ(value_.kind_case(), QueryValue::KindCase::kStringValue);
  ASSERT_EQ(value_.string_value(), "testing");
}

TEST_F(QueryValueBuilderTest, StringViewTest) {
  absl::string_view test_str = "testing";
  builder_ = test_str;
  ASSERT_EQ(value_.kind_case(), QueryValue::KindCase::kStringValue);
  ASSERT_EQ(value_.string_value(), "testing");
}

TEST_F(QueryValueBuilderTest, IdentifierTest) {
  Identifier id =
      ParseTextProtoOrDie(R"pb(local_devpath: "/phys/"
                               machine_devpath: "/phys/PE0"
                               embedded_location_context: "chip1/metrics"
      )pb");
  builder_ = std::move(id);
  ASSERT_EQ(value_.kind_case(), QueryValue::KindCase::kIdentifier);
  ASSERT_THAT(value_.identifier(),
              EqualsProto(R"pb(local_devpath: "/phys/"
                               machine_devpath: "/phys/PE0"
                               embedded_location_context: "chip1/metrics"
              )pb"));
}

TEST_F(QueryValueBuilderTest, ListTest) {
  builder_.append("value1");
  builder_.append(static_cast<int64_t>(100));
  builder_.append(3.25);
  ASSERT_EQ(value_.kind_case(), QueryValue::KindCase::kListValue);
  ASSERT_THAT(value_.list_value(),
              EqualsProto(R"pb(values { string_value: "value1" }
                               values { int_value: 100 }
                               values { double_value: 3.25 })pb"));
}

TEST_F(QueryValueBuilderTest, QueryResultDataTest) {
  builder_["value"] = "value1";
  builder_["list_value"].append(static_cast<int64_t>(100));
  builder_["list_value"].append(3.25);
  ASSERT_EQ(value_.kind_case(), QueryValue::KindCase::kSubqueryValue);
  ASSERT_THAT(value_.subquery_value(),
              EqualsProto(R"pb(fields {
                                 key: "list_value"
                                 value {
                                   list_value {
                                     values { int_value: 100 }
                                     values { double_value: 3.25 }
                                   }
                                 }
                               }
                               fields {
                                 key: "value"
                                 value { string_value: "value1" }
                               })pb"));
}

TEST_F(QueryValueBuilderTest, ModifyListElementsTest) {
  builder_["value"] = "value1";
  builder_["list_value"].append(static_cast<int64_t>(100));
  builder_["list_value"].append(3.25);
  ASSERT_EQ(value_.kind_case(), QueryValue::KindCase::kSubqueryValue);
  ASSERT_THAT(value_.subquery_value(),
              EqualsProto(R"pb(fields {
                                 key: "list_value"
                                 value {
                                   list_value {
                                     values { int_value: 100 }
                                     values { double_value: 3.25 }
                                   }
                                 }
                               }
                               fields {
                                 key: "value"
                                 value { string_value: "value1" }
                               })pb"));
  builder_["list_value"].at(0) = static_cast<int64_t>(200);
  builder_["list_value"].at(1) = "value";
  ASSERT_THAT(value_.subquery_value(),
              EqualsProto(R"pb(fields {
                                 key: "list_value"
                                 value {
                                   list_value {
                                     values { int_value: 200 }
                                     values { string_value: "value" }
                                   }
                                 }
                               }
                               fields {
                                 key: "value"
                                 value { string_value: "value1" }
                               })pb"));
}

TEST(QueryResultDataBuilder, SmokeTest) {
  QueryResultData data;
  QueryResultDataBuilder builder(&data);
  builder["name"] = "dimm0";
  builder["speed"] = static_cast<int64_t>(3200);
  builder["metrics"]["uncorrectable"] = static_cast<int64_t>(10);
  builder["metrics"]["correctable"] = static_cast<int64_t>(10);
  builder["related_items"]["links"].append("link1");
  builder["related_items"]["links"].append("link2");

  Identifier id = ParseTextProtoOrDie(R"pb(local_devpath: "/phys/"
                                           machine_devpath: "/phys/PE0")pb");
  builder["identifier"] = std::move(id);
  ASSERT_THAT(data,
              EqualsProto(R"pb(fields {
                                 key: "identifier"
                                 value {
                                   identifier {
                                     local_devpath: "/phys/"
                                     machine_devpath: "/phys/PE0"
                                   }
                                 }
                               }
                               fields {
                                 key: "metrics"
                                 value {
                                   subquery_value {
                                     fields {
                                       key: "correctable"
                                       value { int_value: 10 }
                                     }
                                     fields {
                                       key: "uncorrectable"
                                       value { int_value: 10 }
                                     }
                                   }
                                 }
                               }
                               fields {
                                 key: "name"
                                 value { string_value: "dimm0" }
                               }
                               fields {
                                 key: "related_items"
                                 value {
                                   subquery_value {
                                     fields {
                                       key: "links"
                                       value {
                                         list_value {
                                           values { string_value: "link1" }
                                           values { string_value: "link2" }
                                         }
                                       }
                                     }
                                   }
                                 }
                               }
                               fields {
                                 key: "speed"
                                 value { int_value: 3200 }
                               })pb"));
}

TEST(QueryValueReaderTest, BooleanTest) {
  QueryValue data = ParseTextProtoOrDie(R"pb(bool_value: true)pb");
  QueryValueReader reader(&data);
  ASSERT_EQ(reader.kind(), QueryValue::KindCase::kBoolValue);
  ASSERT_TRUE(reader.bool_value());

  data = ParseTextProtoOrDie(R"pb(bool_value: false)pb");
  ASSERT_EQ(reader.kind(), QueryValue::KindCase::kBoolValue);
  ASSERT_FALSE(reader.bool_value());
}

TEST(QueryValueReaderTest, IntegerTest) {
  QueryValue data = ParseTextProtoOrDie(R"pb(int_value: 10)pb");
  QueryValueReader reader(&data);
  ASSERT_EQ(reader.kind(), QueryValue::KindCase::kIntValue);
  ASSERT_EQ(reader.int_value(), 10);
}

TEST(QueryValueReaderTest, DoubleTest) {
  QueryValue data = ParseTextProtoOrDie(R"pb(double_value: 3.14)pb");
  QueryValueReader reader(&data);
  ASSERT_EQ(reader.kind(), QueryValue::KindCase::kDoubleValue);
  ASSERT_DOUBLE_EQ(reader.double_value(), 3.14);
}

TEST(QueryValueReaderTest, TimestampTest) {
  QueryValue data =
      ParseTextProtoOrDie(R"pb(timestamp_value { seconds: 10 nanos: 1000 })pb");
  QueryValueReader reader(&data);
  ASSERT_EQ(reader.kind(), QueryValue::KindCase::kTimestampValue);
  ASSERT_THAT(reader.timestamp_value(), EqualsProto("seconds: 10 nanos: 1000"));
}

TEST(QueryValueReaderTest, StringTest) {
  QueryValue data = ParseTextProtoOrDie(R"pb(string_value: "testing")pb");
  QueryValueReader reader(&data);
  ASSERT_EQ(reader.kind(), QueryValue::KindCase::kStringValue);
  ASSERT_THAT(reader.string_value(), "testing");
}

TEST(QueryValueReaderTest, IdentifierTest) {
  QueryValue data = ParseTextProtoOrDie(R"pb(identifier {
                                               local_devpath: "/phys/"
                                               machine_devpath: "/phys/PE0"
                                             })pb");
  QueryValueReader reader(&data);
  ASSERT_EQ(reader.kind(), QueryValue::KindCase::kIdentifier);
  ASSERT_THAT(reader.identifier(),
              EqualsProto(R"pb(local_devpath: "/phys/"
                               machine_devpath: "/phys/PE0")pb"));
}

TEST(QueryValueReaderTest, ListTest) {
  QueryValue data = ParseTextProtoOrDie(R"pb(list_value {
                                               values { string_value: "value1" }
                                               values { int_value: 100 }
                                               values { double_value: 3.25 }
                                             })pb");
  QueryValueReader reader(&data);
  ASSERT_EQ(reader.kind(), QueryValue::KindCase::kListValue);
  ASSERT_EQ(reader.size(), 3);
  ASSERT_THAT(reader[0].kind(), QueryValue::KindCase::kStringValue);
  ASSERT_EQ(reader[0].string_value(), "value1");
  ASSERT_THAT(reader[1].kind(), QueryValue::KindCase::kIntValue);
  ASSERT_EQ(reader[1].int_value(), 100);
  ASSERT_THAT(reader[2].kind(), QueryValue::KindCase::kDoubleValue);
  ASSERT_DOUBLE_EQ(reader[2].double_value(), 3.25);
}

TEST(QueryValueReaderTest, ListRangeBasedLoopTest) {
  QueryValue data = ParseTextProtoOrDie(R"pb(list_value {
                                               values { string_value: "value1" }
                                               values { string_value: "value2" }
                                               values { string_value: "value3" }
                                             })pb");
  QueryValueReader reader(&data);

  absl::flat_hash_set<std::string> expected_values = {"value1", "value2",
                                                      "value3"};
  for (const QueryValue& list_item : reader.list_values()) {
    ASSERT_TRUE(expected_values.contains(list_item.string_value()));
    expected_values.erase(list_item.string_value());
  }
  ASSERT_THAT(expected_values, IsEmpty());
}

TEST(QueryValueReaderTest, ListOutOfBoundsCheck) {
  QueryValue data = ParseTextProtoOrDie(R"pb(list_value {
                                               values { string_value: "value1" }
                                               values { int_value: 100 }
                                             })pb");
  QueryValueReader reader(&data);
  ASSERT_EQ(reader.kind(), QueryValue::KindCase::kListValue);
  ASSERT_EQ(reader.size(), 2);
  ASSERT_DEATH(reader[2], "");
}

TEST(QueryValueReaderTest, SizeOnNonListDataType) {
  QueryValue data = ParseTextProtoOrDie(R"pb(string_value: "testing")pb");
  QueryValueReader reader(&data);
  ASSERT_EQ(reader.kind(), QueryValue::KindCase::kStringValue);
  ASSERT_DEATH(reader.size(), "");
}

TEST(QueryValueReaderTest, SubqueryValueTest) {
  QueryValue data =
      ParseTextProtoOrDie(R"pb(subquery_value {
                                 fields {
                                   key: "list_value"
                                   value {
                                     list_value {
                                       values { int_value: 100 }
                                       values { double_value: 3.25 }
                                     }
                                   }
                                 }
                                 fields {
                                   key: "value"
                                   value { string_value: "value1" }
                                 }
                               })pb");
  QueryValueReader reader(&data);
  ASSERT_EQ(reader.kind(), QueryValue::KindCase::kSubqueryValue);
  ASSERT_TRUE(reader.Has("list_value"));
  ASSERT_TRUE(reader.Has("value"));
  ASSERT_EQ(reader["value"].string_value(), "value1");

  ASSERT_THAT(reader.Get("NewTag"), IsStatusNotFound());
  absl::StatusOr<QueryValueReader> value_reader = reader.Get("list_value");
  ASSERT_THAT(value_reader, IsOk());
  ASSERT_EQ(value_reader->size(), 2);

  EXPECT_THAT(reader.field_keys(), UnorderedElementsAre("list_value", "value"));
}

TEST(QueryValueReaderTest, SubqueryGetValueTest) {
  QueryValue data = ParseTextProtoOrDie(R"pb(
    subquery_value {
      fields {
        key: "bool_val"
        value { bool_value: true }
      }
      fields {
        key: "double_val"
        value { double_value: 3.14 }
      }
      fields {
        key: "int_val"
        value { int_value: 42 }
      }
      fields {
        key: "str_val"
        value { string_value: "abcd" }
      }
      fields {
        key: "_id_"
        value { identifier { local_devpath: "/phys" } }
      }
    })pb");

  QueryValueReader reader(&data);
  ASSERT_THAT(reader.GetBoolValue("bool_val"), IsOkAndHolds(true));
  ASSERT_THAT(reader.GetBoolValue("bool_val_invalid"), IsStatusNotFound());

  ASSERT_THAT(reader.GetDoubleValue("double_val"), IsOkAndHolds(3.14));
  ASSERT_THAT(reader.GetDoubleValue("double_val_invalid"), IsStatusNotFound());

  ASSERT_THAT(reader.GetIntValue("int_val"), IsOkAndHolds(42));
  ASSERT_THAT(reader.GetIntValue("int_val_invalid"), IsStatusNotFound());

  ASSERT_THAT(reader.GetStringValue("str_val"), IsOkAndHolds("abcd"));
  ASSERT_THAT(reader.GetStringValue("str_val_invalid"), IsStatusNotFound());

  ASSERT_THAT(reader.GetIdentifier(),
              IsOkAndHolds(EqualsProto(R"pb(local_devpath: "/phys")pb")));
}

TEST(QueryValueReaderTest, SubqueryGetValueViaPropertyDefinition) {
  QueryValue data = ParseTextProtoOrDie(R"pb(
    subquery_value {
      fields {
        key: "bool_val"
        value { bool_value: true }
      }
      fields {
        key: "double_val"
        value { double_value: 3.14 }
      }
      fields {
        key: "int_val"
        value { int_value: 42 }
      }
      fields {
        key: "str_val"
        value { string_value: "abcd" }
      }
    })pb");

  QueryValueReader reader(&data);
  ASSERT_THAT(reader.GetValue<TestPropertyBool>(), IsOkAndHolds(true));
  ASSERT_THAT(reader.GetValue<TestPropertyBoolInvalid>(), IsStatusNotFound());

  ASSERT_THAT(reader.GetValue<TestPropertyDouble>(), IsOkAndHolds(3.14));
  ASSERT_THAT(reader.GetValue<TestPropertyDoubleInvalid>(), IsStatusNotFound());

  ASSERT_THAT(reader.GetValue<TestPropertyInt>(), IsOkAndHolds(42));
  ASSERT_THAT(reader.GetValue<TestPropertyIntInvalid>(), IsStatusNotFound());

  ASSERT_THAT(reader.GetValue<TestPropertyString>(), IsOkAndHolds("abcd"));
  ASSERT_THAT(reader.GetValue<TestPropertyStringInvalid>(), IsStatusNotFound());
}

TEST(QueryValueReaderTest, SubqueryGetWrongValueTypeTest) {
  QueryValue data = ParseTextProtoOrDie(R"pb(
    subquery_value {
      fields {
        key: "bool_val"
        value { bool_value: true }
      }
      fields {
        key: "double_val"
        value { double_value: 3.14 }
      }
      fields {
        key: "int_val"
        value { int_value: 42 }
      }
      fields {
        key: "str_val"
        value { string_value: "abcd" }
      }
    })pb");

  QueryValueReader reader(&data);
  ASSERT_THAT(reader.GetBoolValue("double_val"), IsStatusInvalidArgument());
  ASSERT_THAT(reader.GetBoolValue("int_val"), IsStatusInvalidArgument());
  ASSERT_THAT(reader.GetBoolValue("str_val"), IsStatusInvalidArgument());
  ASSERT_THAT(reader.GetStringValue("bool_val"), IsStatusInvalidArgument());
}

TEST(QueryResultDataReaderTest, SmokeTest) {
  QueryResultData data = ParseTextProtoOrDie(
      R"pb(fields {
             key: "Sensors"
             value {
               list_value {
                 values {
                   subquery_value {
                     fields {
                       key: "Name"
                       value { string_value: "indus_eat_temp" }
                     }
                     fields {
                       key: "Reading"
                       value { int_value: 28 }
                     }
                     fields {
                       key: "ReadingType"
                       value { string_value: "Temperature" }
                     }
                     fields {
                       key: "ReadingUnits"
                       value { string_value: "Cel" }
                     }
                   }
                 }
                 values {
                   subquery_value {
                     fields {
                       key: "Name"
                       value { string_value: "indus_latm_temp" }
                     }
                     fields {
                       key: "Reading"
                       value { int_value: 35 }
                     }
                     fields {
                       key: "ReadingType"
                       value { string_value: "Temperature" }
                     }
                     fields {
                       key: "ReadingUnits"
                       value { string_value: "Cel" }
                     }
                   }
                 }
                 values {
                   subquery_value {
                     fields {
                       key: "Name"
                       value { string_value: "CPU0" }
                     }
                     fields {
                       key: "Reading"
                       value { int_value: 60 }
                     }
                     fields {
                       key: "ReadingType"
                       value { string_value: "Temperature" }
                     }
                     fields {
                       key: "ReadingUnits"
                       value { string_value: "Cel" }
                     }
                   }
                 }
                 values {
                   subquery_value {
                     fields {
                       key: "Name"
                       value { string_value: "CPU1" }
                     }
                     fields {
                       key: "Reading"
                       value { int_value: 60 }
                     }
                     fields {
                       key: "ReadingType"
                       value { string_value: "Temperature" }
                     }
                     fields {
                       key: "ReadingUnits"
                       value { string_value: "Cel" }
                     }
                   }
                 }
               }
             }
           })pb");
  QueryResultDataReader reader(&data);

  static constexpr absl::string_view kSensorTag = "Sensors";
  static constexpr absl::string_view kNameTag = "Name";
  static constexpr absl::string_view kReadingTag = "Reading";

  ASSERT_TRUE(reader.Has(kSensorTag));
  ASSERT_EQ(reader[kSensorTag].kind(), QueryValue::KindCase::kListValue);
  ASSERT_EQ(reader[kSensorTag].size(), 4);

  for (const auto& item : reader[kSensorTag].list_values()) {
    QueryValueReader item_reader(&item);
    ASSERT_TRUE(item_reader.Has(kNameTag));
    ASSERT_TRUE(item_reader.Has(kReadingTag));

    ASSERT_TRUE(item_reader.Has("ReadingType"));
    ASSERT_EQ(item_reader["ReadingType"].string_value(), "Temperature");

    ASSERT_TRUE(item_reader.Has("ReadingUnits"));
    ASSERT_EQ(item_reader["ReadingUnits"].string_value(), "Cel");
  }

  ASSERT_EQ(reader[kSensorTag][0][kNameTag].string_value(), "indus_eat_temp");
  ASSERT_EQ(reader[kSensorTag][0][kReadingTag].int_value(), 28);

  ASSERT_EQ(reader[kSensorTag][1][kNameTag].string_value(), "indus_latm_temp");
  ASSERT_EQ(reader[kSensorTag][1][kReadingTag].int_value(), 35);

  ASSERT_EQ(reader[kSensorTag][2][kNameTag].string_value(), "CPU0");
  ASSERT_EQ(reader[kSensorTag][2][kReadingTag].int_value(), 60);

  ASSERT_EQ(reader[kSensorTag][3][kNameTag].string_value(), "CPU1");
  ASSERT_EQ(reader[kSensorTag][3][kReadingTag].int_value(), 60);

  ASSERT_THAT(reader.Get("NewTag"), IsStatusNotFound());
  absl::StatusOr<QueryValueReader> value_reader = reader.Get(kSensorTag);
  ASSERT_THAT(value_reader, IsOk());
  ASSERT_EQ(value_reader->size(), 4);
}

TEST(GetQueryResultByValue, SuccessTest) {
  QueryIdToResult result =
      ParseTextProtoOrDie(R"pb(results {
                                 key: "test"
                                 value {
                                   query_id: "test"
                                   data {
                                     fields {
                                       key: "name0"
                                       value { string_value: "value0" }
                                     }
                                   }
                                 }
                               })pb");

  ASSERT_THAT(GetQueryResult(std::move(result), "test"),
              IsOkAndHolds(EqualsProto(R"pb(query_id: "test"
                                            data {
                                              fields {
                                                key: "name0"
                                                value { string_value: "value0" }
                                              }
                                            }
              )pb")));
}

TEST(GetQueryResultByValue, EmptyResult) {
  ASSERT_THAT(GetQueryResult(QueryIdToResult(), "test"),
              IsStatusFailedPrecondition());
}

TEST(GetQueryResultByValue, QueryIdNotPresent) {
  QueryIdToResult result = ParseTextProtoOrDie(
      R"pb(results {
             key: "test"
             value {
               query_id: "test"
               data {
                 fields {
                   key: "name0"
                   value { string_value: "value0" }
                 }
               }
             }
           })pb");
  ASSERT_THAT(GetQueryResult(std::move(result), "query_id_not_set"),
              IsStatusNotFound());
}

TEST(GetQueryResultByValue, QueryHasErrors) {
  QueryIdToResult result = ParseTextProtoOrDie(R"pb(
    results {
      key: "test"
      value {
        query_id: "test"
        status { errors: "internal error" }
      }
    }
  )pb");
  ASSERT_THAT(GetQueryResult(std::move(result), "test"), IsStatusInternal());
}

TEST(QueryResultHasErrors, SuccessTest) {
  QueryResult result =
      ParseTextProtoOrDie(R"pb(query_id: "test"
                               data {
                                 fields {
                                   key: "name0"
                                   value { string_value: "value0" }
                                 }
                               })pb");
  ASSERT_FALSE(QueryResultHasErrors(result));
}

TEST(QueryResultHasErrors, FailureTest) {
  QueryResult result =
      ParseTextProtoOrDie(R"pb(query_id: "test"
                               status { errors: "internal error" })pb");
  ASSERT_TRUE(QueryResultHasErrors(result));
}

TEST(QueryOutputHasErrors, SuccessTest) {
  QueryIdToResult result =
      ParseTextProtoOrDie(R"pb(results {
                                 key: "test1"
                                 value {
                                   query_id: "test1"
                                   data {
                                     fields {
                                       key: "name1"
                                       value { string_value: "value1" }
                                     }
                                   }
                                 }
                               }
                               results {
                                 key: "test0"
                                 value {
                                   query_id: "test0"
                                   data {
                                     fields {
                                       key: "name0"
                                       value { string_value: "value0" }
                                     }
                                   }
                                 }
                               }
      )pb");
  ASSERT_FALSE(QueryOutputHasErrors(result));
}

TEST(QueryOutputHasErrors, FailureTest) {
  QueryIdToResult result =
      ParseTextProtoOrDie(R"pb(results {
                                 key: "test1"
                                 value {
                                   query_id: "test1"
                                   status { errors: "internal error" }
                                 }
                               }
                               results {
                                 key: "test0"
                                 value {
                                   query_id: "test0"
                                   data {
                                     fields {
                                       key: "name0"
                                       value { string_value: "value0" }
                                     }
                                   }
                                 }
                               }
      )pb");
  ASSERT_TRUE(QueryOutputHasErrors(result));
}

TEST(GetQueryDuration, SuccessTest) {
  ASSERT_THAT(GetQueryDuration(ParseTextProtoOrDie(
                  R"pb(query_id: "test"
                       stats {
                         start_time { seconds: 100 }
                         end_time { seconds: 200 }
                       })pb")),
              IsOkAndHolds(absl::Duration(absl::Seconds(100))));
}

TEST(GetQueryDuration, TimeStatsMissing) {
  ASSERT_THAT(GetQueryDuration(ParseTextProtoOrDie(R"pb(query_id: "test")pb")),
              IsStatusInternal());
}

TEST(GetQueryDuration, StartTimeMissing) {
  ASSERT_THAT(GetQueryDuration(ParseTextProtoOrDie(
                  R"pb(query_id: "test"
                       stats { end_time { seconds: 200 } })pb")),
              IsStatusInternal());
}

TEST(GetQueryDuration, EndTimeMissing) {
  ASSERT_THAT(GetQueryDuration(ParseTextProtoOrDie(
                  R"pb(query_id: "test"
                       stats { start_time { seconds: 100 } })pb")),
              IsStatusInternal());
}

TEST(GetQueryDuration, StartTimeAfterEndTime) {
  ASSERT_THAT(GetQueryDuration(ParseTextProtoOrDie(
                  R"pb(query_id: "test"
                       stats {
                         start_time { seconds: 200 }
                         end_time { seconds: 100 }
                       })pb")),
              IsStatusInternal());
}

TEST(RemoveDataForIdentifier, SuccessRemoveAtRoot) {
  QueryResult result = ParseTextProtoOrDie(R"pb(
    data {
      fields {
        key: "name0"
        value {
          subquery_value {
            fields {
              key: "_id_"
              value { identifier { local_devpath: "/phys" } }
            }
            fields {
              key: "name1"
              value { string_value: "value1" }
            }
          }
        }
      }
    }
  )pb");

  ASSERT_TRUE(RemoveDataForIdentifier(
      result, ParseTextProtoOrDie(R"pb(local_devpath: "/phys")pb")));
  EXPECT_THAT(result, EqualsProto(R"pb(data {
                                         fields {
                                           key: "name0"
                                           value {}
                                         }
                                       })pb"));
}

TEST(RemoveDataForIdentifier, SuccessRemoveFromMultipleSubquery) {
  QueryResult result = ParseTextProtoOrDie(
      R"pb(data {
             fields {
               key: "name0"
               value {
                 subquery_value {
                   fields {
                     key: "key0"
                     value { string_value: "test0" }
                   }
                   fields {
                     key: "_id_"
                     value { identifier { local_devpath: "/phys" } }
                   }
                 }
               }
             }
             fields {
               key: "name1"
               value {
                 subquery_value {
                   fields {
                     key: "key1"
                     value { string_value: "test1" }
                   }
                   fields {
                     key: "_id_"
                     value { identifier { local_devpath: "/phys" } }
                   }
                 }
               }
             }
             fields {
               key: "name2"
               value {
                 subquery_value {
                   fields {
                     key: "key2"
                     value { string_value: "test2" }
                   }
                   fields {
                     key: "_id_"
                     value { identifier { local_devpath: "/phys/PE0" } }
                   }
                 }
               }
             }

           }
      )pb");

  ASSERT_TRUE(RemoveDataForIdentifier(
      result, ParseTextProtoOrDie(R"pb(local_devpath: "/phys")pb")));
  EXPECT_THAT(
      result,
      EqualsProto(
          R"pb(data {
                 fields {
                   key: "name0"
                   value {}
                 }
                 fields {
                   key: "name1"
                   value {}
                 }
                 fields {
                   key: "name2"
                   value {
                     subquery_value {
                       fields {
                         key: "key2"
                         value { string_value: "test2" }
                       }
                       fields {
                         key: "_id_"
                         value { identifier { local_devpath: "/phys/PE0" } }
                       }
                     }
                   }
                 }
               })pb"));
}

TEST(RemoveDataForIdentifier, SuccessRemoveFromNestedSubquery) {
  QueryResult result = ParseTextProtoOrDie(
      R"pb(data {
             fields {
               key: "name0"
               value {
                 subquery_value {
                   fields {
                     key: "nested"
                     value {
                       subquery_value {
                         fields {
                           key: "_id_"
                           value { identifier { local_devpath: "/phys" } }
                         }
                         fields {
                           key: "key2"
                           value { string_value: "value2" }
                         }
                       }
                     }
                   }
                   fields {
                     key: "_id_"
                     value { identifier { local_devpath: "/phys" } }
                   }
                   fields {
                     key: "key1"
                     value { string_value: "test1" }
                   }
                 }
               }
             }
           }
      )pb");

  ASSERT_TRUE(RemoveDataForIdentifier(
      result, ParseTextProtoOrDie(R"pb(local_devpath: "/phys")pb")));
  EXPECT_THAT(result, EqualsProto(R"pb(data {
                                         fields {
                                           key: "name0"
                                           value {}
                                         }
                                       })pb"));
}

TEST(RemoveDataForIdentifier, SuccessRemoveFromListValue) {
  QueryResult result = ParseTextProtoOrDie(R"pb(
    data {
      fields {
        key: "list0"
        value {
          list_value {
            values {
              subquery_value {
                fields {
                  key: "key0"
                  value { string_value: "value0" }
                }
                fields {
                  key: "_id_"
                  value { identifier { local_devpath: "/phys" } }
                }
              }
            }
            values {
              subquery_value {
                fields {
                  key: "key1"
                  value { string_value: "value1" }
                }
                fields {
                  key: "_id_"
                  value { identifier { local_devpath: "/phys/PE0" } }
                }
              }
            }
          }
        }
      }
    })pb");

  ASSERT_TRUE(RemoveDataForIdentifier(
      result, ParseTextProtoOrDie(R"pb(local_devpath: "/phys")pb")));
  EXPECT_THAT(
      result,
      EqualsProto(
          R"pb(data {
                 fields {
                   key: "list0"
                   value {
                     list_value {
                       values {}
                       values {
                         subquery_value {
                           fields {
                             key: "_id_"
                             value { identifier { local_devpath: "/phys/PE0" } }
                           }
                           fields {
                             key: "key1"
                             value { string_value: "value1" }
                           }
                         }
                       }
                     }
                   }
                 }
               })pb"));
}

TEST(RemoveDataForIdentifier, SuccessRemoveFromMultipleListValues) {
  QueryResult result = ParseTextProtoOrDie(
      R"pb(data {
             fields {
               key: "list0"
               value {
                 list_value {
                   values {
                     subquery_value {
                       fields {
                         key: "key0"
                         value { string_value: "value0" }
                       }
                       fields {
                         key: "_id_"
                         value { identifier { local_devpath: "/phys" } }
                       }
                     }
                   }
                   values {
                     subquery_value {
                       fields {
                         key: "key1"
                         value { string_value: "value1" }
                       }
                       fields {
                         key: "_id_"
                         value { identifier { local_devpath: "/phys/PE0" } }
                       }
                     }
                   }
                 }
               }
             }
             fields {
               key: "list1"
               value {
                 list_value {
                   values {
                     subquery_value {
                       fields {
                         key: "key2"
                         value { string_value: "value0" }
                       }
                       fields {
                         key: "_id_"
                         value { identifier { local_devpath: "/phys" } }
                       }
                     }
                   }
                 }
               }
             }
           }
      )pb");

  ASSERT_TRUE(RemoveDataForIdentifier(
      result, ParseTextProtoOrDie(R"pb(local_devpath: "/phys")pb")));
  EXPECT_THAT(
      result,
      EqualsProto(
          R"pb(data {
                 fields {
                   key: "list0"
                   value {
                     list_value {
                       values {}
                       values {
                         subquery_value {
                           fields {
                             key: "_id_"
                             value { identifier { local_devpath: "/phys/PE0" } }
                           }
                           fields {
                             key: "key1"
                             value { string_value: "value1" }
                           }
                         }
                       }
                     }
                   }
                 }
                 fields {
                   key: "list1"
                   value { list_value { values {} } }
                 }
               })pb"));
}

TEST(RemoveDataForIdentifier, FailurePartialMatch) {
  QueryResult result = ParseTextProtoOrDie(R"pb(
    data {
      fields {
        key: "name0"
        value {
          subquery_value {
            fields {
              key: "_id_"
              value {
                identifier {
                  local_devpath: "/phys"
                  machine_devpath: "/phys/PE0"
                }
              }
            }
            fields {
              key: "name1"
              value { string_value: "value1" }
            }
          }
        }
      }
    }
  )pb");

  ASSERT_FALSE(RemoveDataForIdentifier(
      result, ParseTextProtoOrDie(R"pb(local_devpath: "/phys")pb")));
  EXPECT_THAT(result, EqualsProto(R"pb(
                data {
                  fields {
                    key: "name0"
                    value {
                      subquery_value {
                        fields {
                          key: "_id_"
                          value {
                            identifier {
                              local_devpath: "/phys"
                              machine_devpath: "/phys/PE0"
                            }
                          }
                        }
                        fields {
                          key: "name1"
                          value { string_value: "value1" }
                        }
                      }
                    }
                  }
                }
              )pb"));
}

TEST(RemoveDataForIdentifier, FailureNotFound) {
  QueryResult result = ParseTextProtoOrDie(R"pb(
    data {
      fields {
        key: "name0"
        value {
          subquery_value {
            fields {
              key: "_id_"
              value { identifier { local_devpath: "/phys" } }
            }
            fields {
              key: "name1"
              value { string_value: "value1" }
            }
          }
        }
      }
    }
  )pb");

  ASSERT_FALSE(RemoveDataForIdentifier(
      result, ParseTextProtoOrDie(R"pb(local_devpath: "/phys/FAN0")pb")));
  EXPECT_THAT(result, EqualsProto(R"pb(
                data {
                  fields {
                    key: "name0"
                    value {
                      subquery_value {
                        fields {
                          key: "_id_"
                          value { identifier { local_devpath: "/phys" } }
                        }
                        fields {
                          key: "name1"
                          value { string_value: "value1" }
                        }
                      }
                    }
                  }
                }
              )pb"));
}

TEST(GetDataForIdentifier, SuccessGetAtRoot) {
  QueryResult result = ParseTextProtoOrDie(R"pb(
    data {
      fields {
        key: "name0"
        value {
          subquery_value {
            fields {
              key: "_id_"
              value { identifier { local_devpath: "/phys" } }
            }
            fields {
              key: "name1"
              value { string_value: "value1" }
            }
          }
        }
      }
    }
  )pb");

  ECCLESIA_ASSIGN_OR_FAIL(
      std::vector<QueryResultData> data,
      GetDataForIdentifier(
          result, ParseTextProtoOrDie(R"pb(local_devpath: "/phys")pb")));
  EXPECT_THAT(data, ElementsAre(EqualsProto(R"pb(
                fields {
                  key: "_id_"
                  value { identifier { local_devpath: "/phys" } }
                }
                fields {
                  key: "name1"
                  value { string_value: "value1" }
                }
              )pb")));
}

TEST(GetDataForIdentifier, SuccessGetFromMultipleSubquery) {
  QueryResult result = ParseTextProtoOrDie(
      R"pb(data {
             fields {
               key: "name0"
               value {
                 subquery_value {
                   fields {
                     key: "key1"
                     value { string_value: "test1" }
                   }
                   fields {
                     key: "_id_"
                     value { identifier { local_devpath: "/phys" } }
                   }
                 }
               }
             }
             fields {
               key: "name1"
               value {
                 subquery_value {
                   fields {
                     key: "key0"
                     value { string_value: "test0" }
                   }
                   fields {
                     key: "_id_"
                     value { identifier { local_devpath: "/phys" } }
                   }
                 }
               }
             }
             fields {
               key: "name2"
               value {
                 subquery_value {
                   fields {
                     key: "key2"
                     value { string_value: "test2" }
                   }
                   fields {
                     key: "_id_"
                     value { identifier { local_devpath: "/phys/PE0" } }
                   }
                 }
               }
             }

           }
      )pb");

  ECCLESIA_ASSIGN_OR_FAIL(
      std::vector<QueryResultData> data,
      GetDataForIdentifier(
          result, ParseTextProtoOrDie(R"pb(local_devpath: "/phys")pb")));
  EXPECT_THAT(data, UnorderedElementsAre(
                        EqualsProto(R"pb(
                          fields {
                            key: "_id_"
                            value { identifier { local_devpath: "/phys" } }
                          }
                          fields {
                            key: "key0"
                            value { string_value: "test0" }
                          }
                        )pb"),
                        EqualsProto(R"pb(
                          fields {
                            key: "_id_"
                            value { identifier { local_devpath: "/phys" } }
                          }
                          fields {
                            key: "key1"
                            value { string_value: "test1" }
                          }
                        )pb")));
}

TEST(GetDataForIdentifier, SuccessGetFromNestedSubquery) {
  QueryResult result = ParseTextProtoOrDie(
      R"pb(data {
             fields {
               key: "name0"
               value {
                 subquery_value {
                   fields {
                     key: "nested"
                     value {
                       subquery_value {
                         fields {
                           key: "_id_"
                           value { identifier { local_devpath: "/phys" } }
                         }
                         fields {
                           key: "key2"
                           value { string_value: "value2" }
                         }
                       }
                     }
                   }
                   fields {
                     key: "_id_"
                     value { identifier { local_devpath: "/phys" } }
                   }
                   fields {
                     key: "key1"
                     value { string_value: "test1" }
                   }
                 }
               }
             }
           }
      )pb");

  ECCLESIA_ASSIGN_OR_FAIL(
      std::vector<QueryResultData> data,
      GetDataForIdentifier(
          result, ParseTextProtoOrDie(R"pb(local_devpath: "/phys")pb")));
  EXPECT_THAT(data, UnorderedElementsAre(EqualsProto(R"pb(
                fields {
                  key: "_id_"
                  value { identifier { local_devpath: "/phys" } }
                }
                fields {
                  key: "key1"
                  value { string_value: "test1" }
                }
                fields {
                  key: "nested"
                  value {
                    subquery_value {
                      fields {
                        key: "_id_"
                        value { identifier { local_devpath: "/phys" } }
                      }
                      fields {
                        key: "key2"
                        value { string_value: "value2" }
                      }
                    }
                  }
                }
              )pb")));
}

TEST(GetDataForIdentifier, SuccessGetFromListValue) {
  QueryResult result = ParseTextProtoOrDie(R"pb(
    data {
      fields {
        key: "list0"
        value {
          list_value {
            values {
              subquery_value {
                fields {
                  key: "key0"
                  value { string_value: "value0" }
                }
                fields {
                  key: "_id_"
                  value { identifier { local_devpath: "/phys" } }
                }
              }
            }
            values {
              subquery_value {
                fields {
                  key: "key1"
                  value { string_value: "value1" }
                }
                fields {
                  key: "_id_"
                  value { identifier { local_devpath: "/phys/PE0" } }
                }
              }
            }
          }
        }
      }
    })pb");

  ECCLESIA_ASSIGN_OR_FAIL(
      std::vector<QueryResultData> data,
      GetDataForIdentifier(
          result, ParseTextProtoOrDie(R"pb(local_devpath: "/phys")pb")));
  EXPECT_THAT(data, ElementsAre(EqualsProto(R"pb(
                fields {
                  key: "_id_"
                  value { identifier { local_devpath: "/phys" } }
                }
                fields {
                  key: "key0"
                  value { string_value: "value0" }
                }
              )pb")));
}

TEST(GetDataForIdentifier, SuccessGetFromMultipleListValues) {
  QueryResult result = ParseTextProtoOrDie(
      R"pb(data {
             fields {
               key: "list0"
               value {
                 list_value {
                   values {
                     subquery_value {
                       fields {
                         key: "key0"
                         value { string_value: "value0" }
                       }
                       fields {
                         key: "_id_"
                         value { identifier { local_devpath: "/phys" } }
                       }
                     }
                   }
                   values {
                     subquery_value {
                       fields {
                         key: "key1"
                         value { string_value: "value1" }
                       }
                       fields {
                         key: "_id_"
                         value { identifier { local_devpath: "/phys/PE0" } }
                       }
                     }
                   }
                 }
               }
             }
             fields {
               key: "list1"
               value {
                 list_value {
                   values {
                     subquery_value {
                       fields {
                         key: "key2"
                         value { string_value: "value0" }
                       }
                       fields {
                         key: "_id_"
                         value { identifier { local_devpath: "/phys" } }
                       }
                     }
                   }
                 }
               }
             }
           }
      )pb");

  ECCLESIA_ASSIGN_OR_FAIL(
      std::vector<QueryResultData> data,
      GetDataForIdentifier(
          result, ParseTextProtoOrDie(R"pb(local_devpath: "/phys")pb")));
  EXPECT_THAT(data, UnorderedElementsAre(
                        EqualsProto(R"pb(
                          fields {
                            key: "_id_"
                            value { identifier { local_devpath: "/phys" } }
                          }
                          fields {
                            key: "key0"
                            value { string_value: "value0" }
                          }
                        )pb"),
                        EqualsProto(R"pb(
                          fields {
                            key: "_id_"
                            value { identifier { local_devpath: "/phys" } }
                          }
                          fields {
                            key: "key2"
                            value { string_value: "value0" }
                          }
                        )pb")));
}

TEST(GetDataForIdentifier, FailurePartialMatch) {
  QueryResult result = ParseTextProtoOrDie(R"pb(
    data {
      fields {
        key: "name0"
        value {
          subquery_value {
            fields {
              key: "_id_"
              value {
                identifier {
                  local_devpath: "/phys"
                  machine_devpath: "/phys/PE0"
                }
              }
            }
            fields {
              key: "name1"
              value { string_value: "value1" }
            }
          }
        }
      }
    }
  )pb");

  EXPECT_THAT(GetDataForIdentifier(
                  result, ParseTextProtoOrDie(R"pb(local_devpath: "/phys")pb")),
              IsStatusNotFound());
}

TEST(GetDataForIdentifier, FailureNotFound) {
  QueryResult result = ParseTextProtoOrDie(R"pb(
    data {
      fields {
        key: "name0"
        value {
          subquery_value {
            fields {
              key: "_id_"
              value { identifier { local_devpath: "/phys" } }
            }
            fields {
              key: "name1"
              value { string_value: "value1" }
            }
          }
        }
      }
    }
  )pb");

  EXPECT_THAT(
      GetDataForIdentifier(
          result, ParseTextProtoOrDie(R"pb(local_devpath: "/phys/FAN0")pb")),
      IsStatusNotFound());
}

}  // namespace
}  // namespace ecclesia
