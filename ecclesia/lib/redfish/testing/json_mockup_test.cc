/*
 * Copyright 2020 Google LLC
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

#include "ecclesia/lib/redfish/testing/json_mockup.h"

#include <assert.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ecclesia/lib/redfish/interface.h"

namespace libredfish {
namespace {

using ::testing::Eq;
using ::testing::UnorderedElementsAre;

TEST(JsonMockup, InvalidJsonDies) {
  EXPECT_DEATH(NewJsonMockupInterface("{"), "Could not load JSON.");
}

TEST(JsonMockup, CanGetFields) {
  auto json_intf = NewJsonMockupInterface(R"json(
    {
      "StringField": "string value",
      "IntField": 2147483647,
      "Int64Field": 9223372036854775807,
      "DoubleField": 95.3,
      "BoolFieldTrue": true,
      "BoolFieldFalse": false
    }
  )json");
  auto root_obj = json_intf->GetRoot().AsObject();
  ASSERT_TRUE(root_obj) << "Root object should not be null.";

  // get value INT_MAX should success
  auto int_val = root_obj->GetNodeValue<int>("IntField");
  ASSERT_TRUE(int_val.has_value());
  EXPECT_THAT(int_val.value(), Eq(2147483647));

  // get value INT64_MAX should success
  auto int64_val = root_obj->GetNodeValue<int64_t>("Int64Field");
  ASSERT_TRUE(int64_val.has_value());
  EXPECT_THAT(int64_val.value(), Eq(9223372036854775807));

  auto double_val = root_obj->GetNodeValue<double>("DoubleField");
  ASSERT_TRUE(double_val.has_value());
  EXPECT_THAT(double_val.value(), Eq(95.3));

  auto str_val = root_obj->GetNodeValue<std::string>("StringField");
  ASSERT_TRUE(str_val.has_value());
  EXPECT_THAT(str_val.value(), Eq("string value"));

  auto true_val = root_obj->GetNodeValue<bool>("BoolFieldTrue");
  ASSERT_TRUE(true_val.has_value());
  EXPECT_TRUE(true_val.value());

  auto false_val = root_obj->GetNodeValue<bool>("BoolFieldFalse");
  ASSERT_TRUE(false_val.has_value());
  EXPECT_FALSE(false_val.value());
}

TEST(JsonMockup, CanHandleMistypedGets) {
  auto json_intf = NewJsonMockupInterface(R"json(
    {
      "StringField": "string value",
      "IntField": 321,
      "DoubleField": 95.3,
      "BoolFieldTrue": true,
      "BoolFieldFalse": false
    }
  )json");
  auto root_obj = json_intf->GetRoot().AsObject();
  ASSERT_TRUE(root_obj) << "Root object should not be null.";

  EXPECT_FALSE(root_obj->GetNodeValue<std::string>("IntField").has_value());
  EXPECT_FALSE(root_obj->GetNodeValue<std::string>("DoubleField").has_value());
  EXPECT_FALSE(
      root_obj->GetNodeValue<std::string>("BoolFieldTrue").has_value());
  EXPECT_FALSE(
      root_obj->GetNodeValue<std::string>("BoolFieldFalse").has_value());

  EXPECT_FALSE(root_obj->GetNodeValue<int>("DoubleField").has_value());
  EXPECT_FALSE(root_obj->GetNodeValue<int>("StringField").has_value());
  EXPECT_FALSE(root_obj->GetNodeValue<int>("BoolFieldTrue").has_value());
  EXPECT_FALSE(root_obj->GetNodeValue<int>("BoolFieldFalse").has_value());

  EXPECT_FALSE(root_obj->GetNodeValue<double>("StringField").has_value());
  EXPECT_FALSE(root_obj->GetNodeValue<double>("BoolFieldTrue").has_value());
  EXPECT_FALSE(root_obj->GetNodeValue<double>("BoolFieldFalse").has_value());

  EXPECT_FALSE(root_obj->GetNodeValue<bool>("StringField").has_value());
  EXPECT_FALSE(root_obj->GetNodeValue<bool>("IntField").has_value());
  EXPECT_FALSE(root_obj->GetNodeValue<bool>("DoubleField").has_value());
}

TEST(JsonMockup, CanGetUriProperty) {
  auto json_intf = NewJsonMockupInterface(R"json(
    {
      "@odata.id": "id"
    }
  )json");
  auto root_obj = json_intf->GetRoot().AsObject();
  ASSERT_TRUE(root_obj) << "Root object should not be null.";

  auto str_val = root_obj->GetUriString();
  ASSERT_TRUE(str_val.has_value());
  EXPECT_THAT(str_val.value(), Eq("id"));
}

TEST(JsonMockup, CanGetSubObjectFields) {
  auto json_intf = NewJsonMockupInterface(R"json(
    {
      "SubObject": {
        "IntField": 321
       }
    }
  )json");
  auto root_obj = json_intf->GetRoot().AsObject();
  ASSERT_TRUE(root_obj) << "Root object should not be null.";

  auto subobj_obj = (*root_obj)["SubObject"].AsObject();
  ASSERT_TRUE(subobj_obj) << "SubObject should not be null.";

  auto int_val = subobj_obj->GetNodeValue<int>("IntField");
  ASSERT_TRUE(int_val.has_value());
  EXPECT_THAT(int_val.value(), Eq(321));
}

TEST(JsonMockup, EmptyArray) {
  auto json_intf = NewJsonMockupInterface(R"json([])json");
  auto root_itr = json_intf->GetRoot().AsIterable();
  ASSERT_TRUE(root_itr) << "Root object should not be null.";

  EXPECT_THAT(root_itr->Size(), Eq(0));
  EXPECT_TRUE(root_itr->Empty());

  // Getting an element should return an unviewable variant
  auto elem0 = (*root_itr)[0];
  EXPECT_FALSE(elem0.AsObject());
  EXPECT_FALSE(elem0.AsIterable());
}

TEST(JsonMockup, CanGetArrayFields) {
  auto json_intf = NewJsonMockupInterface(R"json(
    {
      "ObjArray": [
        { "Val": 0 },
        { "Val": 1 },
        { "Val": 2 }
       ]
    }
  )json");
  auto root_obj = json_intf->GetRoot().AsObject();
  ASSERT_TRUE(root_obj) << "Root object should not be null.";

  auto subobj_itr = (*root_obj)["ObjArray"].AsIterable();
  ASSERT_TRUE(subobj_itr) << "ObjArray should not be null.";
  EXPECT_THAT(subobj_itr->Size(), Eq(3));
  EXPECT_FALSE(subobj_itr->Empty());

  auto elem0 = (*subobj_itr)[0];
  auto elem0_obj = elem0.AsObject();
  ASSERT_TRUE(elem0_obj);
  auto elem0_val = elem0_obj->GetNodeValue<int>("Val");
  ASSERT_TRUE(elem0_val.has_value());
  EXPECT_THAT(elem0_val.value(), Eq(0));

  auto elem1 = (*subobj_itr)[1];
  auto elem1_obj = elem1.AsObject();
  ASSERT_TRUE(elem1_obj);
  auto elem1_val = elem1_obj->GetNodeValue<int>("Val");
  ASSERT_TRUE(elem1_val.has_value());
  EXPECT_THAT(elem1_val.value(), Eq(1));

  auto elem2 = (*subobj_itr)[2];
  auto elem2_obj = elem2.AsObject();
  ASSERT_TRUE(elem2_obj);
  auto elem2_val = elem2_obj->GetNodeValue<int>("Val");
  ASSERT_TRUE(elem2_val.has_value());
  EXPECT_THAT(elem2_val.value(), Eq(2));

  // Getting an out-of-bounds element should return an unviewable variant
  auto elem3 = (*subobj_itr)[3];
  EXPECT_FALSE(elem3.AsObject());
  EXPECT_FALSE(elem3.AsIterable());
  auto elem_neg = (*subobj_itr)[-1];
  EXPECT_FALSE(elem_neg.AsObject());
  EXPECT_FALSE(elem_neg.AsIterable());
}

TEST(JsonMockup, CanGetCollectionFields) {
  auto json_intf = NewJsonMockupInterface(R"json(
    {
      "Members@odata.count": 3,
      "Members": [
        { "Val": 0 },
        { "Val": 1 },
        { "Val": 2 }
       ]
    }
  )json");
  auto root_itr = json_intf->GetRoot().AsIterable();
  ASSERT_TRUE(root_itr) << "Root object should not be null.";

  auto elem0 = (*root_itr)[0];
  auto elem0_obj = elem0.AsObject();
  ASSERT_TRUE(elem0_obj);
  auto elem0_val = elem0_obj->GetNodeValue<int>("Val");
  ASSERT_TRUE(elem0_val.has_value());
  EXPECT_THAT(elem0_val.value(), Eq(0));

  auto elem1 = (*root_itr)[1];
  auto elem1_obj = elem1.AsObject();
  ASSERT_TRUE(elem1_obj);
  auto elem1_val = elem1_obj->GetNodeValue<int>("Val");
  ASSERT_TRUE(elem1_val.has_value());
  EXPECT_THAT(elem1_val.value(), Eq(1));

  auto elem2 = (*root_itr)[2];
  auto elem2_obj = elem2.AsObject();
  ASSERT_TRUE(elem2_obj);
  auto elem2_val = elem2_obj->GetNodeValue<int>("Val");
  ASSERT_TRUE(elem2_val.has_value());
  EXPECT_THAT(elem2_val.value(), Eq(2));

  // Getting an out-of-bounds element should return an unviewable variant
  auto elem3 = (*root_itr)[3];
  EXPECT_FALSE(elem3.AsObject());
  EXPECT_FALSE(elem3.AsIterable());
  auto elem_neg = (*root_itr)[-1];
  EXPECT_FALSE(elem_neg.AsObject());
  EXPECT_FALSE(elem_neg.AsIterable());
}

TEST(JsonMockup, InvalidCollectionsAreNotIterable) {
  {
    auto json_intf = NewJsonMockupInterface(R"json(
      {
        "Members": [
          { "Val": 0 },
          { "Val": 1 },
          { "Val": 2 }
         ]
      }
    )json");
    auto root_itr = json_intf->GetRoot().AsIterable();
    ASSERT_FALSE(root_itr) << "Missing Members@odata.count should fail";
  }
  {
    auto json_intf = NewJsonMockupInterface(R"json(
      {
        "Members@odata.count": 0
      }
    )json");
    auto root_itr = json_intf->GetRoot().AsIterable();
    ASSERT_FALSE(root_itr) << "Missing Members should fail";
  }
  {
    auto json_intf = NewJsonMockupInterface(R"json(
      {
        "Members": "NotAnArray"
      }
    )json");
    auto root_itr = json_intf->GetRoot().AsIterable();
    ASSERT_FALSE(root_itr) << "Non-array Members element should fail";
  }
}

TEST(JsonMockup, CanGetUri) {
  auto json_intf = NewJsonMockupInterface(R"json(
    {
      "Name": "root",
      "Obj" : {
        "Name": "Obj",
        "SubObj": {
          "Name": "SubObj",
          "Val": 42
        },
        "SubArray": [
          {
            "Name": "SubArray0",
            "Val": 0
          },
          {
            "Name": "SubArray1",
            "Val": 1
          },
          {
            "Name": "SubArray2",
            "Val": 2
          }
        ],
        "SubCollection" : {
          "Members@odata.count": 3,
          "Members": [
            {
              "Name": "SubCollection0",
              "Val": 3
            },
            {
              "Name": "SubCollection1",
              "Val": 4
            },
            {
              "Name": "SubCollection2",
              "Val": 5
            }
          ]
        }
      }
    }
  )json");

  auto root = json_intf->GetUri("").AsObject();
  ASSERT_TRUE(root);
  EXPECT_THAT(root->GetNodeValue<std::string>("Name"), Eq("root"));
  auto root_also = json_intf->GetUri("/").AsObject();
  ASSERT_TRUE(root);
  EXPECT_THAT(root->GetNodeValue<std::string>("Name"), Eq("root"));

  auto obj = json_intf->GetUri("Obj").AsObject();
  ASSERT_TRUE(obj);
  EXPECT_THAT(obj->GetNodeValue<std::string>("Name"), Eq("Obj"));
  auto obj_also = json_intf->GetUri("/Obj").AsObject();
  ASSERT_TRUE(obj_also);
  EXPECT_THAT(obj_also->GetNodeValue<std::string>("Name"), Eq("Obj"));
  auto obj_also2 = json_intf->GetUri("Obj/").AsObject();
  ASSERT_TRUE(obj_also2);
  EXPECT_THAT(obj_also2->GetNodeValue<std::string>("Name"), Eq("Obj"));
  auto obj_also3 = json_intf->GetUri("/Obj/").AsObject();
  ASSERT_TRUE(obj_also3);
  EXPECT_THAT(obj_also3->GetNodeValue<std::string>("Name"), Eq("Obj"));

  auto obj_subobj = json_intf->GetUri("Obj/SubObj").AsObject();
  ASSERT_TRUE(obj_subobj);
  EXPECT_THAT(obj_subobj->GetNodeValue<std::string>("Name"), Eq("SubObj"));
  EXPECT_THAT(obj_subobj->GetNodeValue<int>("Val"), Eq(42));

  std::string str_value;
  ASSERT_TRUE(json_intf->GetUri("Obj/SubObj/Name").GetValue(&str_value));
  EXPECT_THAT(str_value, Eq("SubObj"));

  {
    auto obj_subarray = json_intf->GetUri("Obj/SubArray").AsIterable();
    ASSERT_TRUE(obj_subarray);
    int counter = 0;
    for (auto elem : *obj_subarray) {
      auto obj = elem.AsObject();
      ASSERT_TRUE(obj);
      EXPECT_THAT(obj->GetNodeValue<int>("Val"), Eq(counter));
      ++counter;
    }
    int int_value;
    ASSERT_TRUE(json_intf->GetUri("Obj/SubArray/0/Val").GetValue(&int_value));
    EXPECT_THAT(int_value, Eq(0));
    ASSERT_TRUE(json_intf->GetUri("Obj/SubArray/1/Val").GetValue(&int_value));
    EXPECT_THAT(int_value, Eq(1));
    ASSERT_TRUE(json_intf->GetUri("Obj/SubArray/2/Val").GetValue(&int_value));
    EXPECT_THAT(int_value, Eq(2));
    ASSERT_FALSE(json_intf->GetUri("Obj/SubArray/-1/Val").GetValue(&int_value));
    ASSERT_FALSE(json_intf->GetUri("Obj/SubArray/3/Val").GetValue(&int_value));
  }

  {
    auto obj_subcollection =
        json_intf->GetUri("Obj/SubCollection").AsIterable();
    ASSERT_TRUE(obj_subcollection);
    int counter = 3;
    for (auto elem : *obj_subcollection) {
      auto obj = elem.AsObject();
      ASSERT_TRUE(obj);
      EXPECT_THAT(obj->GetNodeValue<int>("Val"), Eq(counter));
      ++counter;
    }
    int int_value;
    ASSERT_TRUE(
        json_intf->GetUri("Obj/SubCollection/0/Val").GetValue(&int_value));
    EXPECT_THAT(int_value, Eq(3));
    ASSERT_TRUE(
        json_intf->GetUri("Obj/SubCollection/1/Val").GetValue(&int_value));
    EXPECT_THAT(int_value, Eq(4));
    ASSERT_TRUE(
        json_intf->GetUri("Obj/SubCollection/2/Val").GetValue(&int_value));
    EXPECT_THAT(int_value, Eq(5));
    ASSERT_FALSE(
        json_intf->GetUri("Obj/SubCollection/-1/Val").GetValue(&int_value));
    ASSERT_FALSE(
        json_intf->GetUri("Obj/SubCollection/3/Val").GetValue(&int_value));
  }
}

TEST(JsonMockup, DateTime) {
  auto json_intf = NewJsonMockupInterface(R"json(
  {
    "DateTime": "2020-07-20T00:00:35+00:00"
  }
  )json");

  absl::Time dt;
  RedfishVariant var = json_intf->GetUri("/DateTime");
  EXPECT_TRUE(var.GetValue(&dt));

  absl::TimeZone utc;
  assert(absl::LoadTimeZone("UTC", &utc));
  absl::Time dt_gold = absl::FromDateTime(2020, 7, 20, 0, 0, 35, utc);
  EXPECT_EQ(dt, dt_gold);
}

TEST(JsonMockup, ForEachProperty) {
  auto json_intf = NewJsonMockupInterface(R"json(
    {
      "StringField": "string value",
      "IntField": 2147483647,
      "Int64Field": 9223372036854775807,
      "DoubleField": 95.3,
      "BoolFieldTrue": true,
      "BoolFieldFalse": false,
      "ObjField": { "Field": "yes" },
      "ArrField": [ 1, 2, 3, 4, 5 ]
    }
  )json");

  std::vector<std::pair<std::string, std::string>> all_properties;
  json_intf->GetUri("/").AsObject()->ForEachProperty(
      [&all_properties](absl::string_view name, RedfishVariant value) {
        all_properties.push_back(
            std::make_pair(std::string(name), value.DebugString()));
        return RedfishIterReturnValue::kContinue;
      });
  EXPECT_THAT(all_properties,
              UnorderedElementsAre(
                  std::make_pair("StringField", "\"string value\""),
                  std::make_pair("IntField", "2147483647"),
                  std::make_pair("Int64Field", "9223372036854775807"),
                  std::make_pair("DoubleField", "95.3"),
                  std::make_pair("BoolFieldTrue", "true"),
                  std::make_pair("BoolFieldFalse", "false"),
                  std::make_pair("ObjField", "{\n \"Field\": \"yes\"\n}"),
                  std::make_pair("ArrField", "[\n 1,\n 2,\n 3,\n 4,\n 5\n]")));
}

TEST(JsonMockup, ForEachPropertyStop) {
  auto json_intf = NewJsonMockupInterface(R"json(
    {
      "StringField": "string value",
      "IntField": 2147483647,
      "Int64Field": 9223372036854775807,
      "DoubleField": 95.3,
      "BoolFieldTrue": true,
      "BoolFieldFalse": false,
      "ObjField": { "Field": "yes" },
      "ArrField": [ 1, 2, 3, 4, 5 ]
    }
  )json");
  std::vector<std::pair<std::string, std::string>> all_properties;
  int called = 0;
  json_intf->GetUri("/").AsObject()->ForEachProperty(
      [&called](absl::string_view name, RedfishVariant value) {
        ++called;
        return RedfishIterReturnValue::kStop;
      });
  EXPECT_THAT(called, Eq(1));
}

}  // namespace
}  // namespace libredfish
