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

// Class for reading/writing EEPROMs and getting some metadata about them.
// EEPROMs are non-volatile memory used to store small amounts of (e.g.
// configuration) data.

#ifndef ECCLESIA_MAGENT_LIB_EEPROM_EEPROM_H_
#define ECCLESIA_MAGENT_LIB_EEPROM_EEPROM_H_

#include <cstddef>
#include <cstdlib>

#include "absl/types/optional.h"
#include "absl/types/span.h"

namespace ecclesia {

// This is an abstract base class, which can be subclassed by specific
// types of EEPROM device classes.
class Eeprom {
 public:
  struct ModeType {
    bool readable;
    bool writable;
  };

  struct SizeType {
    enum {
      kFixed = 0,
      kDynamic = 1,
    } type;
    absl::optional<size_t> size;
  };

  Eeprom() {}

  virtual ~Eeprom() = default;

  // Get the device mode.
  virtual ModeType GetMode() const = 0;

  // Get the device size in bytes.
  virtual SizeType GetSize() const = 0;

  // Read value.size() bytes from the eeprom.
  // Returns: number of bytes successfully read or <0 on error.
  virtual absl::optional<int> ReadBytes(
      size_t offset, absl::Span<unsigned char> value) const = 0;

  // Write to the eeprom.
  // Returns: number of bytes successfully written or <0 on error.
  virtual absl::optional<int> WriteBytes(
      size_t offset, absl::Span<const unsigned char> data) const = 0;
};

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_LIB_EEPROM_EEPROM_H_
