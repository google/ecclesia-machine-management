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

#ifndef ECCLESIA_MAGENT_LIB_NVME_FIRMWARE_SLOT_INFO_H_
#define ECCLESIA_MAGENT_LIB_NVME_FIRMWARE_SLOT_INFO_H_

#include <cstring>
#include <memory>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ecclesia/magent/lib/nvme/nvme_types.emb.h"
#include "runtime/cpp/emboss_cpp_util.h"
#include "runtime/cpp/emboss_prelude.h"

namespace ecclesia {

class FirmwareSlotInfo {
 public:
  // Factory function to parse raw data buffer into FirmwareSlotInfo.
  // Arguments:
  //   buf: Data buffer returned from GetLogPage(Log Identifier 03h).
  static absl::StatusOr<std::unique_ptr<FirmwareSlotInfo>> Parse(
      absl::string_view buf) {
    if (buf.size() != FirmwareSlotInfoFormat::IntrinsicSizeInBytes()) {
      return absl::InternalError("Unexpected error.");
    }
    return absl::WrapUnique(new FirmwareSlotInfo(buf));
  }

  FirmwareSlotInfo(const FirmwareSlotInfo &) = delete;
  FirmwareSlotInfo &operator=(const FirmwareSlotInfo &) = delete;

  virtual ~FirmwareSlotInfo() = default;

  // 7 valid firmware slots 1-7
  enum Slot {
    kSlotInvalid = 0,
    kSlot1 = 1,
    kSlot2 = 2,
    kSlot3 = 3,
    kSlot4 = 4,
    kSlot5 = 5,
    kSlot6 = 6,
    kSlot7 = 7,
    kSlotMax = 8
  };

  // Firmware slot from which the actively running firmware revision was loaded.
  Slot CurrentActiveFirmwareSlot() const {
    return static_cast<Slot>(firmware_slot_info_.afi().Read() & 0x7);
  }
  // Firmware slot that is going to be activated at the next Controller Level
  // Reset. If this value is 0h, then the controller does not indicate the
  // firmware slot that is going to be activated at the next Controller Level
  // Reset.
  Slot NextActiveFirmwareSlot() const {
    return static_cast<Slot>(firmware_slot_info_.afi().Read() >> 4 & 0x7);
  }

  // Returns firmware revisions associated with each firmware slot
  absl::flat_hash_map<Slot, std::string> FirmwareRevisions() const {
    absl::flat_hash_map<Slot, std::string> revisions;
    for (int slot = kSlot1; slot < kSlotMax; ++slot) {
      int index = slot - 1;
      auto revision = firmware_slot_info_.frs()[index]
                          .value()
                          .ToString<absl::string_view>();
      int len = strnlen(revision.data(), revision.size());
      revisions[static_cast<Slot>(slot)] = std::string(revision.data(), len);
    }
    return revisions;
  }

 protected:
  explicit FirmwareSlotInfo(absl::string_view buf)
      : data_(buf), firmware_slot_info_(&data_) {}

 private:
  const std::string data_;
  FirmwareSlotInfoFormatView firmware_slot_info_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_LIB_NVME_FIRMWARE_SLOT_INFO_H_
