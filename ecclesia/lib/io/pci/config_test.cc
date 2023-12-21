/*
 * Copyright 2023 Google LLC
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

#include "ecclesia/lib/io/pci/config.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ecclesia/lib/io/pci/location.h"
#include "ecclesia/lib/io/pci/region.h"
#include "ecclesia/lib/io/pci/signature.h"
#include "ecclesia/lib/testing/status.h"

namespace ecclesia {
namespace {

using ::testing::IsEmpty;
using ::testing::Return;
using ::testing::SizeIs;

class MockPciRegion : public PciRegion {
 public:
  MockPciRegion() : PciRegion(kPciSize) {}

  MOCK_METHOD(absl::StatusOr<uint8_t>, Read8, (OffsetType offset),
              (const, override));
  MOCK_METHOD(absl::Status, Write8, (OffsetType offset, uint8_t data),
              (override));

  MOCK_METHOD(absl::StatusOr<uint16_t>, Read16, (OffsetType offset),
              (const, override));
  MOCK_METHOD(absl::Status, Write16, (OffsetType offset, uint16_t data),
              (override));

  MOCK_METHOD(absl::StatusOr<uint32_t>, Read32, (OffsetType offset),
              (const, override));
  MOCK_METHOD(absl::Status, Write32, (OffsetType offset, uint32_t data),
              (override));

 private:
  static constexpr size_t kPciSize = 4096;
};

class PciConfigTest : public testing::Test {
 protected:
  void ExpectRead8(uint8_t offset, uint8_t value,
                   uint8_t expected_call_count = 1) {
    EXPECT_CALL(region_, Read8(offset))
        .Times(expected_call_count)
        .WillRepeatedly(Return(value));
  }
  void ExpectFailedRead8(uint8_t offset, uint8_t expected_call_count = 1) {
    EXPECT_CALL(region_, Read8(offset))
        .Times(expected_call_count)
        .WillRepeatedly(Return(absl::InternalError("Failed to Read8")));
  }

  void ExpectRead16(uint8_t offset, uint16_t value,
                    uint8_t expected_call_count = 1) {
    EXPECT_CALL(region_, Read16(offset))
        .Times(expected_call_count)
        .WillRepeatedly(Return(value));
  }
  void ExpectFailedRead16(uint8_t offset, uint8_t expected_call_count = 1) {
    EXPECT_CALL(region_, Read16(offset))
        .Times(expected_call_count)
        .WillRepeatedly(Return(absl::InternalError("Failed to Read16")));
  }

  void ExpectRead32(uint8_t offset, uint32_t value,
                    uint8_t expected_call_count = 1) {
    EXPECT_CALL(region_, Read32(offset))
        .Times(expected_call_count)
        .WillRepeatedly(Return(value));
    ;
  }
  void ExpectFailedRead32(uint8_t offset, uint8_t expected_call_count = 1) {
    EXPECT_CALL(region_, Read32(offset))
        .Times(expected_call_count)
        .WillRepeatedly(Return(absl::InternalError("Failed to Read32")));
  }

  MockPciRegion region_;
};

TEST(PciConfigSpaceHelperTest, NumToPcieLinkSpeed) {
  EXPECT_EQ(NumToPcieLinkSpeed(2.5), PcieLinkSpeed::kGen1Speed2500MT);
  EXPECT_EQ(NumToPcieLinkSpeed(5), PcieLinkSpeed::kGen2Speed5GT);
  EXPECT_EQ(NumToPcieLinkSpeed(8), PcieLinkSpeed::kGen3Speed8GT);
  EXPECT_EQ(NumToPcieLinkSpeed(16), PcieLinkSpeed::kGen4Speed16GT);
  EXPECT_EQ(NumToPcieLinkSpeed(32), PcieLinkSpeed::kGen5Speed32GT);
  EXPECT_EQ(NumToPcieLinkSpeed(99), PcieLinkSpeed::kUnknown);
}

TEST(PciConfigSpaceHelperTest, PcieGenToLinkSpeed) {
  EXPECT_EQ(PcieGenToLinkSpeed("Gen1"), PcieLinkSpeed::kGen1Speed2500MT);
  EXPECT_EQ(PcieGenToLinkSpeed("Gen2"), PcieLinkSpeed::kGen2Speed5GT);
  EXPECT_EQ(PcieGenToLinkSpeed("Gen3"), PcieLinkSpeed::kGen3Speed8GT);
  EXPECT_EQ(PcieGenToLinkSpeed("Gen4"), PcieLinkSpeed::kGen4Speed16GT);
  EXPECT_EQ(PcieGenToLinkSpeed("Gen5"), PcieLinkSpeed::kGen5Speed32GT);
  EXPECT_EQ(PcieGenToLinkSpeed("Gen6"), PcieLinkSpeed::kUnknown);
}

TEST(PciConfigSpaceHelperTest, PcieLinkSpeedToMts) {
  EXPECT_EQ(PcieLinkSpeedToMts(PcieLinkSpeed::kGen1Speed2500MT), 2500);
  EXPECT_EQ(PcieLinkSpeedToMts(PcieLinkSpeed::kGen2Speed5GT), 5000);
  EXPECT_EQ(PcieLinkSpeedToMts(PcieLinkSpeed::kGen3Speed8GT), 8000);
  EXPECT_EQ(PcieLinkSpeedToMts(PcieLinkSpeed::kGen4Speed16GT), 16000);
  EXPECT_EQ(PcieLinkSpeedToMts(PcieLinkSpeed::kGen5Speed32GT), 32000);
  EXPECT_EQ(PcieLinkSpeedToMts(PcieLinkSpeed::kUnknown), 0);
}

TEST(PciConfigSpaceHelperTest, NumToPcieLinkWidth) {
  EXPECT_EQ(NumToPcieLinkWidth(1), PcieLinkWidth::kWidth1);
  EXPECT_EQ(NumToPcieLinkWidth(2), PcieLinkWidth::kWidth2);
  EXPECT_EQ(NumToPcieLinkWidth(4), PcieLinkWidth::kWidth4);
  EXPECT_EQ(NumToPcieLinkWidth(8), PcieLinkWidth::kWidth8);
  EXPECT_EQ(NumToPcieLinkWidth(12), PcieLinkWidth::kWidth12);
  EXPECT_EQ(NumToPcieLinkWidth(16), PcieLinkWidth::kWidth16);
  EXPECT_EQ(NumToPcieLinkWidth(32), PcieLinkWidth::kWidth32);
  EXPECT_EQ(NumToPcieLinkWidth(99), PcieLinkWidth::kUnknown);
}

TEST(PciConfigSpaceHelperTest, PcieLinkWidthToInt) {
  EXPECT_EQ(PcieLinkWidthToInt(PcieLinkWidth::kWidth1), 1);
  EXPECT_EQ(PcieLinkWidthToInt(PcieLinkWidth::kWidth2), 2);
  EXPECT_EQ(PcieLinkWidthToInt(PcieLinkWidth::kWidth4), 4);
  EXPECT_EQ(PcieLinkWidthToInt(PcieLinkWidth::kWidth8), 8);
  EXPECT_EQ(PcieLinkWidthToInt(PcieLinkWidth::kWidth12), 12);
  EXPECT_EQ(PcieLinkWidthToInt(PcieLinkWidth::kWidth16), 16);
  EXPECT_EQ(PcieLinkWidthToInt(PcieLinkWidth::kWidth32), 32);
  EXPECT_EQ(PcieLinkWidthToInt(PcieLinkWidth::kUnknown), 0);
}

TEST_F(PciConfigTest, PciCapabilityIdSuccess) {
  PciCapability cap(&region_, /*offset=*/64);
  ExpectRead8(64, 1);
  ASSERT_THAT(cap.CapabilityId(), IsOkAndHolds(1));
}

TEST_F(PciConfigTest, PciCapabilityIdFailure) {
  PciCapability cap(&region_, /*offset=*/64);
  ExpectFailedRead8(64);
  ASSERT_THAT(cap.CapabilityId(), IsStatusInternal());
}

TEST_F(PciConfigTest, PciCapabilityNextPtrSuccess) {
  PciCapability cap(&region_, /*offset=*/64);
  ExpectRead8(65, 1);
  ASSERT_THAT(cap.NextCapabilityPointer(), IsOkAndHolds(1));
}

TEST_F(PciConfigTest, PciCapabilityNextPtrFailure) {
  PciCapability cap(&region_, /*offset=*/64);
  ExpectFailedRead8(65);
  ASSERT_THAT(cap.NextCapabilityPointer(), IsStatusInternal());
}

TEST_F(PciConfigTest, PciCapabilityNextCapabilitySuccess) {
  PciCapability cap(&region_, /*offset=*/64);
  ExpectRead8(65, 68);
  ExpectRead8(68, 2);
  absl::StatusOr<PciCapability> next_cap = cap.NextCapability();
  ASSERT_THAT(next_cap, IsOk());
  EXPECT_THAT(next_cap->CapabilityId(), IsOkAndHolds(2));
}

TEST_F(PciConfigTest, PciCapabilityNextCapabilityFailure) {
  PciCapability cap(&region_, /*offset=*/64);
  ExpectFailedRead8(65);
  ASSERT_THAT(cap.NextCapability(), IsStatusInternal());
}

TEST_F(PciConfigTest, PciCapabilityGetIfSuccess) {
  PciCapability cap(&region_, /*offset=*/64);
  ExpectRead8(64, 0x0d);
  absl::StatusOr<PciSubsystemCapability> subsystem_cap =
      cap.GetIf<PciSubsystemCapability>();
  ASSERT_THAT(subsystem_cap, IsOk());
  EXPECT_EQ(subsystem_cap->kCapabilityId,
            PciSubsystemCapability::kCapabilityId);
}

TEST_F(PciConfigTest, PciCapabilityGetIfFailure) {
  PciCapability cap(&region_, /*offset=*/64);
  ExpectRead8(64, 1);
  absl::StatusOr<PciSubsystemCapability> subsystem_cap =
      cap.GetIf<PciSubsystemCapability>();
  ASSERT_THAT(subsystem_cap, IsStatusNotFound());
}

TEST_F(PciConfigTest, PciExpressCapabilityReadLinkCapabilitiesSuccess) {
  PciCapability cap(&region_, /*offset=*/64);
  PciExpressCapability pcie_cap(cap);

  const uint32_t kExpectedCap =
      static_cast<uint8_t>(PcieLinkSpeed::kGen3Speed8GT) |
      (static_cast<uint8_t>(PcieLinkWidth::kWidth4) << 4) | (true << 20) |
      (244 << 24);

  ExpectRead32(64 + PciExpressCapability::kLinkCapsOffset, kExpectedCap);
  absl::StatusOr<PciExpressCapability::LinkCapabilities> capabilities =
      pcie_cap.ReadLinkCapabilities();
  ASSERT_THAT(capabilities, IsOk());
  EXPECT_EQ(capabilities->max_speed, PcieLinkSpeed::kGen3Speed8GT);
  EXPECT_EQ(capabilities->max_width, PcieLinkWidth::kWidth4);
  EXPECT_TRUE(capabilities->dll_active_capable);
  EXPECT_EQ(capabilities->port_number, 244);
}

TEST_F(PciConfigTest, PciExpressCapabilityReadLinkCapabilitiesFailure) {
  PciCapability cap(&region_, /*offset=*/64);
  PciExpressCapability pcie_cap(cap);

  ExpectFailedRead32(64 + PciExpressCapability::kLinkCapsOffset);
  EXPECT_THAT(pcie_cap.ReadLinkCapabilities(), IsStatusInternal());
}

TEST_F(PciConfigTest, PciExpressCapabilityReadLinkStatusSuccess) {
  PciCapability cap(&region_, /*offset=*/64);
  PciExpressCapability pcie_cap(cap);

  const uint16_t kExpectedStatus =
      static_cast<uint8_t>(PcieLinkSpeed::kGen4Speed16GT) |
      (static_cast<uint8_t>(PcieLinkWidth::kWidth12) << 4) | (true << 13);

  ExpectRead16(64 + PciExpressCapability::kLinkStatusOffset, kExpectedStatus);
  absl::StatusOr<PciExpressCapability::LinkStatus> link_status =
      pcie_cap.ReadLinkStatus();
  ASSERT_THAT(link_status, IsOk());
  EXPECT_EQ(link_status->current_speed, PcieLinkSpeed::kGen4Speed16GT);
  EXPECT_EQ(link_status->current_width, PcieLinkWidth::kWidth12);
  EXPECT_TRUE(link_status->dll_active);
}

TEST_F(PciConfigTest, PciExpressCapabilityReadLinkStatusFailure) {
  PciCapability cap(&region_, /*offset=*/64);
  PciExpressCapability pcie_cap(cap);

  ExpectFailedRead16(64 + PciExpressCapability::kLinkStatusOffset);
  EXPECT_THAT(pcie_cap.ReadLinkStatus(), IsStatusInternal());
}

TEST_F(PciConfigTest, PciConfigSpaceBaseSignatureSuccess) {
  PciConfigSpace config_space(&region_);
  ExpectRead16(PciConfigSpace::kVidOffset, 1);
  ExpectRead16(PciConfigSpace::kDidOffset, 2);
  PciBaseSignature expected_signature = PciBaseSignature::Make<1, 2>();
  ASSERT_THAT(config_space.BaseSignature(), IsOkAndHolds(expected_signature));
}

TEST_F(PciConfigTest, PciConfigSpaceBaseSignatureFailure) {
  PciConfigSpace config_space(&region_);
  {
    ExpectRead16(PciConfigSpace::kVidOffset, 1);
    ExpectFailedRead16(PciConfigSpace::kDidOffset);
    EXPECT_THAT(config_space.BaseSignature(), IsStatusInternal());
  }
  {
    ExpectFailedRead16(PciConfigSpace::kVidOffset);
    EXPECT_THAT(config_space.BaseSignature(), IsStatusInternal());
  }
}

TEST_F(PciConfigTest, PciConfigSpaceSubsystemSignatureSuccess) {
  PciConfigSpace config_space(&region_);
  // First read is for header type.
  ExpectRead8(PciConfigSpace::kHeaderTypeOffset,
              PciType0ConfigSpace::kHeaderType);
  ExpectRead16(PciType0ConfigSpace::kSubsysVendorIdOffset, 1);
  ExpectRead16(PciType0ConfigSpace::kSubsysIdOffset, 2);
  PciSubsystemSignature expected_signature =
      PciSubsystemSignature::Make<1, 2>();
  ASSERT_THAT(config_space.SubsystemSignature(),
              IsOkAndHolds(expected_signature));
}

TEST_F(PciConfigTest, PciConfigSpaceSubsystemSignatureFailure) {
  PciConfigSpace config_space(&region_);
  {
    ExpectFailedRead8(PciConfigSpace::kHeaderTypeOffset);
    EXPECT_THAT(config_space.SubsystemSignature(), IsStatusInternal());
  }
  {
    ExpectRead8(PciConfigSpace::kHeaderTypeOffset,
                PciType0ConfigSpace::kHeaderType);
    ExpectRead16(PciType0ConfigSpace::kSubsysVendorIdOffset, 1);
    ExpectFailedRead16(PciType0ConfigSpace::kSubsysIdOffset);
    EXPECT_THAT(config_space.SubsystemSignature(), IsStatusInternal());
  }
  {
    ExpectRead8(PciConfigSpace::kHeaderTypeOffset,
                PciType0ConfigSpace::kHeaderType);
    ExpectFailedRead16(PciType0ConfigSpace::kSubsysVendorIdOffset);
    EXPECT_THAT(config_space.SubsystemSignature(), IsStatusInternal());
  }
}

TEST_F(PciConfigTest,
       PciConfigSpaceSubsystemSignatureWithType1HeaderTypeSuccess) {
  PciConfigSpace config_space(&region_);
  // 8-bit reads are to get capabilities.
  ExpectRead8(PciConfigSpace::kCapPointerOffset, 64);
  ExpectRead8(64, PciSubsystemCapability::kCapabilityId);
  ExpectRead8(65, 0);

  ExpectRead16(64 + PciSubsystemCapability::kSsvidOffset, 1);
  ExpectRead16(64 + PciSubsystemCapability::kSsidOffset, 2);
  PciSubsystemSignature expected_signature =
      PciSubsystemSignature::Make<1, 2>();
  ASSERT_THAT(config_space.SubsystemSignature(PciType1ConfigSpace::kHeaderType),
              IsOkAndHolds(expected_signature));
}

TEST_F(PciConfigTest,
       PciConfigSpaceSubsystemSignatureWithType1HeaderTypeFailure) {
  PciConfigSpace config_space(&region_);
  {
    ExpectFailedRead8(PciConfigSpace::kCapPointerOffset);
    EXPECT_THAT(
        config_space.SubsystemSignature(PciType1ConfigSpace::kHeaderType),
        IsStatusInternal());
  }
  {
    ExpectRead8(PciConfigSpace::kCapPointerOffset, 64);
    ExpectRead8(64, PciSubsystemCapability::kCapabilityId);
    ExpectRead8(65, 0);

    ExpectRead16(64 + PciSubsystemCapability::kSsvidOffset, 1);
    ExpectFailedRead16(64 + PciSubsystemCapability::kSsidOffset);
    EXPECT_THAT(
        config_space.SubsystemSignature(PciType1ConfigSpace::kHeaderType),
        IsStatusInternal());
  }
  {
    ExpectRead8(PciConfigSpace::kCapPointerOffset, 64);
    ExpectRead8(64, PciSubsystemCapability::kCapabilityId);
    ExpectRead8(65, 0);

    ExpectFailedRead16(64 + PciSubsystemCapability::kSsvidOffset);
    EXPECT_THAT(
        config_space.SubsystemSignature(PciType1ConfigSpace::kHeaderType),
        IsStatusInternal());
  }
}

TEST_F(PciConfigTest, PciConfigSpaceClassCodeSuccess) {
  PciConfigSpace config_space(&region_);
  ExpectRead32(PciConfigSpace::kRevisionIdOffset, 699);
  EXPECT_THAT(config_space.ClassCode(), IsOkAndHolds(2));
}

TEST_F(PciConfigTest, PciConfigSpaceClassCodeFailure) {
  PciConfigSpace config_space(&region_);
  ExpectFailedRead32(PciConfigSpace::kRevisionIdOffset);
  EXPECT_THAT(config_space.ClassCode(), IsStatusInternal());
}

TEST_F(PciConfigTest, PciConfigSpaceHeaderTypeSuccess) {
  PciConfigSpace config_space(&region_);
  ExpectRead8(PciConfigSpace::kHeaderTypeOffset, 99);
  EXPECT_THAT(config_space.HeaderType(), IsOkAndHolds(99));
}

TEST_F(PciConfigTest, PciConfigSpaceHeaderTypeFailure) {
  PciConfigSpace config_space(&region_);
  ExpectFailedRead8(PciConfigSpace::kHeaderTypeOffset);
  EXPECT_THAT(config_space.HeaderType(), IsStatusInternal());
}

TEST_F(PciConfigTest, PciConfigSpaceForEachCapabilityNoCapabilities) {
  PciConfigSpace config_space(&region_);
  ExpectRead8(52, 0);

  std::vector<PciCapability> capabilities;
  absl::Status status = config_space.ForEachCapability(
      [&capabilities](const PciCapability &cap) -> absl::Status {
        capabilities.push_back(cap);
        return absl::OkStatus();
      });

  ASSERT_THAT(status, IsOk());
  EXPECT_THAT(capabilities, IsEmpty());
}

TEST_F(PciConfigTest, PciConfigSpaceForEachCapabilitySuccess) {
  PciConfigSpace config_space(&region_);

  // Initial read will always be for the first "next capability ptr" offset at
  // byte 52, and should point to 64.
  ExpectRead8(PciConfigSpace::kCapPointerOffset, 64);

  for (uint8_t i = 0; i < PciConfigSpace::kMaxPciCapabilities; i++) {
    uint8_t offset = 64 + (4 * i);
    // Read Capability ID. Return value of i+1 as a dummy value.
    ExpectRead8(offset, i + 1);
    // Read next capability ptr, always the byte offset that follows the ID.
    if (i + 1 != PciConfigSpace::kMaxPciCapabilities) {
      ExpectRead8(offset + 1, offset + 4);
    } else {
      // If on the last iteration, return 0 to signal no further capabilities.
      ExpectRead8(offset + 1, 0);
    }
  }

  std::vector<PciCapability> capabilities;
  absl::Status status = config_space.ForEachCapability(
      [&capabilities](const PciCapability &cap) -> absl::Status {
        capabilities.push_back(cap);
        return absl::OkStatus();
      });

  ASSERT_THAT(status, IsOk());
  ASSERT_THAT(capabilities, SizeIs(PciConfigSpace::kMaxPciCapabilities));
  for (uint8_t i = 0; i < PciConfigSpace::kMaxPciCapabilities; i++) {
    EXPECT_THAT(capabilities[i].CapabilityId(), IsOkAndHolds(i + 1));
  }
}

TEST_F(PciConfigTest, PciConfigSpaceForEachCapabilityExceedsMaxCapabilities) {
  PciConfigSpace config_space(&region_);

  // Initial read will always be for the first "next capability ptr" offset at
  // byte 52, and should point to 64.
  ExpectRead8(PciConfigSpace::kCapPointerOffset, 64);

  for (uint8_t i = 0; i < PciConfigSpace::kMaxPciCapabilities; i++) {
    uint8_t offset = 64 + (4 * i);
    // Read next capability ptr, always the byte offset that follows the ID.
    if (i + 1 != PciConfigSpace::kMaxPciCapabilities) {
      ExpectRead8(offset + 1, offset + 4);
    } else {
      // Because we want to simulate a PCI that erroneously reports more than 48
      // capabilities, set the "next ptr" for the 48th capability to point to
      // offset 4. This should never happen, since bytes 0-63 are reserved for
      // the Config Space header but by the time we've read this capability,
      // we've used up all 192 bytes allocated to the capabilities list. Thus,
      // this simulates a malformed PCI capability pointing to an invalid offset
      // in a way that allows us to fake 49 linked list elements.
      ExpectRead8(offset + 1, 4);
    }
  }

  std::vector<PciCapability> capabilities;
  absl::Status status = config_space.ForEachCapability(
      [&capabilities](const PciCapability &cap) -> absl::Status {
        capabilities.push_back(cap);
        return absl::OkStatus();
      });

  ASSERT_THAT(status, IsStatusInternal());
  ASSERT_THAT(capabilities, SizeIs(48));
}

TEST_F(PciConfigTest,
       PciConfigSpaceForEachCapabilityCallbackErrorStopsIteration) {
  PciConfigSpace config_space(&region_);

  // Setup expected reads such that we *don't* expect a read for register 65 to
  // get the next ptr of the first capability. Initial ptr.
  ExpectRead8(52, 64);
  // Capability ID.
  ExpectRead8(64, 1);

  std::vector<PciCapability> capabilities;
  absl::Status status = config_space.ForEachCapability(
      [&capabilities](const PciCapability &cap) -> absl::Status {
        capabilities.push_back(cap);
        // Return an error after our first capability is read.
        return absl::UnknownError("error");
      });

  EXPECT_THAT(status, IsStatusUnknown());
  // The first read capability should still exist in the vector.
  ASSERT_THAT(capabilities, SizeIs(1));
  EXPECT_THAT(capabilities[0].CapabilityId(), IsOkAndHolds(1));
}

TEST_F(PciConfigTest,
       PciConfigSpaceForEachCapabilityCircularListHandledGracefully) {
  PciConfigSpace config_space(&region_);

  // Setup a malformed, circular linked list of PCI capabilities.
  // Initial ptr.
  ExpectRead8(PciConfigSpace::kCapPointerOffset, 64);
  // Capability ID.
  ExpectRead8(64, 1);
  // Next ptr that induces circular list because `CapabilityPointerToOffset`
  // changes 66 -> 64, which we've already visited.
  ExpectRead8(65, 66);

  std::vector<PciCapability> capabilities;
  absl::Status status = config_space.ForEachCapability(
      [&capabilities](const PciCapability &cap) -> absl::Status {
        capabilities.push_back(cap);
        return absl::OkStatus();
      });

  EXPECT_THAT(status, IsStatusInternal());
  ASSERT_THAT(capabilities, SizeIs(1));
  EXPECT_THAT(capabilities[0].CapabilityId(), IsOkAndHolds(1));
}

TEST_F(PciConfigTest, PciType1ConfigSpaceSecondaryBusNumSuccess) {
  PciConfigSpace config_space(&region_);
  PciType1ConfigSpace type1_config_space(config_space);

  ExpectRead8(PciType1ConfigSpace::kSecBusNumOffset, 3);
  const PciBusNum kExpectedBus = PciBusNum::Make<3>();
  EXPECT_THAT(type1_config_space.SecondaryBusNum(), IsOkAndHolds(kExpectedBus));
}

TEST_F(PciConfigTest, PciType1ConfigSpaceSecondaryBusNumFailure) {
  PciConfigSpace config_space(&region_);
  PciType1ConfigSpace type1_config_space(config_space);

  ExpectFailedRead8(PciType1ConfigSpace::kSecBusNumOffset);
  EXPECT_THAT(type1_config_space.SecondaryBusNum(), IsStatusInternal());
}

TEST_F(PciConfigTest, PciType1ConfigSpaceSubordinateBusNumSuccess) {
  PciConfigSpace config_space(&region_);
  PciType1ConfigSpace type1_config_space(config_space);

  ExpectRead8(PciType1ConfigSpace::kSubBusNumOffset, 22);
  const PciBusNum kExpectedBus = PciBusNum::Make<22>();
  EXPECT_THAT(type1_config_space.SubordinateBusNum(),
              IsOkAndHolds(kExpectedBus));
}

TEST_F(PciConfigTest, PciType1ConfigSpaceSubordinateBusNumFailure) {
  PciConfigSpace config_space(&region_);
  PciType1ConfigSpace type1_config_space(config_space);

  ExpectFailedRead8(PciType1ConfigSpace::kSubBusNumOffset);
  EXPECT_THAT(type1_config_space.SubordinateBusNum(), IsStatusInternal());
}

}  // namespace
}  // namespace ecclesia
