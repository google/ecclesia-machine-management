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

#include "ecclesia/magent/lib/eeprom/ipmi_eeprom.h"

#include <optional>

#include "absl/types/span.h"
#include "ecclesia/lib/logging/logging.h"

namespace ecclesia {

IpmiEeprom::IpmiEeprom(IpmiInterface *ipmi, ModeType mode, uint8_t fru_id)
    : ipmi_(ipmi), fru_id_(fru_id), mode_(mode) {
  ReadEepromIntoCache();
}

Eeprom::SizeType IpmiEeprom::GetSize() const {
  return {SizeType::kFixed, cache_.size()};
}
Eeprom::ModeType IpmiEeprom::GetMode() const { return mode_; }
std::optional<int> IpmiEeprom::ReadBytes(
    size_t offset, absl::Span<unsigned char> value) const {
  size_t len = value.size();
  if (offset >= cache_.size() || offset + len > cache_.size() ||
      offset + len < offset) {
    ErrorLog() << absl::StrCat("Couldn't read IPMI FRU ID: ", fru_id_,
                               " bad range.");
    return std::nullopt;
  }
  std::copy(cache_.begin() + offset, cache_.begin() + offset + len,
            value.begin());
  assert(len <= INT_MAX);
  return len;
}
std::optional<int> IpmiEeprom::WriteBytes(
    size_t offset, absl::Span<const unsigned char> data) const {
  return std::nullopt;
}

void IpmiEeprom::ReadEepromIntoCache() {
  // Reset the cache in case we hit an error
  cache_.clear();

  uint16_t size;

  if (absl::Status status = ipmi_->GetFruSize(fru_id_, &size); !status.ok()) {
    ErrorLog() << "Couldn't read IPMI FRU Inventory size FRU ID: "
               << int{fru_id_} << " status: " << status;
    return;
  }

  if (!size) {
    ErrorLog() << "IPMI reported fru size 0. FRU ID;" << int{fru_id_};
    return;
  }

  std::vector<uint8_t> data(size);
  if (absl::Status status =
          ipmi_->ReadFru(fru_id_, /*offset=*/0, absl::MakeSpan(data));
      !status.ok()) {
    ErrorLog() << "Couldn't read IPMI FRU ID: " << int{fru_id_}
               << " size: " << size << " status: " << status;
    return;
  }

  cache_ = std::move(data);
}

}  // namespace ecclesia
