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

// A class for reading SMBus compatible I2C EEPROMs.

#ifndef ECCLESIA_MAGENT_LIB_EEPROM_SMBUS_EEPROM_H_
#define ECCLESIA_MAGENT_LIB_EEPROM_SMBUS_EEPROM_H_

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "ecclesia/lib/io/smbus/smbus.h"
#include "ecclesia/magent/lib/eeprom/eeprom.h"

namespace ecclesia {

// An Eeprom interface which reads data via I2C/SMBus. Subclass should implement
// ReadBytes and WriteBytes.
class SmbusEeprom : public Eeprom {
 public:
  struct Option {
    std::string name;
    SizeType size;
    ModeType mode;
    // Function which creates a SMBUS device for reading the EEPROM.
    std::function<std::optional<SmbusDevice>()> get_device;
  };

  explicit SmbusEeprom(Option option) : option_(std::move(option)) {}
  SmbusEeprom(const SmbusEeprom &) = delete;
  SmbusEeprom &operator=(const SmbusEeprom &) = delete;

  Eeprom::SizeType GetSize() const override { return option_.size; }
  Eeprom::ModeType GetMode() const override { return option_.mode; }

 protected:
  std::optional<SmbusDevice> GetDevice() const { return option_.get_device(); }

 private:
  Option option_;
};

// A class representing EEPROMs that range in size from 8k-bit to 512k-bit
// and are addressed using a 2-byte offset.
class SmbusEeprom2ByteAddr : public SmbusEeprom {
 public:
  explicit SmbusEeprom2ByteAddr(Option option)
      : SmbusEeprom(std::move(option)) {}
  SmbusEeprom2ByteAddr(const SmbusEeprom2ByteAddr &) = delete;
  SmbusEeprom2ByteAddr &operator=(const SmbusEeprom2ByteAddr &) = delete;

  // Currently only support sequential read
  std::optional<int> ReadBytes(size_t offset,
                               absl::Span<unsigned char> value) const override;
  std::optional<int> WriteBytes(
      size_t offset, absl::Span<const unsigned char> data) const override;

 private:
  // Reads the eeprom one byte at a time.
  std::optional<int> SequentialRead(size_t offset,
                                    absl::Span<unsigned char> value) const
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(device_mutex_);

  mutable absl::Mutex device_mutex_;
};

// A class representing the "standard" 2k-bit (256 byte) SMBus compatible
// EEPROM.
class SmbusEeprom2K : public SmbusEeprom {
 public:
  explicit SmbusEeprom2K(Option option) : SmbusEeprom(std::move(option)) {}
  SmbusEeprom2K(const SmbusEeprom2ByteAddr &) = delete;
  SmbusEeprom2K &operator=(const SmbusEeprom2ByteAddr &) = delete;

  Eeprom::SizeType GetSize() const override {
    return {.type = ecclesia::Eeprom::SizeType::kFixed, .size = kEepromSize};
  }

  std::optional<int> ReadBytes(size_t offset,
                               absl::Span<unsigned char> value) const override;
  std::optional<int> WriteBytes(
      size_t offset, absl::Span<const unsigned char> data) const override {
    return std::nullopt;
  }

 private:
  static constexpr size_t kEepromSize = 256;
};

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_LIB_EEPROM_SMBUS_EEPROM_H_
