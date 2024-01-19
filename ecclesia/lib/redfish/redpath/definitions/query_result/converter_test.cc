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

#include "ecclesia/lib/redfish/redpath/definitions/query_result/converter.h"

#include <cstdint>
#include <string>

#include "google/protobuf/timestamp.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.pb.h"
#include "ecclesia/lib/testing/proto.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {
namespace {

absl::Status Validate(const google::protobuf::Timestamp& t) {
  const auto sec = t.seconds();
  const auto ns = t.nanos();
  if (sec < -62135596800 || sec > 253402300799) {
    return absl::InvalidArgumentError(absl::StrCat("seconds=", sec));
  }
  if (ns < 0 || ns > 999999999) {
    return absl::InvalidArgumentError(absl::StrCat("nanos=", ns));
  }
  return absl::OkStatus();
}
absl::Status EncodeGoogleApiProto(absl::Time t,
                                  google::protobuf::Timestamp* proto) {
  const int64_t s = absl::ToUnixSeconds(t);
  proto->set_seconds(s);
  proto->set_nanos(static_cast<int32_t>((t - absl::FromUnixSeconds(s)) /
                                        absl::Nanoseconds(1)));
  return Validate(*proto);
}

absl::StatusOr<google::protobuf::Timestamp> EncodeGoogleApiProto(absl::Time t) {
  google::protobuf::Timestamp proto;
  absl::Status status = EncodeGoogleApiProto(t, &proto);
  if (!status.ok()) return status;
  return proto;
}

TEST(QueryResultDataConverterTest, ConvertLegacyToQueryResult) {
  ecclesia::DelliciusQueryResult legacy_result = ParseTextProtoOrDie(R"pb(
    query_id: "ServiceRoot"
    subquery_output_by_id {
      key: "RedfishVersion"
      value {
        data_sets {
          properties { name: "Uri" string_value: "/redfish/v1" }
          properties { name: "RedfishSoftwareVersion" string_value: "1.6.1" }
          child_subquery_output_by_id {
            key: "ChassisLinked"
            value {
              data_sets {
                properties {
                  name: "serial_number"
                  string_value: "MBBQTW194106556"
                }
                properties { name: "part_number" string_value: "1043652-02" }
              }
            }
          }
        }
      }
    }
    subquery_output_by_id {
      key: "ChassisNotLinked"
      value {
        data_sets {
          properties { name: "serial_number" string_value: "MBBQTW194106556" }
          properties { name: "part_number" string_value: "1043652-02" }
        }
      }
    }
    start_timestamp { seconds: 10 }
    end_timestamp { seconds: 10 }
  )pb");

  QueryResult result = ToQueryResult(legacy_result);
  ASSERT_THAT(
      result,
      EqualsProto(
          R"pb(query_id: "ServiceRoot"
               stats: {
                 start_time { seconds: 10 }
                 end_time { seconds: 10 }
               }
               data {
                 fields {
                   key: "ChassisNotLinked"
                   value {
                     list_value {
                       values {
                         subquery_value {
                           fields {
                             key: "part_number"
                             value { string_value: "1043652-02" }
                           }
                           fields {
                             key: "serial_number"
                             value { string_value: "MBBQTW194106556" }
                           }
                         }
                       }
                     }
                   }
                 }
                 fields {
                   key: "RedfishVersion"
                   value {
                     list_value {
                       values {
                         subquery_value {
                           fields {
                             key: "ChassisLinked"
                             value {
                               list_value {
                                 values {
                                   subquery_value {
                                     fields {
                                       key: "part_number"
                                       value { string_value: "1043652-02" }
                                     }
                                     fields {
                                       key: "serial_number"
                                       value { string_value: "MBBQTW194106556" }
                                     }
                                   }
                                 }
                               }
                             }
                           }
                           fields {
                             key: "RedfishSoftwareVersion"
                             value { string_value: "1.6.1" }
                           }
                           fields {
                             key: "Uri"
                             value { string_value: "/redfish/v1" }
                           }
                         }
                       }
                     }
                   }
                 }
               })pb"));

  ASSERT_THAT(QueryResultDataToJson(result.data()),
              nlohmann::json::parse(R"json(
  {
    "ChassisNotLinked": [
      {
        "part_number": "1043652-02",
        "serial_number": "MBBQTW194106556"
      }
    ],
    "RedfishVersion": [
      {
        "ChassisLinked": [
          {
            "part_number": "1043652-02",
            "serial_number": "MBBQTW194106556"
          }
        ],
        "RedfishSoftwareVersion": "1.6.1",
        "Uri": "/redfish/v1"
      }
    ]
  })json"));
}

TEST(QueryResultDataConverterTest, ConvertJsonToQueryResult) {
  ASSERT_THAT(JsonToQueryResultData(nlohmann::json::parse(R"json(
    {
      "Sensors": [
        {
          "Name": "CPU0",
          "Reading": 60,
          "ReadingType": "Temperature",
          "ReadingUnits": "Cel"
        },
        {
          "Name": "CPU1",
          "Reading": 60.123,
          "ReadingType": "Temperature",
          "ReadingUnits": "Cel"
        }
      ]
    }
  )json")),
              EqualsProto(R"pb(
                fields {
                  key: "Sensors"
                  value {
                    list_value {
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
                            value { double_value: 60.123 }
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
                }
              )pb"));
}

TEST(QueryResultDataConverterTest, PartialDataVerification) {
  ecclesia::DelliciusQueryResult legacy_result = ParseTextProtoOrDie(
      R"pb(query_id: "SensorCollectorTemplate"
           subquery_output_by_id {
             key: "Sensors"
             value {
               data_sets {
                 devpath: "/phys/KA0"
                 properties { name: "Name" string_value: "indus_eat_temp" }
                 properties { name: "ReadingType" string_value: "Temperature" }
                 properties { name: "ReadingUnits" string_value: "Cel" }
                 properties { name: "Reading" int64_value: 28 }
                 decorators { machine_devpath: "/phys/KA0" }
               }
               data_sets {
                 devpath: "/phys/KA2"
                 properties { name: "Name" string_value: "indus_latm_temp" }
                 properties { name: "ReadingType" string_value: "Temperature" }
                 properties { name: "ReadingUnits" string_value: "Cel" }
                 properties { name: "Reading" int64_value: 35 }
                 decorators { machine_devpath: "/phys/KA2" }
               }
               data_sets {
                 devpath: "/phys/CPU0"
                 properties { name: "Name" string_value: "CPU0" }
                 properties { name: "ReadingType" string_value: "Temperature" }
                 properties { name: "ReadingUnits" string_value: "Cel" }
                 properties { name: "Reading" int64_value: 60 }
                 decorators { machine_devpath: "/phys/CPU0" }
               }
               data_sets {
                 devpath: "/phys/CPU1"
                 properties { name: "Name" string_value: "CPU1" }
                 properties { name: "ReadingType" string_value: "Temperature" }
                 properties { name: "ReadingUnits" string_value: "Cel" }
                 properties { name: "Reading" double_value: 60.25 }
                 decorators { machine_devpath: "/phys/CPU1" }
               }
             }
           })pb");

  QueryResult result = ToQueryResult(legacy_result);
  ASSERT_THAT(result,
              EqualsProto(
                  R"pb(query_id: "SensorCollectorTemplate"
                       data {
                         fields {
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
                                   fields {
                                     key: "_id_"
                                     value {
                                       identifier {
                                         local_devpath: "/phys/KA0"
                                         machine_devpath: "/phys/KA0"
                                       }
                                     }
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
                                   fields {
                                     key: "_id_"
                                     value {
                                       identifier {
                                         local_devpath: "/phys/KA2"
                                         machine_devpath: "/phys/KA2"
                                       }
                                     }
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
                                   fields {
                                     key: "_id_"
                                     value {
                                       identifier {
                                         local_devpath: "/phys/CPU0"
                                         machine_devpath: "/phys/CPU0"
                                       }
                                     }
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
                                     value { double_value: 60.25 }
                                   }
                                   fields {
                                     key: "ReadingType"
                                     value { string_value: "Temperature" }
                                   }
                                   fields {
                                     key: "ReadingUnits"
                                     value { string_value: "Cel" }
                                   }
                                   fields {
                                     key: "_id_"
                                     value {
                                       identifier {
                                         local_devpath: "/phys/CPU1"
                                         machine_devpath: "/phys/CPU1"
                                       }
                                     }
                                   }
                                 }
                               }
                             }
                           }
                         }
                       })pb"));

  ASSERT_THAT(result.data(),
              EqualsProto(JsonToQueryResultData(nlohmann::json::parse(R"json(
    {
    "Sensors": [
      {
        "Name": "indus_eat_temp",
        "Reading": 28,
        "ReadingType": "Temperature",
        "ReadingUnits": "Cel",
        "_id_": {
          "_local_devpath_": "/phys/KA0",
          "_machine_devpath_": "/phys/KA0"
        }
      },
      {
        "Name": "indus_latm_temp",
        "Reading": 35,
        "ReadingType": "Temperature",
        "ReadingUnits": "Cel",
        "_id_": {
          "_local_devpath_": "/phys/KA2",
          "_machine_devpath_": "/phys/KA2"
        }
      },
      {
        "Name": "CPU0",
        "Reading": 60,
        "ReadingType": "Temperature",
        "ReadingUnits": "Cel",
        "_id_": {
          "_local_devpath_": "/phys/CPU0",
          "_machine_devpath_": "/phys/CPU0"
        }
      },
      {
        "Name": "CPU1",
        "Reading": 60.25,
        "ReadingType": "Temperature",
        "ReadingUnits": "Cel",
        "_id_": {
          "_local_devpath_": "/phys/CPU1",
          "_machine_devpath_": "/phys/CPU1"
        }
      }
    ]
  }
  )json"))));
}

TEST(QueryResultDataConverterTest, ConvertLegacyResultWithArrayProperty) {
  ecclesia::DelliciusQueryResult legacy_result = ParseTextProtoOrDie(
      R"pb(query_id: "PCIErrorLogSingleEntry"
           subquery_output_by_id {
             key: "PCIErrorLog"
             value {
               data_sets {
                 properties { name: "DiagnosticDataType" string_value: "CPER" }
                 properties {
                   name: "MessageArgs"
                   collection_value {
                     values: { string_value: "0000:16:01.2" }
                     values: { string_value: "CE" }
                   }
                 }
               }
             }
           })pb");

  QueryResult result = ToQueryResult(legacy_result);
  ASSERT_THAT(result,
              EqualsProto(
                  R"pb(query_id: "PCIErrorLogSingleEntry"
                       data {
                         fields {
                           key: "PCIErrorLog"
                           value {
                             list_value {
                               values {
                                 subquery_value {
                                   fields {
                                     key: "DiagnosticDataType"
                                     value { string_value: "CPER" }
                                   }
                                   fields {
                                     key: "MessageArgs"
                                     value {
                                       list_value {
                                         values { string_value: "0000:16:01.2" }
                                         values { string_value: "CE" }
                                       }
                                     }
                                   }
                                 }
                               }
                             }
                           }
                         }
                       })pb"));
}
TEST(QueryResultDataConverterTest, ConvertLegacyResultWithErrors) {
  ecclesia::DelliciusQueryResult legacy_result = ParseTextProtoOrDie(
      R"pb(query_id: "PCIErrorLogSingleEntry"
           status { code: 4 message: "Deadline Exceeded" })pb");

  QueryResult result = ToQueryResult(legacy_result);
  ASSERT_THAT(result, EqualsProto(
                          R"pb(query_id: "PCIErrorLogSingleEntry"
                               status {
                                 errors: "Deadline Exceeded"
                                 error_code: ERROR_NETWORK
                               }
                               data {}
                          )pb"));
}

TEST(QueryResultDataConverterTest, DataVerificationWithMetrics) {
  ecclesia::DelliciusQueryResult legacy_result = ParseTextProtoOrDie(
      R"pb(
        query_id: "AssemblyCollectorWithPropertyNameNormalization"
        subquery_output_by_id {
          key: "Chassis"
          value {
            data_sets {
              devpath: "/phys"
              properties {
                name: "serial_number"
                string_value: "MBBQTW194106556"
              }
              properties { name: "part_number" string_value: "1043652-02" }
            }
          }
        }
        subquery_output_by_id {
          key: "Processors"
          value {
            data_sets { devpath: "/phys/CPU0" }
            data_sets { devpath: "/phys/CPU1" }
          }
        }
        redfish_metrics {
          uri_to_metrics_map {
            key: "/redfish/v1"
            value {
              request_type_to_metadata {
                key: "GET"
                value { request_count: 1 }
              }
            }
          }
          uri_to_metrics_map {
            key: "/redfish/v1/Chassis"
            value {
              request_type_to_metadata {
                key: "GET"
                value { request_count: 1 }
              }
            }
          }
          uri_to_metrics_map {
            key: "/redfish/v1/Chassis/chassis"
            value {
              request_type_to_metadata {
                key: "GET"
                value { request_count: 1 }
              }
            }
          }
          uri_to_metrics_map {
            key: "/redfish/v1/Systems"
            value {
              request_type_to_metadata {
                key: "GET"
                value { request_count: 1 }
              }
            }
          }
          uri_to_metrics_map {
            key: "/redfish/v1/Systems/system"
            value {
              request_type_to_metadata {
                key: "GET"
                value { request_count: 1 }
              }
            }
          }
          uri_to_metrics_map {
            key: "/redfish/v1/Systems/system/Processors"
            value {
              request_type_to_metadata {
                key: "GET"
                value { request_count: 1 }
              }
            }
          }
          uri_to_metrics_map {
            key: "/redfish/v1/Systems/system/Processors/0"
            value {
              request_type_to_metadata {
                key: "GET"
                value { request_count: 1 }
              }
            }
          }
          uri_to_metrics_map {
            key: "/redfish/v1/Systems/system/Processors/1"
            value {
              request_type_to_metadata {
                key: "GET"
                value { request_count: 1 }
              }
            }
          }
        }
      )pb");

  QueryResult result = ToQueryResult(legacy_result);
  Statistics expected_stats = ParseTextProtoOrDie(R"pb(
    redfish_metrics {
      uri_to_metrics_map {
        key: "/redfish/v1"
        value {
          request_type_to_metadata {
            key: "GET"
            value { request_count: 1 }
          }
        }
      }
      uri_to_metrics_map {
        key: "/redfish/v1/Chassis"
        value {
          request_type_to_metadata {
            key: "GET"
            value { request_count: 1 }
          }
        }
      }
      uri_to_metrics_map {
        key: "/redfish/v1/Chassis/chassis"
        value {
          request_type_to_metadata {
            key: "GET"
            value { request_count: 1 }
          }
        }
      }
      uri_to_metrics_map {
        key: "/redfish/v1/Systems"
        value {
          request_type_to_metadata {
            key: "GET"
            value { request_count: 1 }
          }
        }
      }
      uri_to_metrics_map {
        key: "/redfish/v1/Systems/system"
        value {
          request_type_to_metadata {
            key: "GET"
            value { request_count: 1 }
          }
        }
      }
      uri_to_metrics_map {
        key: "/redfish/v1/Systems/system/Processors"
        value {
          request_type_to_metadata {
            key: "GET"
            value { request_count: 1 }
          }
        }
      }
      uri_to_metrics_map {
        key: "/redfish/v1/Systems/system/Processors/0"
        value {
          request_type_to_metadata {
            key: "GET"
            value { request_count: 1 }
          }
        }
      }
      uri_to_metrics_map {
        key: "/redfish/v1/Systems/system/Processors/1"
        value {
          request_type_to_metadata {
            key: "GET"
            value { request_count: 1 }
          }
        }
      }
    }
  )pb");
  EXPECT_THAT(result.stats(), EqualsProto(expected_stats));
}
TEST(ConvertToJsonTest, IntegerTest) {
  QueryValue value;
  value.set_int_value(12345);
  ASSERT_EQ(ValueToJson(value), nlohmann::json::parse(R"json(12345)json"));
}

TEST(ConvertToJsonTest, DoubleTest) {
  QueryValue value;
  value.set_double_value(4.3543);
  ASSERT_EQ(ValueToJson(value), nlohmann::json::parse(R"json(4.3543)json"));
}

TEST(ConvertToJsonTest, StringTest) {
  QueryValue value;
  value.set_string_value("testing");
  ASSERT_EQ(ValueToJson(value), nlohmann::json::parse(R"json("testing")json"));
}

TEST(ConvertToJsonTest, BooleanTest) {
  QueryValue value;
  value.set_bool_value(true);
  ASSERT_EQ(ValueToJson(value), nlohmann::json::parse(R"json(true)json"));

  value.set_bool_value(false);
  ASSERT_EQ(ValueToJson(value), nlohmann::json::parse(R"json(false)json"));
}

TEST(ConvertToJsonTest, TimestampTest) {
  absl::Time expected_time = absl::FromUnixSeconds(1694462400);
  std::string formatted_time = absl::FormatTime(expected_time);
  nlohmann::json expected_time_json = formatted_time;
  absl::ParseTime("%Y-%m-%dT%H:%M:%S%Ez", formatted_time, &expected_time,
                  nullptr);
  absl::StatusOr<google::protobuf::Timestamp> expected_timestamp =
                       EncodeGoogleApiProto(expected_time);
  if (!expected_timestamp.ok()) {
    ADD_FAILURE() << expected_timestamp.status();
  }
  QueryValue value;
  value.mutable_timestamp_value()->set_seconds(expected_timestamp->seconds());
  value.mutable_timestamp_value()->set_nanos(expected_timestamp->nanos());
  ASSERT_EQ(ValueToJson(value), expected_time_json);
}

TEST(ConvertToJsonTest, IdentifierTest) {
  QueryValue value = ParseTextProtoOrDie(
      R"pb(identifier {
             local_devpath: "/phys/"
             machine_devpath: "/phys/PE0",
             embedded_location_context: "embedded_dev/sub_fru"
           })pb");
  ASSERT_EQ(ValueToJson(value), nlohmann::json::parse(R"json({
    "_local_devpath_": "/phys/",
    "_machine_devpath_": "/phys/PE0",
    "_embedded_location_context_": "embedded_dev/sub_fru"
  })json"));

  Identifier id = ParseTextProtoOrDie(R"pb(local_devpath: "/phys/"
                                           machine_devpath: "/phys/PE0",
                                           embedded_location_context: "")pb");
  ASSERT_EQ(IdentifierValueToJson(id), nlohmann::json::parse(R"json({
    "_local_devpath_": "/phys/",
    "_machine_devpath_": "/phys/PE0"
  })json"));
}

TEST(ConvertToJsonTest, SubqueryValueTest) {
  QueryValue value =
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
  ASSERT_EQ(ValueToJson(value), nlohmann::json::parse(R"json(
  {
    "list_value": [
      100,
      3.25
    ],
    "value": "value1"
  })json"));
}

TEST(ConvertToJsonTest, ListValueTest) {
  QueryValue value =
      ParseTextProtoOrDie(R"pb(list_value {
                                 values { string_value: "value1" }
                                 values { int_value: 100 }
                                 values { double_value: 3.25 }
                               })pb");
  ASSERT_EQ(ValueToJson(value),
            nlohmann::json::parse(R"json(["value1", 100, 3.25])json"));

  ListValue list_value =
      ParseTextProtoOrDie(R"pb(values { string_value: "value1" }
                               values { int_value: 100 }
                               values { double_value: 3.25 }
      )pb");
  ASSERT_EQ(ListValueToJson(list_value),
            nlohmann::json::parse(R"json(["value1", 100, 3.25])json"));
}

TEST(JsonToValueTest, IntegerTest) {
  nlohmann::json json = nlohmann::json::parse(R"json(12345)json");
  ASSERT_THAT(JsonToQueryValue(json), EqualsProto(R"pb(int_value: 12345)pb"));
}

TEST(JsonToValueTest, DoubleTest) {
  nlohmann::json json = nlohmann::json::parse(R"json(4.3543)json");
  ASSERT_THAT(JsonToQueryValue(json), EqualsProto(R"pb(double_value: 4.3543)pb"));
}

TEST(JsonToValueTest, StringTest) {
  nlohmann::json json = nlohmann::json::parse(R"json("testing")json");
  ASSERT_THAT(JsonToQueryValue(json), EqualsProto(R"pb(string_value: "testing")pb"));
}

TEST(JsonToValueTest, BooleanTest) {
  nlohmann::json json = nlohmann::json::parse(R"json(true)json");
  ASSERT_THAT(JsonToQueryValue(json), EqualsProto(R"pb(bool_value: true)pb"));

  json = nlohmann::json::parse(R"json(false)json");
  ASSERT_THAT(JsonToQueryValue(json), EqualsProto(R"pb(bool_value: false)pb"));
}

TEST(JsonToValueTest, TimestampTest) {
  nlohmann::json json =
      nlohmann::json::parse(R"json("2023-09-11T13:00:00-07:00")json");
  ASSERT_THAT(
      JsonToQueryValue(json),
      EqualsProto(R"pb(timestamp_value { seconds: 1694462400 nanos: 0 })pb"));

  json = nlohmann::json::parse(R"json("2023-09-11T13:00:00")json");
  ASSERT_THAT(
      JsonToQueryValue(json),
      EqualsProto(R"pb(timestamp_value { seconds: 1694437200 nanos: 0 })pb"));

  json = nlohmann::json::parse(R"json("2023-09-11")json");
  ASSERT_THAT(JsonToQueryValue(json),
              EqualsProto(R"pb(string_value: "2023-09-11")pb"));
}

TEST(JsonToValueTest, IdentifierTest) {
  nlohmann::json json = nlohmann::json::parse(R"json({
    "_local_devpath_": "/phys/",
    "_machine_devpath_": "/phys/PE0"
  })json");
  ASSERT_THAT(JsonToQueryValue(json), EqualsProto(R"pb(identifier {
                                                    local_devpath: "/phys/"
                                                    machine_devpath: "/phys/PE0"
                                                  })pb"));

  ASSERT_THAT(JsonToIdentifierValue(json).value(), EqualsProto(R"pb(
                local_devpath: "/phys/"
                machine_devpath: "/phys/PE0"
              )pb"));
}

TEST(JsonToValueTest, SubqueryValueTest) {
  nlohmann::json json = nlohmann::json::parse(R"json(
  {
    "list_value": [
      100,
      3.25
    ],
    "value": "value1"
  })json");
  ASSERT_THAT(JsonToQueryValue(json),
              EqualsProto(R"pb(subquery_value {
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
                               })pb"));
}

TEST(JsonToValueTest, ListValueTest) {
  nlohmann::json json =
      nlohmann::json::parse(R"json(["value1", 100, 3.25])json");
  ASSERT_THAT(JsonToQueryValue(json),
              EqualsProto(R"pb(list_value {
                                 values { string_value: "value1" }
                                 values { int_value: 100 }
                                 values { double_value: 3.25 }
                               })pb"));

  ASSERT_THAT(JsonToQueryListValue(json), EqualsProto(R"pb(
                values { string_value: "value1" }
                values { int_value: 100 }
                values { double_value: 3.25 }
              )pb"));
}

TEST(JsonToValueTest, RawDataInQueryResultDataTest) {
  ecclesia::DelliciusQueryResult legacy_raw_data_result = ParseTextProtoOrDie(
      R"pb(
        query_id: "FaultLogsExpand"
        subquery_output_by_id {
          key: "ExpandUri"
          value {
            data_sets {
              properties { name: "Name" string_value: "Host CPER Log" }
              child_subquery_output_by_id {
                key: "ExpandLogUriToBytes"
                value {
                  data_sets {
                    raw_data {
                      bytes_value: "CPER\001\001\377\377\377\377\001\000\002\000\000\000\002\000\000\000X\001\000\000\000#\010\000\021\004# \264\005]\341\001\316\352\001\000\020\336\277$\224&g\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\254\325\251\t\004R\024B\226\345\224\231.u+\315\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\310\000\000\000\220\000\000\000\000\001\000\000\000\000\000\000\026=\236\341\021\274\344\021\234\252\302\005\035]F\260\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\002\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\t\000\000\000\001\000\000\000\220\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000h\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\004\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000A\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
                    }
                  }
                }
              }
              child_subquery_output_by_id {
                key: "ExpandLogUriToString"
                value {
                  data_sets {
                    raw_data {
                      string_value: "Q1BFUgEB/////wEAAgAAAAIAAABYAQAAACMIABEEIyC0BV3hAc7qAQAQ3r8klCZnAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACs1akJBFIUQpbllJkudSvNAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAADIAAAAkAAAAAABAAAAAAAAFj2e4RG85BGcqsIFHV1GsAAAAAAAAAAAAAAAAAAAAAACAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAJAAAAAQAAAJAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAGgAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABBAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="
                    }
                  }
                }
              }
            }
          }
        }
      )pb");

  QueryResult result = ToQueryResult(legacy_raw_data_result);
  ASSERT_THAT(
      result,
      EqualsProto(
          R"pb(
            query_id: "FaultLogsExpand"
            data {
              fields {
                key: "ExpandUri"
                value {
                  list_value {
                    values {
                      subquery_value {
                        fields {
                          key: "ExpandLogUriToBytes"
                          value {
                            raw_data {
                              raw_bytes_value: "CPER\001\001\377\377\377\377\001\000\002\000\000\000\002\000\000\000X\001\000\000\000#\010\000\021\004# \264\005]\341\001\316\352\001\000\020\336\277$\224&g\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\254\325\251\t\004R\024B\226\345\224\231.u+\315\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\310\000\000\000\220\000\000\000\000\001\000\000\000\000\000\000\026=\236\341\021\274\344\021\234\252\302\005\035]F\260\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\002\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\t\000\000\000\001\000\000\000\220\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000h\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\004\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000A\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
                            }
                          }
                        }
                        fields {
                          key: "ExpandLogUriToString"
                          value {
                            raw_data {
                              raw_string_value: "Q1BFUgEB/////wEAAgAAAAIAAABYAQAAACMIABEEIyC0BV3hAc7qAQAQ3r8klCZnAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACs1akJBFIUQpbllJkudSvNAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAADIAAAAkAAAAAABAAAAAAAAFj2e4RG85BGcqsIFHV1GsAAAAAAAAAAAAAAAAAAAAAACAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAJAAAAAQAAAJAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAGgAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABBAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="
                            }
                          }
                        }
                        fields {
                          key: "Name"
                          value { string_value: "Host CPER Log" }
                        }
                      }
                    }
                  }
                }
              }
            })pb"));
}

TEST(JsonToValueTest, QueryResultWithRawDataToJson) {
  QueryResult raw_data_result = ParseTextProtoOrDie(
      R"pb(
        query_id: "FaultLogsExpand"
        data {
          fields {
            key: "ExpandUri"
            value {
              list_value {
                values {
                  subquery_value {
                    fields {
                      key: "ExpandLogUriToBytes"
                      value {
                        raw_data {
                          raw_bytes_value: "CPER\001\001\377\377\377\377\001\000\002\000\000\000\002\000\000\000X\001\000\000\000#\010\000\021\004# \264\005]\341\001\316\352\001\000\020\336\277$\224&g\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\254\325\251\t\004R\024B\226\345\224\231.u+\315\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\310\000\000\000\220\000\000\000\000\001\000\000\000\000\000\000\026=\236\341\021\274\344\021\234\252\302\005\035]F\260\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\002\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\t\000\000\000\001\000\000\000\220\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000h\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\004\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000A\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
                        }
                      }
                    }
                    fields {
                      key: "ExpandLogUriToString"
                      value {
                        raw_data {
                          raw_string_value: "Q1BFUgEB/////wEAAgAAAAIAAABYAQAAACMIABEEIyC0BV3hAc7qAQAQ3r8klCZnAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACs1akJBFIUQpbllJkudSvNAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAADIAAAAkAAAAAABAAAAAAAAFj2e4RG85BGcqsIFHV1GsAAAAAAAAAAAAAAAAAAAAAACAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAJAAAAAQAAAJAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAGgAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABBAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="
                        }
                      }
                    }
                    fields {
                      key: "Name"
                      value { string_value: "Host CPER Log" }
                    }
                  }
                }
              }
            }
          }
        })pb");
  ASSERT_THAT(ValueToJson(raw_data_result.data().fields().at("ExpandUri")),
              testing::Eq(nlohmann::json::parse(
                  R"json(
          [
            {
              "ExpandLogUriToBytes":"Q1BFUgEB/////wEAAgAAAAIAAABYAQAAACMIABEEIyC0BV3hAc7qAQAQ3r8klCZnAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACs1akJBFIUQpbllJkudSvNAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAADIAAAAkAAAAAABAAAAAAAAFj2e4RG85BGcqsIFHV1GsAAAAAAAAAAAAAAAAAAAAAACAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAJAAAAAQAAAJAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAGgAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABBAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=",
              "ExpandLogUriToString":"Q1BFUgEB/////wEAAgAAAAIAAABYAQAAACMIABEEIyC0BV3hAc7qAQAQ3r8klCZnAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACs1akJBFIUQpbllJkudSvNAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAADIAAAAkAAAAAABAAAAAAAAFj2e4RG85BGcqsIFHV1GsAAAAAAAAAAAAAAAAAAAAAACAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAJAAAAAQAAAJAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAGgAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABBAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=",
              "Name":"Host CPER Log"
            }
          ]
          )json")));
}

}  // namespace
}  // namespace ecclesia
