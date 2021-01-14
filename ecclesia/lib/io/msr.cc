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

#include "ecclesia/lib/io/msr.h"

#include <array>
#include <cstdint>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "ecclesia/lib/apifs/apifs.h"
#include "ecclesia/lib/codec/endian.h"
#include "ecclesia/lib/status/macros.h"

namespace ecclesia {

Msr::Msr(std::string path) : msr_file_(std::move(path)) {}

bool Msr::Exists() const { return msr_file_.Exists(); }

absl::StatusOr<uint64_t> Msr::Read(uint64_t reg) const {
  std::array<char, sizeof(uint64_t)> res;
  ECCLESIA_RETURN_IF_ERROR(msr_file_.SeekAndRead(reg, absl::MakeSpan(res)));
  return LittleEndian::Load64(res.data());
}

absl::Status Msr::Write(uint64_t reg, uint64_t value) const {
  char buffer[8];
  LittleEndian::Store64(value, buffer);
  return msr_file_.SeekAndWrite(reg, absl::MakeConstSpan(buffer));
}

}  // namespace ecclesia
