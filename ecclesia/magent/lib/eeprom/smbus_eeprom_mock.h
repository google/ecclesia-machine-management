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

#ifndef ECCLESIA_MAGENT_LIB_EEPROM_SMBUS_EEPROM_MOCK_H_
#define ECCLESIA_MAGENT_LIB_EEPROM_SMBUS_EEPROM_MOCK_H_

#include <cstddef>
#include <cstring>
#include <utility>

#include "gmock/gmock.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "ecclesia/magent/lib/eeprom/smbus_eeprom.h"

namespace ecclesia {

class MockSmbusEeprom : public SmbusEeprom {
 public:
  explicit MockSmbusEeprom(Option option) : SmbusEeprom(std::move(option)) {}
  MOCK_METHOD(absl::optional<int>, ReadBytes,
              (size_t, absl::Span<unsigned char>), (const, override));
  MOCK_METHOD(absl::optional<int>, WriteBytes,
              (size_t, absl::Span<const unsigned char>), (const, override));
};

// This is to faciliate mocking up ReadBytes method.
ACTION_P(SmbusEepromReadBytes, data) {
  unsigned char *output_data = arg1.data();
  const size_t len = arg1.size();
  memcpy(output_data, data, len);
  return len;
}
}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_LIB_EEPROM_SMBUS_EEPROM_MOCK_H_
