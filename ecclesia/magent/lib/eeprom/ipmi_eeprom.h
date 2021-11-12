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

#ifndef ECCLESIA_MAGENT_LIB_EEPROM_IPMI_EEPROM_H_
#define ECCLESIA_MAGENT_LIB_EEPROM_IPMI_EEPROM_H_

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <utility>

#include "absl/types/span.h"
#include "ecclesia/magent/lib/eeprom/eeprom.h"
#include "ecclesia/magent/lib/ipmi/ipmi.h"

namespace ecclesia {

// An Eeprom interface which reads data via IPMI. Subclass should implement
// ReadBytes and WriteBytes.
class IpmiEeprom : public Eeprom {
 public:
  explicit IpmiEeprom(IpmiInterface *ipmi, ModeType mode, uint8_t fru_id);
  IpmiEeprom(const IpmiEeprom &) = delete;
  IpmiEeprom &operator=(const IpmiEeprom &) = delete;

  Eeprom::SizeType GetSize() const override;
  Eeprom::ModeType GetMode() const override;
  std::optional<int> ReadBytes(size_t offset,
                               absl::Span<unsigned char> value) const override;
  std::optional<int> WriteBytes(
      size_t offset, absl::Span<const unsigned char> data) const override;

 private:
  // Updates cache_ by reading the entire FRU contents over IPMI.
  void ReadEepromIntoCache();

  IpmiInterface *const ipmi_;
  const uint8_t fru_id_;
  ModeType mode_;
  std::vector<uint8_t> cache_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_LIB_EEPROM_IPMI_EEPROM_H_
