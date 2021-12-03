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

#include "ecclesia/lib/redfish/transport/http_redfish_intf.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/http/client.h"
#include "ecclesia/lib/http/cred.pb.h"
#include "ecclesia/lib/http/curl_client.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/property_definitions.h"
#include "ecclesia/lib/redfish/testing/fake_redfish_server.h"
#include "ecclesia/lib/redfish/transport/http.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "single_include/nlohmann/json.hpp"

namespace libredfish {
namespace {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::UnorderedElementsAre;

// Test harness to start a FakeRedfishServer and create a RedfishInterface
// for testing.
class HttpRedfishInterfaceTest : public ::testing::Test {
 protected:
  HttpRedfishInterfaceTest() {
    server_ = std::make_unique<ecclesia::FakeRedfishServer>(
        "barebones_session_auth/mockup.shar",
        absl::StrCat(ecclesia::GetTestTempUdsDirectory(), "/mockup.socket"));
    auto config = server_->GetConfig();
    ecclesia::HttpCredential creds;
    auto curl_http_client = std::make_unique<ecclesia::CurlHttpClient>(
        ecclesia::LibCurlProxy::CreateInstance(), creds);
    auto transport = ecclesia::HttpRedfishTransport::MakeNetwork(
        std::move(curl_http_client),
        absl::StrFormat("%s:%d", config.hostname, config.port));
    intf_ = NewHttpInterface(std::move(transport), RedfishInterface::kTrusted);
  }

  std::unique_ptr<ecclesia::FakeRedfishServer> server_;
  std::unique_ptr<RedfishInterface> intf_;
};

TEST_F(HttpRedfishInterfaceTest, GetRoot) {
  auto root = intf_->GetRoot();
  EXPECT_THAT(nlohmann::json::parse(root.DebugString(), nullptr,
                                    /*allow_exceptions=*/false),
              Eq(nlohmann::json::parse(R"json({
  "@odata.context": "/redfish/v1/$metadata#ServiceRoot.ServiceRoot",
  "@odata.id": "/redfish/v1",
  "@odata.type": "#ServiceRoot.v1_5_0.ServiceRoot",
  "Chassis": {
      "@odata.id": "/redfish/v1/Chassis"
  },
  "Id": "RootService",
  "Links": {
      "Sessions": {
          "@odata.id": "/redfish/v1/SessionService/Sessions"
      }
  },
  "Name": "Root Service",
  "RedfishVersion": "1.6.1"
})json")));
}

TEST_F(HttpRedfishInterfaceTest, CrawlToChassisCollection) {
  auto chassis_collection = intf_->GetRoot()[libredfish::kRfPropertyChassis];
  EXPECT_THAT(nlohmann::json::parse(chassis_collection.DebugString(), nullptr,
                                    /*allow_exceptions=*/false),
              Eq(nlohmann::json::parse(R"json({
    "@odata.context": "/redfish/v1/$metadata#ChassisCollection.ChassisCollection",
    "@odata.id": "/redfish/v1/Chassis",
    "@odata.type": "#ChassisCollection.ChassisCollection",
    "Members": [
        {
            "@odata.id": "/redfish/v1/Chassis/chassis"
        }
    ],
    "Members@odata.count": 1,
    "Name": "Chassis Collection"
})json")));
}

TEST_F(HttpRedfishInterfaceTest, CrawlToChassis) {
  auto chassis_collection = intf_->GetRoot()[libredfish::kRfPropertyChassis][0];
  EXPECT_THAT(nlohmann::json::parse(chassis_collection.DebugString(), nullptr,
                                    /*allow_exceptions=*/false),
              Eq(nlohmann::json::parse(R"json({
    "@odata.context": "/redfish/v1/$metadata#Chassis.Chassis",
    "@odata.id": "/redfish/v1/Chassis/chassis",
    "@odata.type": "#Chassis.v1_10_0.Chassis",
    "Id": "chassis",
    "Name": "chassis",
    "Status": {
        "State": "StandbyOffline"
    }
})json")));
}

TEST_F(HttpRedfishInterfaceTest, GetUri) {
  auto chassis = intf_->GetUri("/redfish/v1/Chassis/chassis");
  EXPECT_THAT(nlohmann::json::parse(chassis.DebugString(), nullptr,
                                    /*allow_exceptions=*/false),
              Eq(nlohmann::json::parse(R"json({
    "@odata.context": "/redfish/v1/$metadata#Chassis.Chassis",
    "@odata.id": "/redfish/v1/Chassis/chassis",
    "@odata.type": "#Chassis.v1_10_0.Chassis",
    "Id": "chassis",
    "Name": "chassis",
    "Status": {
        "State": "StandbyOffline"
    }
})json")));
}

TEST_F(HttpRedfishInterfaceTest, GetUriFragmentString) {
  auto chassis = intf_->GetUri("/redfish/v1/Chassis/chassis#/Name");
  EXPECT_THAT(chassis.DebugString(), Eq("\"chassis\""));
}

TEST_F(HttpRedfishInterfaceTest, GetUriFragmentObject) {
  auto status = intf_->GetUri("/redfish/v1/Chassis/chassis#/Status");
  EXPECT_THAT(nlohmann::json::parse(status.DebugString(), nullptr,
                                    /*allow_exceptions=*/false),
              Eq(nlohmann::json::parse(R"json({
    "State": "StandbyOffline"
})json")));
}

TEST_F(HttpRedfishInterfaceTest, EachTest) {
  std::vector<std::string> names;
  intf_->GetRoot()[libredfish::kRfPropertyChassis].Each().Do(
      [&names](std::unique_ptr<libredfish::RedfishObject> &obj) {
        auto name = obj->GetNodeValue<libredfish::PropertyName>();
        if (name.has_value()) names.push_back(*std::move(name));
      });
  EXPECT_THAT(names, ElementsAre("chassis"));
}

TEST_F(HttpRedfishInterfaceTest, ForEachPropertyTest) {
  auto chassis = intf_->GetUri("/redfish/v1/Chassis/chassis");
  std::vector<std::pair<std::string, std::string>> all_properties;
  chassis.AsObject()->ForEachProperty(
      [&all_properties](absl::string_view name, RedfishVariant value) {
        all_properties.push_back(
            std::make_pair(std::string(name), value.DebugString()));
        return RedfishObject::ForEachReturn::kContinue;
      });
  EXPECT_THAT(
      all_properties,
      UnorderedElementsAre(
          std::make_pair("@odata.context",
                         "\"/redfish/v1/$metadata#Chassis.Chassis\""),
          std::make_pair("@odata.id", "\"/redfish/v1/Chassis/chassis\""),
          std::make_pair("@odata.type", "\"#Chassis.v1_10_0.Chassis\""),
          std::make_pair("Id", "\"chassis\""),
          std::make_pair("Name", "\"chassis\""),
          std::make_pair("Status", "{\"State\":\"StandbyOffline\"}")));
}

TEST_F(HttpRedfishInterfaceTest, ForEachPropertyTestStop) {
  auto chassis = intf_->GetUri("/redfish/v1/Chassis/chassis");
  std::vector<std::pair<std::string, std::string>> all_properties;
  int called = 0;
  chassis.AsObject()->ForEachProperty(
      [&called](absl::string_view name, RedfishVariant value) {
        ++called;
        return RedfishObject::ForEachReturn::kStop;
      });
  EXPECT_THAT(called, Eq(1));
}

}  // namespace
}  // namespace libredfish
