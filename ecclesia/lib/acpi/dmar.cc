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

#include "ecclesia/lib/acpi/dmar.h"

#include <cstdint>

#include "absl/strings/str_format.h"
#include "ecclesia/lib/acpi/dmar.emb.h"
#include "ecclesia/lib/logging/logging.h"

namespace ecclesia {

constexpr uint8_t kMaximumSupportedAcpiDmarRevision = 1;

bool DmarReader::ValidateSignature() const {
  return GetHeaderView().signature().Read() == Dmar::kAcpiDmarSignature;
}

bool DmarReader::ValidateRevision() const {
  return GetHeaderView().revision().Read() <= kMaximumSupportedAcpiDmarRevision;
}

bool DmarSraHeaderDescriptor::Validate(View header_view,
                                       const uint16_t expected_type,
                                       const uint16_t minimum_size,
                                       const char* const structure_name) {
  Check(minimum_size >= View::SizeInBytes(), "sufficient minimum size")
      << absl::StrFormat(
             "insufficient minimum size for DMAR header check, "
             "need at least %u, actual %u",
             View::SizeInBytes(), minimum_size);
  if (header_view.struct_type().Read() != expected_type) {
    ErrorLog() << absl::StrFormat(
        "Unexpected header type %d vs. %d for %s "
        "structure %p.",
        header_view.struct_type().Read(), expected_type, structure_name,
        header_view.BackingStorage().data());
    return false;
  }
  if (header_view.length().Read() < minimum_size) {
    ErrorLog() << absl::StrFormat("%s structure %p too small %d vs. %d bytes.",
                                  structure_name,
                                  header_view.BackingStorage().data(),
                                  header_view.length().Read(), minimum_size);
    return false;
  }
  return true;
}

bool DmarSraHeaderDescriptor::ValidateMaximumSize(
    View header_view, const uint32_t maximum_sra_size) {
  if (View::SizeInBytes() > maximum_sra_size ||
      header_view.length().Read() > maximum_sra_size) {
    ErrorLog() << absl::StrFormat(
        "Static resource allocation structure size %d "
        "exceeds the maximum size %d",
        header_view.length().Read(), static_cast<int>(maximum_sra_size));
    return false;
  }
  return true;
}

}  // namespace ecclesia
