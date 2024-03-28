/*
 * Copyright 2021 Google LLC
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

#include "ecclesia/lib/redfish/testing/fake_redfish_server.h"

#include <memory>
#include <string>
#include <variant>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/property.h"
#include "ecclesia/lib/redfish/property_definitions.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {
namespace {

// Only a single handler thread is needed.
constexpr int kNumWorkerThreads = 1;

using ::testing::Eq;

TEST(PatchableMockupServer, CanProxy) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");

  auto redfish_intf = server.RedfishClientInterface();

  auto chassis_obj =
      redfish_intf->UncachedGetUri("/redfish/v1/Chassis/chassis").AsObject();
  ASSERT_TRUE(chassis_obj);
  EXPECT_THAT(chassis_obj->GetUriString(), Eq("/redfish/v1/Chassis/chassis"));
  EXPECT_THAT(chassis_obj->GetNodeValue<PropertyName>(), Eq("Indus Chassis"));

  auto system_obj =
      redfish_intf->UncachedGetUri("/redfish/v1/Systems/system").AsObject();
  ASSERT_TRUE(system_obj);
  EXPECT_THAT(system_obj->GetNodeValue<PropertyName>(), Eq("Indus"));
}

TEST(PatchableMockupServer, CanGetExpand) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");

  auto redfish_intf = server.RedfishClientInterface();

  auto chassis_obj =
      redfish_intf
          ->UncachedGetUri(
              "/redfish/v1/Chassis/chassis?$expand=.(level=1)&$filter=&")
          .AsObject();
  ASSERT_TRUE(chassis_obj);
  EXPECT_THAT(chassis_obj->GetUriString(), Eq("/redfish/v1/Chassis/chassis"));
  EXPECT_THAT(chassis_obj->GetNodeValue<PropertyName>(), Eq("Indus Chassis"));

  auto system_obj =
      redfish_intf->UncachedGetUri("/redfish/v1/Systems/system").AsObject();
  ASSERT_TRUE(system_obj);
  EXPECT_THAT(system_obj->GetNodeValue<PropertyName>(), Eq("Indus"));
}

TEST(PatchableMockupServer, CanAddHandlerOwningData) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  server.AddHttpGetHandlerWithOwnedData(
      "/redfish/v1", std::string("\"abc\""));
  auto transport = server.RedfishClientTransport();
  auto value = transport->Get("/redfish/v1");
  ASSERT_TRUE(value.ok());
  EXPECT_EQ(std::get<nlohmann::json>(value->body), nlohmann::json("abc"));
}

TEST(PatchableMockupServer, CanPatchDirect) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");

  constexpr char kMyPatch[] = R"json({ "Name": "My Patched Name" })json";
  server.AddHttpGetHandlerWithData("/redfish/v1/Chassis/chassis",
                                   absl::MakeSpan(kMyPatch));

  auto redfish_intf = server.RedfishClientInterface();

  // Chassis should be patched.
  auto chassis_obj =
      redfish_intf->UncachedGetUri("/redfish/v1/Chassis/chassis").AsObject();
  ASSERT_TRUE(chassis_obj);
  EXPECT_THAT(chassis_obj->GetNodeValue<PropertyName>(), Eq("My Patched Name"));

  // Systems should be untouched.
  auto system_obj =
      redfish_intf->UncachedGetUri("/redfish/v1/Systems/system").AsObject();
  ASSERT_TRUE(system_obj);
  EXPECT_THAT(system_obj->GetNodeValue<PropertyName>(), Eq("Indus"));
}

TEST(PatchableMockupServer, CanPatchViaCrawl) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");

  constexpr char kMyPatch[] = R"json({ "Name": "My Patched Name" })json";
  server.AddHttpGetHandlerWithData("/redfish/v1/Chassis/chassis",
                                   absl::MakeSpan(kMyPatch));

  auto redfish_intf = server.RedfishClientInterface();

  // Chassis should be patched, access the URI indirectly through Collection.
  auto chassis_itr =
      redfish_intf->UncachedGetUri("/redfish/v1/Chassis").AsIterable();
  ASSERT_TRUE(chassis_itr);

  bool chassis_found = false;
  for (auto chassis : *chassis_itr) {
    auto obj = chassis.AsObject();
    ASSERT_TRUE(obj);
    EXPECT_THAT(obj->GetNodeValue<PropertyName>(), Eq("My Patched Name"));
    chassis_found = true;
  }
  EXPECT_TRUE(chassis_found);
}

TEST(PatchableMockupServer, ExportsProtocolFeaturesSupportedCorrectly) {
  static constexpr absl::string_view expected_value = R"json(
    {
      "ExpandQuery": {
        "ExpandAll": true,
        "Levels": true,
        "Links": true,
        "MaxLevels": 6,
        "NoLinks": true
      },
      "FilterQuery": true,
      "TopSkipQuery": true
    }
  )json";

  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  server.EnableAllParamsGetHandler();
  auto redfish_intf = server.RedfishClientInterface();
  RedfishVariant root = redfish_intf->GetRoot();
  std::unique_ptr<RedfishObject> root_object = root.AsObject();
  ASSERT_TRUE(root_object);
  std::unique_ptr<RedfishObject> expand_features_json =
      (*root_object)[kProtocolFeaturesSupported].AsObject();
  EXPECT_TRUE(expand_features_json);

  nlohmann::json expected_json = nlohmann::json::parse(expected_value);
  nlohmann::json protocol_features_supported =
      expand_features_json->GetContentAsJson();
  EXPECT_EQ(protocol_features_supported, expected_json);
}

}  // namespace
}  // namespace ecclesia
