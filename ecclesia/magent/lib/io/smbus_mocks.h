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

#ifndef ECCLESIA_MAGENT_LIB_IO_SMBUS_MOCKS_H_
#define ECCLESIA_MAGENT_LIB_IO_SMBUS_MOCKS_H_

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "gmock/gmock.h"
#include "absl/status/status.h"
#include "absl/types/span.h"
#include "ecclesia/magent/lib/io/smbus.h"

namespace ecclesia {

// Mock low-level device access.
class MockSmbusAccessInterface : public SmbusAccessInterface {
 public:
  MockSmbusAccessInterface() {}

  MOCK_METHOD(absl::Status, ProbeDevice, (const SmbusLocation &loc),
              (const, override));

  MOCK_METHOD(absl::Status, WriteQuick,
              (const SmbusLocation &loc, uint8_t data), (const, override));

  MOCK_METHOD(absl::Status, SendByte, (const SmbusLocation &loc, uint8_t data),
              (const, override));

  MOCK_METHOD(absl::Status, ReceiveByte,
              (const SmbusLocation &loc, uint8_t *data), (const, override));

  MOCK_METHOD(absl::Status, Write8,
              (const SmbusLocation &loc, int command, uint8_t data),
              (const, override));

  MOCK_METHOD(absl::Status, Read8,
              (const SmbusLocation &loc, int command, uint8_t *data),
              (const, override));

  MOCK_METHOD(absl::Status, Write16,
              (const SmbusLocation &loc, int command, uint16_t data),
              (const, override));

  MOCK_METHOD(absl::Status, Read16,
              (const SmbusLocation &loc, int command, uint16_t *data),
              (const, override));

  MOCK_METHOD(absl::Status, WriteBlockI2C,
              (const SmbusLocation &loc, int command,
               absl::Span<const unsigned char> data),
              (const, override));

  MOCK_METHOD(absl::Status, ReadBlockI2C,
              (const SmbusLocation &loc, int command,
               absl::Span<unsigned char> data, size_t *len),
              (const, override));

  MOCK_METHOD(bool, SupportBlockRead, (const SmbusLocation &loc),
              (const, override));
};

// Read 1 byte from provided immediate value
ACTION_P(SmbusRead8, data) {
  uint8_t *output_data = arg2;
  memcpy(output_data, data, 1);
  return absl::OkStatus();
}

// Read 2 byte from provided immediate value
ACTION_P(SmbusRead16, data) {
  uint16_t *output_data = arg2;
  memcpy(output_data, data, 2);
  return absl::OkStatus();
}

// Read 1 byte from provided immediate value
ACTION_P(SmbusReceiveByte, data) {
  uint8_t *output_data = arg1;
  memcpy(output_data, data, 1);
  return absl::OkStatus();
}

// Read block data from provided immediate value
ACTION_P2(SmbusReadBlock, data, size) {
  unsigned char *output_data = arg2.data();
  memcpy(output_data, data, size);
  *arg3 = size;
  return absl::OkStatus();
}

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_LIB_IO_SMBUS_MOCKS_H_
