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

#include "ecclesia/lib/io/usb/proto_lib.h"

#include <type_traits>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/types/optional.h"
#include "ecclesia/lib/io/usb/usb.h"
#include "ecclesia/lib/io/usb/usb.pb.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/testing/proto.h"

namespace ecclesia {
namespace {

TEST(UsbLocationToProto, NoPortsInLocation) {
  UsbBusLocation bus = UsbBusLocation::Make<1>();
  UsbPortSequence sequence;
  UsbLocation location(bus, sequence);

  UsbLocationProtobuf protobuf = UsbLocationToProto(location);
  EXPECT_THAT(protobuf, EqualsProto(R"pb(bus: 1 ports: "0")pb"));
}

TEST(UsbLocationToProto, PortSequenceAvailable) {
  UsbBusLocation bus = UsbBusLocation::Make<1>();
  UsbPortSequence sequence = UsbPortSequence::Make<1, 2, 3>();
  UsbLocation location(bus, sequence);

  UsbLocationProtobuf protobuf = UsbLocationToProto(location);
  EXPECT_THAT(protobuf, EqualsProto(R"pb(bus: 1 ports: "1.2.3")pb"));
}

TEST(UsbLocationFromProto, MisformattedBusAndPorts) {
  {
    UsbLocationProtobuf protobuf = ParseTextProtoOrDie(R"pb(
      bus: -1 ports: "0"
    )pb");
    EXPECT_FALSE(UsbLocationFromProto(protobuf).has_value());
  }
  {
    UsbLocationProtobuf protobuf = ParseTextProtoOrDie(R"pb(
      bus: 1
      ports: "0.-1.2"
    )pb");
    EXPECT_FALSE(UsbLocationFromProto(protobuf).has_value());
  }
  {
    UsbLocationProtobuf protobuf = ParseTextProtoOrDie(R"pb(
      bus: 1
      ports: "0.1.a"
    )pb");
    EXPECT_FALSE(UsbLocationFromProto(protobuf).has_value());
  }
}

TEST(UsbLocationFromProto, ProperlyFormattedBusAndPorts) {
  {
    UsbLocationProtobuf protobuf = ParseTextProtoOrDie(R"pb(
      bus: 1 ports: "0"
    )pb");
    const auto location = UsbLocationFromProto(protobuf);
    ASSERT_TRUE(location.has_value());
    EXPECT_EQ(location->Bus().value(), 1);
    EXPECT_EQ(location->NumPorts(), 0);
  }
  {
    UsbLocationProtobuf protobuf = ParseTextProtoOrDie(R"pb(
      bus: 1 ports: "1.2"
    )pb");
    const auto location = UsbLocationFromProto(protobuf);
    ASSERT_TRUE(location.has_value());
    EXPECT_EQ(location->Bus().value(), 1);
    ASSERT_EQ(location->NumPorts(), 2);
    EXPECT_EQ(location->Port(0)->value(), 1);
    EXPECT_EQ(location->Port(1)->value(), 2);
  }
}

}  // namespace
}  // namespace ecclesia
