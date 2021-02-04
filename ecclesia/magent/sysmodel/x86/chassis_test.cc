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

#include "ecclesia/magent/sysmodel/x86/chassis.h"

#include <memory>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/optional.h"
#include "ecclesia/lib/io/usb/ids.h"
#include "ecclesia/lib/io/usb/mocks.h"
#include "ecclesia/lib/io/usb/usb.h"

namespace ecclesia {
namespace {

using ::testing::ElementsAre;
using ::testing::Return;

TEST(ChassisIdTest, GetChassisStringAndType) {
  EXPECT_EQ(ChassisIdToString(ChassisId::kIndus), "Indus");
  EXPECT_EQ(GetChassisType(ChassisId::kIndus), ChassisType::kRackMount);
  EXPECT_EQ(GetChassisTypeAsString(ChassisId::kIndus), "RackMount");
  EXPECT_EQ(ChassisIdToString(ChassisId::kSleipnir), "Sleipnir");
  EXPECT_EQ(GetChassisType(ChassisId::kSleipnir),
            ChassisType::kStorageEnclosure);
  EXPECT_EQ(GetChassisTypeAsString(ChassisId::kSleipnir), "StorageEnclosure");
}

TEST(DetectChassisByUsbTest, DetectUsbChassis) {
  auto usb_location0 = UsbLocation::Make<2, 1>();
  auto sleipnir_usb_location = UsbLocation::Make<1, 6>();
  MockUsbDiscovery usb_discovery;
  auto sleipnir_bmc = std::make_unique<MockUsbDevice>();
  EXPECT_CALL(*sleipnir_bmc, GetSignature())
      .WillOnce(Return(kUsbSignatureSleipnirBmc));
  std::vector<UsbLocation> usb_locations = {usb_location0,
                                            sleipnir_usb_location};
  EXPECT_CALL(usb_discovery, EnumerateAllUsbDevices())
      .WillOnce(Return(usb_locations));

  EXPECT_CALL(usb_discovery, CreateDevice(usb_location0)).WillOnce([](auto...) {
    return nullptr;
  });
  EXPECT_CALL(usb_discovery, CreateDevice(sleipnir_usb_location))
      .WillOnce([&](auto...) { return std::move(sleipnir_bmc); });

  auto maybe_chassisid = DetectChassisByUsb(&usb_discovery);
  ASSERT_TRUE(maybe_chassisid.has_value());
  EXPECT_EQ(maybe_chassisid.value(), ChassisId::kSleipnir);
}

TEST(DetectChassisByUsbTest, NoUsbChassisFound) {
  MockUsbDiscovery usb_discovery;
  auto usb_location0 = UsbLocation::Make<2, 1>();
  std::vector<UsbLocation> usb_locations = {usb_location0};
  EXPECT_CALL(usb_discovery, EnumerateAllUsbDevices())
      .WillOnce([&](auto...) { return std::move(usb_locations); })
      .WillOnce([](auto...) { return absl::InternalError("Some error"); });

  // The first time no chassis found is because usb device is nullptr.
  EXPECT_CALL(usb_discovery, CreateDevice(usb_location0)).WillOnce([](auto...) {
    return nullptr;
  });
  auto maybe_chassisid = DetectChassisByUsb(&usb_discovery);
  EXPECT_FALSE(maybe_chassisid.has_value());
  // The second time no chassis found is due to EnumerateAllUsbDevices error.
  maybe_chassisid = DetectChassisByUsb(&usb_discovery);
  EXPECT_FALSE(maybe_chassisid.has_value());
}

TEST(CreateChassisTest, CreateServerAndUsbChassis) {
  auto usb_location0 = UsbLocation::Make<2, 1>();
  auto sleipnir_usb_location = UsbLocation::Make<1, 6>();
  MockUsbDiscovery usb_discovery;
  auto sleipnir_bmc = std::make_unique<MockUsbDevice>();
  EXPECT_CALL(*sleipnir_bmc, GetSignature())
      .WillOnce(Return(kUsbSignatureSleipnirBmc));
  std::vector<UsbLocation> usb_locations = {usb_location0,
                                            sleipnir_usb_location};
  EXPECT_CALL(usb_discovery, EnumerateAllUsbDevices())
      .WillOnce(Return(usb_locations));

  EXPECT_CALL(usb_discovery, CreateDevice(usb_location0)).WillOnce([](auto...) {
    return nullptr;
  });
  EXPECT_CALL(usb_discovery, CreateDevice(sleipnir_usb_location))
      .WillOnce([&](auto...) { return std::move(sleipnir_bmc); });

  std::vector<ChassisId> chassis_ids = CreateChassis(&usb_discovery);
  EXPECT_THAT(chassis_ids,
              ElementsAre(ChassisId::kIndus, ChassisId::kSleipnir));
}

}  // namespace
}  // namespace ecclesia
