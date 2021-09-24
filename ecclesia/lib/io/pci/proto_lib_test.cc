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

#include "ecclesia/lib/io/pci/proto_lib.h"

#include <optional>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "ecclesia/lib/io/pci/location.h"
#include "ecclesia/lib/io/pci/pci.pb.h"
#include "ecclesia/lib/io/pci/signature.h"
#include "ecclesia/lib/protobuf/parse.h"
#include "ecclesia/lib/testing/proto.h"

namespace ecclesia {
namespace {

using ::testing::Eq;
using ::testing::Optional;

TEST(PciLocationToProto, SuccessfulConversions) {
  auto zero = PciLocationToProto(PciDbdfLocation::Make<0, 0, 0, 0>());
  PciLocationProtobuf zero_expected =
      ParseTextProtoOrDie("domain: 0 bus: 0 device: 0 function: 0");
  EXPECT_THAT(zero, EqualsProto("domain: 0 bus: 0 device: 0 function: 0"));

  auto nonzero = PciLocationToProto(PciDbdfLocation::Make<1, 2, 3, 4>());
  EXPECT_THAT(nonzero, EqualsProto("domain: 1 bus: 2 device: 3 function: 4"));
}

TEST(PciLocationFromProto, SuccessfulConversions) {
  PciLocationProtobuf zero =
      ParseTextProtoOrDie("domain: 0 bus: 0 device: 0 function: 0");
  EXPECT_THAT(PciLocationFromProto(zero),
              Optional(PciDbdfLocation::Make<0, 0, 0, 0>()));

  PciLocationProtobuf nonzero =
      ParseTextProtoOrDie("domain: 1 bus: 2 device: 3 function: 4");
  EXPECT_THAT(PciLocationFromProto(nonzero),
              Optional(PciDbdfLocation::Make<1, 2, 3, 4>()));
}

TEST(PciLocationFromProto, FailedConversions) {
  PciLocationProtobuf bad_domain =
      ParseTextProtoOrDie("domain: 0x10000 bus: 2 device: 3 function: 4");
  EXPECT_THAT(PciLocationFromProto(bad_domain), Eq(std::nullopt));

  PciLocationProtobuf bad_bus =
      ParseTextProtoOrDie("domain: 1 bus: 0x100 device: 3 function: 4");
  EXPECT_THAT(PciLocationFromProto(bad_bus), Eq(std::nullopt));

  PciLocationProtobuf bad_device =
      ParseTextProtoOrDie("domain: 1 bus: 2 device: 0x20 function: 4");
  EXPECT_THAT(PciLocationFromProto(bad_device), Eq(std::nullopt));

  PciLocationProtobuf bad_function =
      ParseTextProtoOrDie("domain: 1 bus: 2 device: 3 function: 0x8");
  EXPECT_THAT(PciLocationFromProto(bad_function), Eq(std::nullopt));

  PciLocationProtobuf all_bad =
      ParseTextProtoOrDie("domain: -1 bus: -1 device: -1 function: -1");
  EXPECT_THAT(PciLocationFromProto(all_bad), Eq(std::nullopt));
}

TEST(PciBaseSignatureToProto, SuccessfulConversions) {
  auto zero = PciBaseSignatureToProto(PciBaseSignature::Make<0, 0>());
  EXPECT_THAT(zero, EqualsProto("vendor_id: 0 device_id: 0"));

  auto nonzero =
      PciBaseSignatureToProto(PciBaseSignature::Make<0x1ae0, 0x0021>());
  EXPECT_THAT(nonzero, EqualsProto("vendor_id: 0x1ae0 device_id: 0x0021"));
}

TEST(PciBaseSignatureFromProto, SuccessfulConversions) {
  PciBaseSignatureProtobuf zero =
      ParseTextProtoOrDie("vendor_id: 0 device_id: 0");
  EXPECT_THAT(PciBaseSignatureFromProto(zero),
              Optional(PciBaseSignature::Make<0, 0>()));

  PciBaseSignatureProtobuf nonzero =
      ParseTextProtoOrDie("vendor_id: 0x1ae0 device_id: 0x0021");
  EXPECT_THAT(PciBaseSignatureFromProto(nonzero),
              Optional(PciBaseSignature::Make<0x1ae0, 0x0021>()));
}

TEST(PciBaseSignatureFromProto, FailedConversions) {
  PciBaseSignatureProtobuf bad_vid =
      ParseTextProtoOrDie("vendor_id: 0x10000 device_id: 0x0021");
  EXPECT_THAT(PciBaseSignatureFromProto(bad_vid), Eq(std::nullopt));

  PciBaseSignatureProtobuf bad_did =
      ParseTextProtoOrDie("vendor_id: 0x1ae0 device_id: 0x10000");
  EXPECT_THAT(PciBaseSignatureFromProto(bad_did), Eq(std::nullopt));

  PciBaseSignatureProtobuf all_bad =
      ParseTextProtoOrDie("vendor_id: -1 device_id: -1");
  EXPECT_THAT(PciBaseSignatureFromProto(all_bad), Eq(std::nullopt));
}

TEST(PciSubsystemSignatureToProto, SuccessfulConversions) {
  auto zero = PciSubsystemSignatureToProto(PciSubsystemSignature::Make<0, 0>());
  EXPECT_THAT(zero, EqualsProto("vendor_id: 0 id: 0"));

  auto nonzero = PciSubsystemSignatureToProto(
      PciSubsystemSignature::Make<0x1ae0, 0x0045>());
  EXPECT_THAT(nonzero, EqualsProto("vendor_id: 0x1ae0 id: 0x0045"));
}

TEST(PciSubsystemSignatureFromProto, SuccessfulConversions) {
  PciSubsystemSignatureProtobuf zero =
      ParseTextProtoOrDie("vendor_id: 0 id: 0");
  EXPECT_THAT(PciSubsystemSignatureFromProto(zero),
              Optional(PciSubsystemSignature::Make<0, 0>()));

  PciSubsystemSignatureProtobuf nonzero =
      ParseTextProtoOrDie("vendor_id: 0x1ae0 id: 0x0045");
  EXPECT_THAT(PciSubsystemSignatureFromProto(nonzero),
              Optional(PciSubsystemSignature::Make<0x1ae0, 0x0045>()));
}

TEST(PciSubsystemSignatureFromProto, FailedConversions) {
  PciSubsystemSignatureProtobuf bad_ssvid =
      ParseTextProtoOrDie("vendor_id: 0x10000 id: 0x0045");
  EXPECT_THAT(PciSubsystemSignatureFromProto(bad_ssvid), Eq(std::nullopt));

  PciSubsystemSignatureProtobuf bad_ssid =
      ParseTextProtoOrDie("vendor_id: 0x1ae0 id: 0x10000");
  EXPECT_THAT(PciSubsystemSignatureFromProto(bad_ssid), Eq(std::nullopt));

  PciSubsystemSignatureProtobuf all_bad =
      ParseTextProtoOrDie("vendor_id: -1 id: -1");
  EXPECT_THAT(PciSubsystemSignatureFromProto(all_bad), Eq(std::nullopt));
}

}  // namespace
}  // namespace ecclesia
