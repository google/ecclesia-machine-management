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

#include "ecclesia/lib/io/usb/usb.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "ecclesia/lib/io/usb/mocks.h"

namespace ecclesia {
namespace {

using ::testing::Return;

TEST(UsbPortSequenceTest, ValidateStaticFunctions) {
  static constexpr UsbPortSequence kEmptyDefault;
  EXPECT_EQ(kEmptyDefault.Size(), 0);

  static constexpr UsbPortSequence kEmptyExplicit = UsbPortSequence::Make<>();
  EXPECT_EQ(kEmptyExplicit.Size(), 0);

  static constexpr UsbPortSequence kLongSequence =
      UsbPortSequence::Make<1, 2, 3, 4, 5>();
  EXPECT_EQ(kLongSequence.Size(), 5);

  static constexpr UsbPortSequence kMaxSequence =
      UsbPortSequence::Make<1, 2, 3, 4, 5, 6>();
  EXPECT_EQ(kMaxSequence.Size(), 6);

  auto maybe_long_to_max = kLongSequence.Downstream(UsbPort::Make<6>());
  ASSERT_TRUE(maybe_long_to_max.has_value());
  auto &long_to_max = maybe_long_to_max.value();
  EXPECT_EQ(kMaxSequence, long_to_max);
}

TEST(UsbPortSequenceTest, ValidateDynamicFunctions) {
  auto maybe_seq = UsbPortSequence::TryMake({1, 2, 3, 4, 5, 6, 7});
  EXPECT_FALSE(maybe_seq.has_value());

  maybe_seq = UsbPortSequence::TryMake({1, 2, 3, 4, 5});
  ASSERT_TRUE(maybe_seq.has_value());
  auto &seq_0 = maybe_seq.value();
  EXPECT_EQ(seq_0.Size(), 5);

  auto maybe_seq_1 = UsbPortSequence::TryMake({1, 2, 3, 4, 5, 6});
  ASSERT_TRUE(maybe_seq_1.has_value());
  auto &seq_1 = maybe_seq_1.value();
  auto seq_0_child = seq_0.Downstream(UsbPort::Make<6>());
  ASSERT_TRUE(seq_0_child.has_value());
  EXPECT_EQ(seq_0_child.value(), seq_1);

  auto maybe_seq_1_child = seq_1.Downstream(UsbPort::Make<7>());
  EXPECT_FALSE(maybe_seq_1_child.has_value());
}

TEST(UsbLocationTest, ValidateStaticFunctions) {
  static constexpr UsbLocation kController = UsbLocation::Make<3>();
  EXPECT_EQ(kController.Bus().value(), 3);
  EXPECT_EQ(kController.NumPorts(), 0);

  static constexpr UsbLocation kDevice = UsbLocation::Make<3, 1, 1, 5>();
  EXPECT_EQ(kDevice.Bus().value(), 3);
  EXPECT_EQ(kDevice.NumPorts(), 3);
}

TEST(ExistUsbDeviceWithSignatureTest, InvalidArgument) {
  UsbSignature usb_sig{0x18d1, 0x0215};
  auto result = FindUsbDeviceWithSignature(nullptr, usb_sig);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), absl::StatusCode::kInvalidArgument);
}

TEST(ExistUsbDeviceWithSignatureTest, EnumerateUsbFailure) {
  MockUsbDiscovery usb_discover;
  EXPECT_CALL(usb_discover, EnumerateAllUsbDevices())
      .WillOnce(Return(absl::InternalError("Some internal error")));
  UsbSignature usb_sig{0x18d1, 0x0215};
  auto result = FindUsbDeviceWithSignature(&usb_discover, usb_sig);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), absl::StatusCode::kInternal);
}

TEST(ExistUsbDeviceWithSignatureTest, FoundMatchedDevice) {
  UsbSignature usb_sig{0x18d1, 0x0215};
  auto usb_device = std::make_unique<MockUsbDevice>();
  EXPECT_CALL(*usb_device, GetSignature()).WillOnce(Return(usb_sig));
  UsbLocation usb_location0 = UsbLocation::Make<1, 2>();
  UsbLocation usb_location1 = UsbLocation::Make<1, 3>();
  std::vector<UsbLocation> usb_locations = {usb_location0, usb_location1};

  MockUsbDiscovery usb_discover;
  EXPECT_CALL(usb_discover, EnumerateAllUsbDevices())
      .WillOnce(Return(usb_locations));

  EXPECT_CALL(usb_discover, CreateDevice(usb_location0)).WillOnce([](auto...) {
    return nullptr;
  });
  EXPECT_CALL(usb_discover, CreateDevice(usb_location1)).WillOnce([&](auto...) {
    return std::move(usb_device);
  });

  auto result = FindUsbDeviceWithSignature(&usb_discover, usb_sig);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value(), usb_location1);
}

}  // namespace
}  // namespace ecclesia
