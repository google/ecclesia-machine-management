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

#include "ecclesia/magent/lib/nvme/firmware_slot_info.h"

#include <memory>
#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "ecclesia/magent/lib/nvme/nvme_types.emb.h"

namespace ecclesia {
namespace {
constexpr unsigned char
    kFirmwareSlotInfoData[FirmwareSlotInfoFormat::IntrinsicSizeInBytes()] = {
        /*0x0000*/ 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*0x0008*/ 0x4d, 0x50, 0x4b, 0x44, 0x30, 0x50, 0x31, 0x35,
        /*0x0010*/ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*0x0018*/ 0x4d, 0x50, 0x4b, 0x44, 0x30, 0x50, 0x31, 0x00,
};

using ::testing::Pair;
using ::testing::UnorderedElementsAre;

TEST(FirmwareSlotInfoTest, ParseFirmwareSlotInfo) {
  auto ret = FirmwareSlotInfo::Parse(std::string(
      kFirmwareSlotInfoData,
      kFirmwareSlotInfoData + FirmwareSlotInfoFormat::IntrinsicSizeInBytes()));
  ASSERT_TRUE(ret.ok());

  std::unique_ptr<FirmwareSlotInfo> firmware_info = std::move(ret.value());
  ASSERT_TRUE(firmware_info.get());
  EXPECT_EQ(firmware_info->CurrentActiveFirmwareSlot(), 1);
  EXPECT_EQ(firmware_info->NextActiveFirmwareSlot(), 1);
  absl::flat_hash_map<FirmwareSlotInfo::Slot, std::string> revisions =
      firmware_info->FirmwareRevisions();

  EXPECT_THAT(revisions,
              UnorderedElementsAre(Pair(FirmwareSlotInfo::kSlot1, "MPKD0P15"),
                                   Pair(FirmwareSlotInfo::kSlot2, ""),
                                   Pair(FirmwareSlotInfo::kSlot3, "MPKD0P1"),
                                   Pair(FirmwareSlotInfo::kSlot4, ""),
                                   Pair(FirmwareSlotInfo::kSlot5, ""),
                                   Pair(FirmwareSlotInfo::kSlot6, ""),
                                   Pair(FirmwareSlotInfo::kSlot7, "")));
}

}  // namespace
}  // namespace ecclesia
