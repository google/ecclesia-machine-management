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

#include "ecclesia/lib/redfish/location.h"

#include <memory>
#include <optional>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/testing/json_mockup.h"

namespace ecclesia {
namespace {

TEST(HealthRollup, NoLocation) {
  std::unique_ptr<RedfishInterface> intf = NewJsonMockupInterface(R"json({
        "@odata.id": "/redfish/v1/Chassis/TestChassis",
        "@odata.type": "#Chassis.v1_14_0.Chassis",
        "Id": "TestChassis",
        "Name": "SansLocation"
      })json");
  std::unique_ptr<RedfishObject> obj = intf->GetRoot().AsObject();
  ASSERT_NE(obj, nullptr);

  std::optional<SupplementalLocationInfo> location =
      GetSupplementalLocationInfo(*obj);
  ASSERT_EQ(location, std::nullopt);
}

TEST(HealthRollup, WithPartLocationContextAndServiceLabel) {
  std::unique_ptr<RedfishInterface> intf = NewJsonMockupInterface(R"json({
        "@odata.id": "/redfish/v1/Chassis/TestChassis",
        "@odata.type": "#Chassis.v1_14_0.Chassis",
        "Id": "TestChassis",
        "Location": {
          "PartLocation": {
            "LocationType": "Slot",
            "ServiceLabel": "XYZ"
          },
          "PartLocationContext": "ABC/CDE"
        },
        "Name": "TestResource"
      })json");
  std::unique_ptr<RedfishObject> obj = intf->GetRoot().AsObject();
  ASSERT_NE(obj, nullptr);

  std::optional<SupplementalLocationInfo> location =
      GetSupplementalLocationInfo(*obj);
  ASSERT_NE(location, std::nullopt);
  EXPECT_EQ(location->service_label, "XYZ");
  EXPECT_EQ(location->part_location_context, "ABC/CDE");
}

TEST(HealthRollup, WithServiceLabelOnly) {
  std::unique_ptr<RedfishInterface> intf = NewJsonMockupInterface(R"json({
        "@odata.id": "/redfish/v1/Chassis/TestChassis",
        "@odata.type": "#Chassis.v1_14_0.Chassis",
        "Id": "TestChassis",
        "Location": {
          "PartLocation": {
            "LocationType": "Slot",
            "ServiceLabel": "XYZ"
          }
        },
        "Name": "TestResource"
      })json");
  std::unique_ptr<RedfishObject> obj = intf->GetRoot().AsObject();
  ASSERT_NE(obj, nullptr);

  std::optional<SupplementalLocationInfo> location =
      GetSupplementalLocationInfo(*obj);
  ASSERT_NE(location, std::nullopt);
  EXPECT_EQ(location->service_label, "XYZ");
  EXPECT_EQ(location->part_location_context, "");
}

TEST(HealthRollup, WithPartLocationContextOnly) {
  std::unique_ptr<RedfishInterface> intf = NewJsonMockupInterface(R"json({
        "@odata.id": "/redfish/v1/Chassis/TestChassis",
        "@odata.type": "#Chassis.v1_14_0.Chassis",
        "Id": "TestChassis",
        "Location": {
          "PartLocation": {
            "LocationType": "Slot"
          },
          "PartLocationContext": "ABC/CDE"
        },
        "Name": "TestResource"
      })json");
  std::unique_ptr<RedfishObject> obj = intf->GetRoot().AsObject();
  ASSERT_NE(obj, nullptr);

  std::optional<SupplementalLocationInfo> location =
      GetSupplementalLocationInfo(*obj);
  ASSERT_NE(location, std::nullopt);
  EXPECT_EQ(location->service_label, "");
  EXPECT_EQ(location->part_location_context, "ABC/CDE");
}

TEST(HealthRollup, LocationWithoutSupplementalInfo) {
  std::unique_ptr<RedfishInterface> intf = NewJsonMockupInterface(R"json({
        "@odata.id": "/redfish/v1/Chassis/TestChassis",
        "@odata.type": "#Chassis.v1_14_0.Chassis",
        "Id": "TestChassis",
        "Location": {
          "PartLocation": {
            "LocationType": "Slot"
          }
        },
        "Name": "TestResource"
      })json");
  std::unique_ptr<RedfishObject> obj = intf->GetRoot().AsObject();
  ASSERT_NE(obj, nullptr);

  std::optional<SupplementalLocationInfo> location =
      GetSupplementalLocationInfo(*obj);
  ASSERT_EQ(location, std::nullopt);
}

TEST(HealthRollup, PhysicalLocationWithSupplementalInfo) {
  std::unique_ptr<RedfishInterface> intf = NewJsonMockupInterface(R"json({
        "@odata.id": "/redfish/v1/Chassis/TestChassis/Drives/TestDrive",
        "@odata.type": "#Drive.v1_0_0.Drive",
        "Id": "TestDrive",
        "PhysicalLocation": {
          "PartLocation": {
            "LocationType": "Slot",
            "ServiceLabel": "XYZ"
          },
          "PartLocationContext": "ABC/CDE"
        },
        "Name": "TestResource"
      })json");
  std::unique_ptr<RedfishObject> obj = intf->GetRoot().AsObject();
  ASSERT_NE(obj, nullptr);

  std::optional<SupplementalLocationInfo> location =
      GetSupplementalLocationInfo(*obj);
  ASSERT_NE(location, std::nullopt);
  EXPECT_EQ(location->service_label, "XYZ");
  EXPECT_EQ(location->part_location_context, "ABC/CDE");
}

}  // namespace
}  // namespace ecclesia
