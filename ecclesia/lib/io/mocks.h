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

#ifndef ECCLESIA_LIB_IO_MOCKS_H_
#define ECCLESIA_LIB_IO_MOCKS_H_

#include "gmock/gmock.h"
#include "ecclesia/lib/io/generic_mmio.h"

namespace ecclesia {

class MockMmioAccess : public MmioAccessInterface {
 public:
  explicit MockMmioAccess(size_t size) : size_(size) {}

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

  size_t Size() const override { return size_; }

 private:
  const size_t size_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_IO_MOCKS_H_
