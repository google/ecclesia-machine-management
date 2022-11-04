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

#include "ecclesia/lib/redfish/redfish_override/transport_with_override.h"

#include <memory>
#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/redfish/redfish_override/rf_override.pb.h"
#include "ecclesia/lib/redfish/testing/grpc_dynamic_mockup_server.h"
#include "ecclesia/lib/redfish/transport/grpc_tls_options.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/redfish/transport/mocked_interface.h"
#include "ecclesia/lib/testing/proto.h"
#include "ecclesia/lib/testing/status.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {
namespace {
using ::testing::Eq;
using ::testing::Return;

class RedfishOverrideTest : public ::testing::Test {
 protected:
  void SetUp() override {
    transport_ = std::make_unique<RedfishTransportMock>();
    transport_mock_ptr_ = transport_.get();
    expected_result1_ = nlohmann::json::parse(R"json({
    "TestString": "test123",
    "TestNumber": 123.0,
    "TestBool": true,
    "TestArray":[
      {
        "TestStruct": "tests0"
      },
      {
        "TestStruct": "tests1"
      },
      {
        "TestStruct": "tests2"
      },
      {
        "TestNumber": 1234.0 
      },
      "TestArrayString",
      [ "TestArrayInArray1" ]
    ]})json",
                                              nullptr, false);
    expected_result2_ = nlohmann::json::parse(R"json({
    "TestString": "test456",
    "TestNumber": 456.0,
    "TestBool": false,
    "TestArray":[
      56789.0,
      {
        "TestStruct":"tests0"
      },
      {
        "TestStruct":"tests1"
      },
      {
        "TestStruct":"tests2"
      },
      {
        "TestNumber": 4567.0 
      },
      "TestArrayString2",
      [ "TestArrayInArray2" ]
    ]})json",
                                              nullptr, false);
    absl::string_view uri1 = "/expected/result/1";
    absl::string_view uri2 = "/expected/result/2";
    absl::string_view uri1_expand = "/expected/result/1?$expand=.($levels=1)";
    EXPECT_CALL(*transport_mock_ptr_, Get(uri1))
        .WillRepeatedly(Return(
            RedfishTransport::Result{.code = 200, .body = expected_result1_}));
    EXPECT_CALL(*transport_mock_ptr_, Get(uri2))
        .WillRepeatedly(Return(
            RedfishTransport::Result{.code = 200, .body = expected_result2_}));
    EXPECT_CALL(*transport_mock_ptr_, Get(uri1_expand))
        .WillRepeatedly(Return(
            RedfishTransport::Result{.code = 200, .body = expected_result1_}));
  }
  std::unique_ptr<RedfishTransportMock> transport_;
  RedfishTransportMock *transport_mock_ptr_;
  nlohmann::json expected_result1_;
  nlohmann::json expected_result2_;
};

TEST_F(RedfishOverrideTest, LoadPolicyTest) {
  OverridePolicy policy = LoadOverridePolicy(
      GetTestDataDependencyPath(
          "lib/redfish/redfish_override/test_selector.binarypb"),
      transport_.get());
  OverridePolicy expected_policy = ParseTextProtoOrDie(R"pb(
    override_content_map_uri: {
      key: "/expected/result/1"
      value: {
        override_field:
        [ {
          action_replace: {
            object_identifier: {
              individual_object_identifier:
              [ { field_name: "TestString" }]
            }
            override_value: {
              value: { string_value: "OverrideReplaceByField" }
            }
          }
        }
          , {
            action_replace: {
              object_identifier: {
                individual_object_identifier:
                [ { field_name: "TestArray" }
                  , {
                    array_field: {
                      field_name: "TestNumber"
                      value: { number_value: 1234 }
                    }
                  }
                  , { field_name: "TestNumber" }]
              }
              override_value: { value: { number_value: 54321 } }
            }
          }
          , {
            action_replace: {
              object_identifier: {
                individual_object_identifier:
                [ { field_name: "TestArray" }
                  , { array_idx: 0 }
                  , { field_name: "TestStruct" }]
              }
              override_value: {
                value: { string_value: "OverrideReplaceByIndex" }
              }
            }
          }]
      }
    }
  )pb");
  EXPECT_THAT(policy, EqualsProto(expected_policy));
}

TEST_F(RedfishOverrideTest, LoadPolicyInvalidFileTest) {
  OverridePolicy policy =
      LoadOverridePolicy(GetTestDataDependencyPath(
                             "lib/redfish/redfish_override/non_exist.binarypb"),
                         transport_.get());
  OverridePolicy expected_policy = ParseTextProtoOrDie(R"pb()pb");
  EXPECT_THAT(policy, EqualsProto(expected_policy));
}

TEST_F(RedfishOverrideTest, GetOverridePolicyByFileTest) {
  OverridePolicy policy = GetOverridePolicy(
      GetTestDataDependencyPath(
          "lib/redfish/redfish_override/test_policy.binarypb"));
  OverridePolicy expected_policy = ParseTextProtoOrDie(R"pb(
    override_content_map_uri: {
      key: "/expected/result/1"
      value: {
        override_field:
        [ {
          action_replace: {
            object_identifier: {
              individual_object_identifier:
              [ { field_name: "TestString" }]
            }
            override_value: {
              value: { string_value: "OverrideReplaceByField" }
            }
          }
        }
          , {
            action_replace: {
              object_identifier: {
                individual_object_identifier:
                [ { field_name: "TestArray" }
                  , {
                    array_field: {
                      field_name: "TestNumber"
                      value: { number_value: 1234 }
                    }
                  }
                  , { field_name: "TestNumber" }]
              }
              override_value: { value: { number_value: 54321 } }
            }
          }
          , {
            action_replace: {
              object_identifier: {
                individual_object_identifier:
                [ { field_name: "TestArray" }
                  , { array_idx: 0 }
                  , { field_name: "TestStruct" }]
              }
              override_value: {
                value: { string_value: "OverrideReplaceByIndex" }
              }
            }
          }]
      }
    }
  )pb");
  EXPECT_THAT(policy, EqualsProto(expected_policy));
}

TEST_F(RedfishOverrideTest, GetOverridePolicyByFileFailedTest) {
  EXPECT_THAT(GetOverridePolicy(GetTestDataDependencyPath(
                  "lib/redfish/redfish_override/do_not_exist.binarypb")),
              EqualsProto(OverridePolicy::default_instance()));
}

TEST_F(RedfishOverrideTest, GetOverridePolicyByServerTest) {
  GrpcDynamicMockupServer mockup_server("barebones_session_auth/mockup.shar",
                                        "localhost", 0);
  auto port = mockup_server.Port();
  ASSERT_THAT(port.has_value(), true);
  std::string policy_str =
      R"pb(override_content_map_uri: {
             key: "/expected/result/1"
             value: {
               override_field:
               [ {
                 action_replace: {
                   object_identifier: {
                     individual_object_identifier:
                     [ { field_name: "TestString" }]
                   }
                   override_value: {
                     value: { string_value: "OverrideReplaceByField" }
                   }
                 }
               }]
             }
           })pb";
  mockup_server.AddOverridePolicy(policy_str);
  StaticBufferBasedTlsOptions options;
  OverridePolicy get_policy = GetOverridePolicy(
      "localhost", port.value(), options.GetChannelCredentials());
  OverridePolicy expected_policy = ParseTextProtoOrDie(policy_str);
  EXPECT_THAT(get_policy, EqualsProto(expected_policy));
}

TEST_F(RedfishOverrideTest, SelectorConstructorTest) {
  auto rf_override = std::make_unique<RedfishTransportWithOverride>(
      std::move(transport_),
      GetTestDataDependencyPath(
          "lib/redfish/redfish_override/test_selector.binarypb"));
  absl::string_view expected_get_str = R"json({
    "TestString": "OverrideReplaceByField",
    "TestNumber": 123.0,
    "TestBool": true,
    "TestArray":[
      {"TestStruct":"OverrideReplaceByIndex"},
      {
        "TestStruct":"tests1"
      },
      {
        "TestStruct": "tests2"
      },
      {
        "TestNumber": 54321.0
      },
      "TestArrayString",
      [ "TestArrayInArray1" ]
    ]})json";
  nlohmann::json expected_get =
      nlohmann::json::parse(expected_get_str, nullptr, false);
  absl::StatusOr<RedfishTransport::Result> res_get =
      rf_override->Get("/expected/result/1");
  ASSERT_THAT(res_get, IsOk());
  ASSERT_TRUE(std::holds_alternative<nlohmann::json>(res_get->body));
  EXPECT_THAT(std::get<nlohmann::json>(res_get->body), Eq(expected_get));
  EXPECT_THAT(res_get->code, Eq(200));
}

TEST_F(RedfishOverrideTest, SelectorConstructorInvalidFileTest) {
  auto rf_override = std::make_unique<RedfishTransportWithOverride>(
      std::move(transport_),
      GetTestDataDependencyPath(
          "lib/redfish/redfish_override/non_exist.binarypb"));
  absl::string_view expected_get_str = R"json({
    "TestString": "test123",
    "TestNumber": 123.0,
    "TestBool": true,
    "TestArray":[
      {
        "TestStruct": "tests0"
      },
      {
        "TestStruct": "tests1"
      },
      {
        "TestStruct": "tests2"
      },
      {
        "TestNumber": 1234.0
      },
      "TestArrayString",
      [ "TestArrayInArray1" ]
    ]})json";
  nlohmann::json expected_get =
      nlohmann::json::parse(expected_get_str, nullptr, false);
  absl::StatusOr<RedfishTransport::Result> res_get =
      rf_override->Get("/expected/result/1");
  ASSERT_THAT(res_get, IsOk());
  ASSERT_TRUE(std::holds_alternative<nlohmann::json>(res_get->body));
  EXPECT_THAT(std::get<nlohmann::json>(res_get->body), Eq(expected_get));
  EXPECT_THAT(res_get->code, Eq(200));
}

TEST_F(RedfishOverrideTest, GetReplaceValue) {
  OverridePolicy policy = ParseTextProtoOrDie(R"pb(
    override_content_map_uri: {
      key: "/expected/result/1"
      value: {
        override_field:
        [ {
          action_replace: {
            object_identifier: {
              individual_object_identifier:
              [ { field_name: "TestString" }]
            }
            override_value: {
              value: { string_value: "OverrideReplaceByField" }
            }
          }
        }
          , {
            action_replace: {
              object_identifier: {
                individual_object_identifier:
                [ { field_name: "TestArray" }
                  , {
                    array_field: {
                      field_name: "TestNumber"
                      value: { number_value: 1234 }
                    }
                  }
                  , { field_name: "TestNumber" }]
              }
              override_value: { value: { number_value: 54321 } }
            }
          }
          , {
            action_replace: {
              object_identifier: {
                individual_object_identifier:
                [ { field_name: "TestArray" }
                  , { array_idx: 0 }
                  , { field_name: "TestStruct" }]
              }
              override_value: {
                value: { string_value: "OverrideReplaceByIndex" }
              }
            }
          }]
      }
    }
  )pb");
  auto rf_override = std::make_unique<RedfishTransportWithOverride>(
      std::move(transport_), policy);

  absl::string_view expected_get_str = R"json({
    "TestString": "OverrideReplaceByField",
    "TestNumber": 123.0,
    "TestBool": true,
    "TestArray":[
      {"TestStruct":"OverrideReplaceByIndex"},
      {
        "TestStruct":"tests1"
      },
      {
        "TestStruct": "tests2"
      },
      {
        "TestNumber": 54321.0
      },
      "TestArrayString",
      [ "TestArrayInArray1" ]
    ]})json";
  nlohmann::json expected_get =
      nlohmann::json::parse(expected_get_str, nullptr, false);
  absl::StatusOr<RedfishTransport::Result> res_get =
      rf_override->Get("/expected/result/1");
  ASSERT_THAT(res_get, IsOk());
  ASSERT_TRUE(std::holds_alternative<nlohmann::json>(res_get->body));
  EXPECT_THAT(std::get<nlohmann::json>(res_get->body), Eq(expected_get));
  EXPECT_THAT(res_get->code, Eq(200));
}

TEST_F(RedfishOverrideTest, GetExpandReplaceValue) {
  OverridePolicy policy = ParseTextProtoOrDie(R"pb(
    override_content_map_uri: {
      key: "/expected/result/1"
      value: {
        override_field:
        [ {
          action_replace: {
            object_identifier: {
              individual_object_identifier:
              [ { field_name: "TestString" }]
            }
            override_value: {
              value: { string_value: "OverrideReplaceByField" }
            }
          }
        }
          , {
            action_replace: {
              object_identifier: {
                individual_object_identifier:
                [ { field_name: "TestArray" }
                  , {
                    array_field: {
                      field_name: "TestNumber"
                      value: { number_value: 1234 }
                    }
                  }
                  , { field_name: "TestNumber" }]
              }
              override_value: { value: { number_value: 54321 } }
            }
          }
          , {
            action_replace: {
              object_identifier: {
                individual_object_identifier:
                [ { field_name: "TestArray" }
                  , { array_idx: 0 }
                  , { field_name: "TestStruct" }]
              }
              override_value: {
                value: { string_value: "OverrideReplaceByIndex" }
              }
            }
          }]
      }
    }
  )pb");
  auto rf_override = std::make_unique<RedfishTransportWithOverride>(
      std::move(transport_), policy);

  absl::string_view expected_get_str = R"json({
    "TestString": "OverrideReplaceByField",
    "TestNumber": 123.0,
    "TestBool": true,
    "TestArray":[
      {"TestStruct":"OverrideReplaceByIndex"},
      {
        "TestStruct":"tests1"
      },
      {
        "TestStruct": "tests2"
      },
      {
        "TestNumber": 54321.0
      },
      "TestArrayString",
      [ "TestArrayInArray1" ]
    ]})json";
  nlohmann::json expected_get =
      nlohmann::json::parse(expected_get_str, nullptr, false);
  absl::StatusOr<RedfishTransport::Result> res_get =
      rf_override->Get("/expected/result/1?$expand=.($levels=1)");
  ASSERT_THAT(res_get, IsOk());
  ASSERT_TRUE(std::holds_alternative<nlohmann::json>(res_get->body));
  EXPECT_THAT(std::get<nlohmann::json>(res_get->body), Eq(expected_get));
  EXPECT_THAT(res_get->code, Eq(200));
}

TEST_F(RedfishOverrideTest, GetReplaceValueRegex) {
  OverridePolicy policy = ParseTextProtoOrDie(R"pb(
    override_content_map_regex: {
      key: "/expected/(.*)/1"
      value: {
        override_field:
        [ {
          action_replace: {
            object_identifier: {
              individual_object_identifier:
              [ { field_name: "TestString" }]
            }
            override_value: {
              value: { string_value: "OverrideReplaceByField" }
            }
          }
        }]
      }
    }
  )pb");
  auto rf_override = std::make_unique<RedfishTransportWithOverride>(
      std::move(transport_), policy);

  absl::string_view expected_get_str = R"json({
    "TestString": "OverrideReplaceByField",
    "TestNumber": 123.0,
    "TestBool": true,
    "TestArray":[
      {
        "TestStruct": "tests0"
      },
      {
        "TestStruct":"tests1"
      },
      {
        "TestStruct": "tests2"
      },
      {
        "TestNumber": 1234.0
      },
      "TestArrayString",
      [ "TestArrayInArray1" ]

    ]})json";
  nlohmann::json expected_get =
      nlohmann::json::parse(expected_get_str, nullptr, false);
  absl::StatusOr<RedfishTransport::Result> res_get =
      rf_override->Get("/expected/result/1");
  ASSERT_THAT(res_get, IsOk());
  ASSERT_TRUE(std::holds_alternative<nlohmann::json>(res_get->body));
  EXPECT_THAT(std::get<nlohmann::json>(res_get->body), Eq(expected_get));
  EXPECT_THAT(res_get->code, Eq(200));
}

TEST_F(RedfishOverrideTest, GetAddValue) {
  OverridePolicy policy = ParseTextProtoOrDie(R"pb(
    override_content_map_uri: {
      key: "/expected/result/1"
      value: {
        override_field:
        [ {
          action_add: {
            object_identifier: {
              individual_object_identifier:
              [ { field_name: "AddField" }]
            }
            override_value: { value: { string_value: "OverrideAdd" } }
          }
        }
          , {
            action_add: {
              object_identifier: {
                individual_object_identifier:
                [ { field_name: "TestArray" }]
              }
              override_value: {
                value: {
                  struct_value: {
                    fields {
                      key: "TestAddArrayString"
                      value: { string_value: "tests1" }
                    }
                    fields {
                      key: "TestAddArrayNumber"
                      value: { number_value: 98765.0 }
                    }
                  }
                }
              }
            }
          }
          , {
            action_add: {
              object_identifier: {
                individual_object_identifier:
                [ { field_name: "TestArray" }
                  , { array_idx: 1 }
                  , { field_name: "AddField" }]
              }
              override_value: { value: { string_value: "AddValue" } }
            }
          }
          , {
            action_add: {
              object_identifier: {
                individual_object_identifier:
                [ { field_name: "TestArray" }
                  , {
                    array_field: {
                      field_name: "TestStruct"
                      value: { string_value: "tests2" }
                    }
                  }
                  , { field_name: "AddField" }]
              }
              override_value: { value: { string_value: "AddValue" } }
            }
          }
          , {
            action_add: {
              object_identifier: {
                individual_object_identifier:
                [ { field_name: "TestArray" }
                  , { array_idx: 5 }]
              }
              override_value: { value: { string_value: "AddValue" } }
            }
          }]
      }
    }
  )pb");
  auto rf_override = std::make_unique<RedfishTransportWithOverride>(
      std::move(transport_), policy);
  absl::string_view expected_get_str = R"json({
    "TestArray":[
      {
        "TestStruct": "tests0"
      },
      {
        "AddField":"AddValue",
        "TestStruct":"tests1"
      },
      {
        "TestStruct": "tests2",
        "AddField":"AddValue"
      },
      {

        "TestNumber":1234.0
      },
      "TestArrayString",
      ["TestArrayInArray1", "AddValue"],
      {
        "TestAddArrayNumber":98765.0,
        "TestAddArrayString":"tests1"
      }
    ],
    "TestBool":true,
    "TestNumber":123.0,
    "TestString":"test123",
    "AddField":"OverrideAdd"
  })json";
  nlohmann::json expected_get =
      nlohmann::json::parse(expected_get_str, nullptr, false);
  absl::StatusOr<RedfishTransport::Result> res_get =
      rf_override->Get("/expected/result/1");
  ASSERT_THAT(res_get, IsOk());
  ASSERT_TRUE(std::holds_alternative<nlohmann::json>(res_get->body));
  EXPECT_THAT(std::get<nlohmann::json>(res_get->body), Eq(expected_get));
  EXPECT_THAT(res_get->code, Eq(200));
}

TEST_F(RedfishOverrideTest, GetClear) {
  OverridePolicy policy = ParseTextProtoOrDie(R"pb(
    override_content_map_uri: {
      key: "/expected/result/2"
      value: {
        override_field:
        [ {
          action_clear: {
            object_identifier: {
              individual_object_identifier:
              [ { field_name: "TestString" }]
            }
          }
        }
          , {
            action_clear: {
              object_identifier: {
                individual_object_identifier:
                [ { field_name: "TestArray" }
                  , {
                    array_field: {
                      field_name: "TestStruct"
                      value: { string_value: "tests2" }
                    }
                  }]
              }
            }
          }
          , {
            action_clear: {
              object_identifier: {
                individual_object_identifier:
                [ { field_name: "TestArray" }
                  , { array_idx: 0 }]
              }
            }
          }]
      }
    }
  )pb");
  auto rf_override = std::make_unique<RedfishTransportWithOverride>(
      std::move(transport_), policy);
  absl::string_view expected_get_str = R"json({
    "TestArray":[
      {"TestStruct":"tests0"},
      {"TestStruct":"tests1"},
      {"TestNumber":4567.0},
      "TestArrayString2",
      ["TestArrayInArray2"]
    ],
    "TestBool":false,
    "TestNumber":456.0
  })json";
  nlohmann::json expected_get =
      nlohmann::json::parse(expected_get_str, nullptr, false);
  absl::StatusOr<RedfishTransport::Result> res_get =
      rf_override->Get("/expected/result/2");
  ASSERT_THAT(res_get, IsOk());
  ASSERT_TRUE(std::holds_alternative<nlohmann::json>(res_get->body));
  EXPECT_THAT(std::get<nlohmann::json>(res_get->body), Eq(expected_get));
  EXPECT_THAT(res_get->code, Eq(200));
}
TEST_F(RedfishOverrideTest, ReplaceTypeFail) {
  OverridePolicy replace_policy = ParseTextProtoOrDie(R"pb(
    override_content_map_uri: {
      key: "/expected/result/1"
      value: {
        override_field:
        [ {
          action_replace: {
            object_identifier: {
              individual_object_identifier:
              [ { field_name: "TestString" }]
            }
            override_value: { value: { number_value: 1.0 } }
          }
        }]
      }
    }
  )pb");
  auto rf_override = std::make_unique<RedfishTransportWithOverride>(
      std::move(transport_), replace_policy);

  absl::StatusOr<RedfishTransport::Result> res_get =
      rf_override->Get("/expected/result/1");
  ASSERT_THAT(res_get, IsOk());
}

TEST_F(RedfishOverrideTest, AddArrayFail) {
  OverridePolicy policy = ParseTextProtoOrDie(R"pb(
    override_content_map_uri: {
      key: "/expected/result/1"
      value: {
        override_field:
        [ {
          action_add: {
            object_identifier: {
              individual_object_identifier:
              [ { field_name: "TestArray" }
                , { array_idx: 0 }]
            }
            override_value: { value: { string_value: "test" } }
          }
        }]
      }
    }
  )pb");
  auto rf_override = std::make_unique<RedfishTransportWithOverride>(
      std::move(transport_), policy);

  absl::StatusOr<RedfishTransport::Result> res_get =
      rf_override->Get("/expected/result/1");
  EXPECT_THAT(res_get, IsOk());
}

TEST_F(RedfishOverrideTest, EmptyIdentifierFail) {
  OverridePolicy policy = ParseTextProtoOrDie(R"pb(
    override_content_map_uri: {
      key: "/expected/result/1"
      value: {
        override_field:
        [ {
          action_add: {
            object_identifier: {
              individual_object_identifier:
              [ {}]
            }
            override_value: { value: { string_value: "test" } }
          }
        }]
      }
    }
  )pb");
  auto rf_override = std::make_unique<RedfishTransportWithOverride>(
      std::move(transport_), policy);

  absl::StatusOr<RedfishTransport::Result> res_get =
      rf_override->Get("/expected/result/1");
  EXPECT_THAT(res_get, IsOk());
}
}  // namespace
}  // namespace ecclesia
