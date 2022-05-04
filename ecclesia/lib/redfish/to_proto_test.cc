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

#include "ecclesia/lib/redfish/to_proto.h"

#include <memory>
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "ecclesia/lib/redfish/testing/json_mockup.h"
#include "ecclesia/lib/redfish/to_proto_test.pb.h"
#include "ecclesia/lib/testing/proto.h"

namespace ecclesia {
namespace {

using ::ecclesia::EqualsProto;

TEST(RedfishToProto, Baseline) {
  auto json_intf = NewJsonMockupInterface(R"json(
    {
      "a1": 1.5,
      "a2": 2.5,
      "a3": 3,
      "a4": 4,
      "a5": 5,
      "a6": 6,
      "a7": true,
      "a8": "a8",
      "a9": "A92",
      "b": {
        "b1": 1.25,
        "b2": 2.25,
        "b3": 3,
        "b4": 4,
        "b5": 5,
        "b6": 6,
        "b7": true,
        "b8": "b8"
      }
    }
  )json");

  auto root_obj = json_intf->GetRoot().AsObject();
  ASSERT_TRUE(root_obj);

  RedfishToProtoTestDataA data;
  ASSERT_TRUE(RedfishObjToProto(*root_obj, &data).ok());

  ASSERT_THAT(
      data, EqualsProto(R"pb(
        a1: 1.5
        a2: 2.5
        a3: 3
        a4: 4
        a5: 5
        a6: 6
        a7: true
        a8: "a8"
        a9: A92
        b: { b1: 1.25 b2: 2.25 b3: 3 b4: 4 b5: 5 b6: 6 b7: true b8: "b8" }
      )pb"));
}

TEST(RedfishToProto, Repeated) {
  auto json_intf = NewJsonMockupInterface(R"json(
    {
      "a1": 1.5,
      "a2": 2.5,
      "a3": 3,
      "a4": 4,
      "a5": 5,
      "a6": 6,
      "a6repeated": [7,8,9],
      "a7": true,
      "a8": "a8",
      "a9": "A92",
      "a9repeated": ["A92", "A91"],
      "b": {
        "b1": 1.25,
        "b2": 2.25,
        "b3": 3,
        "b4": 4,
        "b5": 5,
        "b6": 6,
        "b6repeated": [17,18,19],
        "b7": true,
        "b8": "b8"
      }
    }
  )json");

  auto root_obj = json_intf->GetRoot().AsObject();
  ASSERT_TRUE(root_obj);

  RedfishToProtoTestDataA data;
  ASSERT_TRUE(RedfishObjToProto(*root_obj, &data).ok());

  ASSERT_THAT(data, EqualsProto(R"pb(
                a1: 1.5
                a2: 2.5
                a3: 3
                a4: 4
                a5: 5
                a6: 6
                a6repeated: 7
                a6repeated: 8
                a6repeated: 9
                a7: true
                a8: "a8"
                a9: A92
                a9repeated: A92
                a9repeated: A91
                b: {
                  b1: 1.25
                  b2: 2.25
                  b3: 3
                  b4: 4
                  b5: 5
                  b6: 6
                  b6repeated: 17
                  b6repeated: 18
                  b6repeated: 19
                  b7: true
                  b8: "b8"
                }
              )pb"));
}

TEST(RedfishToProto, Empty) {
  auto json_intf = NewJsonMockupInterface("{}");
  auto root_obj = json_intf->GetRoot().AsObject();
  ASSERT_TRUE(root_obj);

  RedfishToProtoTestDataA data;
  ASSERT_TRUE(RedfishObjToProto(*root_obj, &data).ok());

  ASSERT_THAT(data, EqualsProto(""));
}

TEST(RedfishToProto, EmptyRepeated) {
  auto json_intf = NewJsonMockupInterface(R"json(
    {
      "a6repeated": []
    }
  )json");
  auto root_obj = json_intf->GetRoot().AsObject();
  ASSERT_TRUE(root_obj);

  RedfishToProtoTestDataA data;
  ASSERT_TRUE(RedfishObjToProto(*root_obj, &data).ok());

  ASSERT_THAT(data, EqualsProto(""));
}

TEST(RedfishToProto, RedfishExtraField) {
  auto json_intf = NewJsonMockupInterface(R"json(
    {
      "something_else": "abc"
    }
  )json");
  auto root_obj = json_intf->GetRoot().AsObject();
  ASSERT_TRUE(root_obj);

  RedfishToProtoTestDataA data;
  ASSERT_TRUE(RedfishObjToProto(*root_obj, &data).ok());

  ASSERT_THAT(data, EqualsProto(""));
}

TEST(RedfishToProto, NullMessage) {
  auto json_intf = NewJsonMockupInterface("{}");
  auto root_obj = json_intf->GetRoot().AsObject();
  ASSERT_TRUE(root_obj);
  auto status = RedfishObjToProto(*root_obj, nullptr);
  ASSERT_EQ(status.code(), absl::StatusCode::kInternal);
}

}  // namespace
}  // namespace ecclesia
