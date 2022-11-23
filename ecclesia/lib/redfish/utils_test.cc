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

#include "ecclesia/lib/redfish/utils.h"

#include <memory>
#include <optional>
#include <string>

#include "gtest/gtest.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/property_definitions.h"
#include "ecclesia/lib/redfish/test_mockup.h"
#include "ecclesia/lib/redfish/testing/fake_redfish_server.h"
#include "ecclesia/lib/redfish/testing/json_mockup.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {
namespace {

constexpr absl::string_view kMemoryResource = R"json({
    "@odata.id": "/redfish/v1/Systems/system/Memory/dimm0",
    "@odata.type": "#Memory.v1_11_0.Memory",
    "AllowedSpeedsMHz": [],
    "BaseModuleType": "RDIMM",
    "BusWidthBits": 0,
    "CapacityMiB": 65536,
    "DataWidthBits": 64,
    "ErrorCorrection": "NoECC",
    "FirmwareRevision": "0",
    "Id": "dimm0",
    "Links": {
        "Chassis": { "@odata.id": "/redfish/v1/Chassis/GSZ_EVT" }
    },
    "Location": {
        "PartLocation": {
            "LocationType": "Slot",
            "ServiceLabel": "DIMM6"
        }
    },
    "Manufacturer": "Vendor",
    "MemoryDeviceType": "DDR5",
    "MemoryType": "DRAM",
    "MemoryLocation": {
        "MemoryDeviceLocator": "DIMM6"
    },
    "Model": "",
    "Name": "DIMM Slot",
    "OperatingSpeedMhz": 4800,
    "PartNumber": "part-12345",
    "RankCount": 2,
    "SerialNumber": "01579C7E",
    "SparePartNumber": "",
    "Status": {
        "Health": "OK",
        "HealthRollup": "OK",
        "State": "Enabled"
    }
})json";

TEST(GetConvertedResourceName, ConvertedNameFromNameProperty) {
  ecclesia::FakeRedfishServer mockup("topology_v2_testing/mockup.shar");
  auto raw_intf = mockup.RedfishClientInterface();

  auto redfish_obj =
      raw_intf->CachedGetUri("/redfish/v1/Chassis/expansion_child").AsObject();
  auto raw_name = redfish_obj->GetNodeValue<PropertyName>();
  ASSERT_TRUE(raw_name.has_value());
  EXPECT_EQ(*raw_name, " Expansion Child ");
  auto converted_name = GetConvertedResourceName(*redfish_obj);
  ASSERT_TRUE(converted_name.has_value());
  EXPECT_EQ(*converted_name, "expansion_child");

  redfish_obj =
      raw_intf->CachedGetUri("/redfish/v1/Chassis/expansion_tray").AsObject();
  raw_name = redfish_obj->GetNodeValue<PropertyName>();
  ASSERT_TRUE(raw_name.has_value());
  EXPECT_EQ(*raw_name, "Expansion Tray ");
  converted_name = GetConvertedResourceName(*redfish_obj);
  ASSERT_TRUE(converted_name.has_value());
  EXPECT_EQ(*converted_name, "expansion_tray");
}

TEST(GetConvertedResourceName, CorrectMemoryResourceName) {
  nlohmann::json memory_resource_json = nlohmann::json::parse(kMemoryResource);

  auto redfish_obj = std::make_unique<JsonMockupObject>(memory_resource_json);

  auto raw_name = redfish_obj->GetNodeValue<PropertyName>();
  ASSERT_TRUE(raw_name.has_value());
  EXPECT_EQ(*raw_name, "DIMM Slot");
  auto converted_name = GetConvertedResourceName(*redfish_obj);
  ASSERT_TRUE(converted_name.has_value());
  EXPECT_EQ(*converted_name, "ddr5");
}

TEST(ParseJson, CanParse) {
  EXPECT_EQ(ParseJson(R"({"a": "b"})"), nlohmann::json({{"a", "b"}}));
}

TEST(ParseJson, WontAbort) { EXPECT_TRUE(ParseJson(",").is_discarded()); }

TEST(JsonToString, CanSerialize) {
  EXPECT_EQ(JsonToString(nlohmann::json{{"a", "b"}}), R"({"a":"b"})");
  EXPECT_EQ(JsonToString(nlohmann::json("aaa")), "\"aaa\"");
}

TEST(JsonToString, WontAbort) {
  EXPECT_EQ(JsonToString(nlohmann::json("\xff")),  // This is invalid UTF-8.
            "\"\"");
}

TEST(TruncateLastUnderScoreAndNumericSuffix, Works) {
  EXPECT_EQ(TruncateLastUnderScoreAndNumericSuffix("hdmi_cable"), "hdmi_cable");
  EXPECT_EQ(TruncateLastUnderScoreAndNumericSuffix("resource_1"), "resource");
  EXPECT_EQ(TruncateLastUnderScoreAndNumericSuffix("resource_1a"),
            "resource_1a");
  EXPECT_EQ(TruncateLastUnderScoreAndNumericSuffix("resource_1_2"),
            "resource_1");
}

}  // namespace
}  // namespace ecclesia
