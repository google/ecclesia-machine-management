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

#include "ecclesia/magent/lib/eeprom/smbus_eeprom.h"

#include <assert.h>

#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <optional>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "ecclesia/lib/io/smbus/smbus.h"
#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/logging.h"

namespace ecclesia {
namespace {
constexpr size_t kReadBlockSizeInBytes = 8;

absl::StatusOr<size_t> ReadByBlock(size_t offset, size_t len,
                                   absl::Span<unsigned char> value,
                                   const SmbusDevice &device) {
  if (!device.SupportBlockRead()) {
    return absl::InternalError("Smbus device doesn't support block read.");
  }

  for (size_t i = 0; i < len; i += kReadBlockSizeInBytes) {
    unsigned char temp_data[kReadBlockSizeInBytes]{};
    size_t to_read = std::min(kReadBlockSizeInBytes, len - i);
    size_t num_bytes_read;

    // If len is not block aligned, the last num_bytes_read would be greater
    // than to_read(the number of bytes we need to read). In that case, we just
    // copy what we want and discard the rest of bytes read.
    if (!device.ReadBlockI2C(offset + i, temp_data, &num_bytes_read).ok() ||
        num_bytes_read < to_read || num_bytes_read > kReadBlockSizeInBytes) {
      ErrorLog() << "SMBus EEPROM " << device.location() << ": Failed "
                 << kReadBlockSizeInBytes << "byte read at 0x" << std::hex
                 << offset + i;
      return absl::InternalError("Unexpected error.");
    }
    memcpy(value.data() + i, temp_data, to_read);
  }

  return len;
}

absl::StatusOr<size_t> ReadByWord(size_t offset, size_t len,
                                  absl::Span<unsigned char> value,
                                  const SmbusDevice &device) {
  if (len == 0) {
    return 0;
  }

  size_t write_index = value.size() - len;
  for (size_t i = 0; i < len; i += sizeof(uint16_t)) {
    uint16_t temp_data = 0;
    if (!device.Read16(offset + i, &temp_data).ok()) {
      ErrorLog() << "SMBus EEPROM " << device.location()
                 << ": Failed 16bit read at 0x" << std::hex << offset + i;
      // Unexpected error, read should not continue.
      return absl::InternalError("Unexpected error.");
    }

    value[write_index++] = temp_data & 0xff;
    // When len is odd number, the check will prevent overwriting.
    if (write_index < value.size()) {
      value.data()[write_index++] = temp_data >> 8;
    }
  }

  return len;
}
}  // namespace

std::optional<int> SmbusEeprom2ByteAddr::SequentialRead(
    size_t offset, absl::Span<unsigned char> value) const {
  // We can't actually use smbus block read because the driver doesn't know how
  // to do the 2-byte address write. So we do the best we can by performing the
  // address write once, then calling read byte repeatedly to keep the overhead
  // to a minimum.

  std::optional<SmbusDevice> device = GetDevice();
  if (!device) return std::nullopt;

  // Write the eeprom offset as a command byte + data byte.
  uint8_t hi = offset >> 8;
  uint8_t lo = offset & 0xff;
  size_t len = value.size();

  absl::Status status = device->Write8(hi, lo);
  if (!status.ok()) {
    ErrorLog() << "smbus device " << device->location()
               << " Failed to write smbus register 0x" << std::hex << offset
               << '\n';
    return std::nullopt;
  }

  // Read the block a byte a time.
  int i = 0;
  for (i = 0; i < len; ++i) {
    uint8_t val;
    if (!device->ReceiveByte(&val).ok()) {
      ErrorLog() << "smbus device " << device->location()
                 << " Failed to read smbus register 0x" << std::hex
                 << offset + i;
      return std::nullopt;
    }
    value[i] = val;
  }

  return i;
}

std::optional<int> SmbusEeprom2ByteAddr::ReadBytes(
    size_t offset, absl::Span<unsigned char> value) const {
  memset(value.data(), 0, value.size());

  absl::MutexLock ml(&device_mutex_);

  return SequentialRead(offset, value);
}

std::optional<int> SmbusEeprom2ByteAddr::WriteBytes(
    size_t offset, absl::Span<const unsigned char> data) const {
  return std::nullopt;
}

std::optional<int> SmbusEeprom2K::ReadBytes(
    size_t offset, absl::Span<unsigned char> value) const {
  std::optional<SmbusDevice> device = GetDevice();
  if (!device.has_value()) {
    return std::nullopt;
  }

  memset(value.data(), 0, value.size());
  size_t len = value.size();
  // Check if the requested read length exceeds the size of the eeprom
  if (len < 1 || len > kEepromSize) {
    ErrorLog() << "Requested read length exceeds the size of the eeprom.";
    return std::nullopt;
  }

  // First try block read, if any error happens then fall back to word read.
  // In some cases, even though ReadByBlock is supported, the number of bytes
  // read is less than expected. Falling back to word can succeed in such cases.
  auto maybe_num_bytes_read = ReadByBlock(offset, len, value, device.value());
  if (!maybe_num_bytes_read.ok() || maybe_num_bytes_read.value() != len) {
    maybe_num_bytes_read = ReadByWord(offset, len, value, device.value());
    if (!maybe_num_bytes_read.ok() || maybe_num_bytes_read.value() != len) {
      return std::nullopt;
    }
  }

  assert(len <= INT_MAX);
  return len;
}

}  // namespace ecclesia
