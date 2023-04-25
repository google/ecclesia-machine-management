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

#include "ecclesia/lib/smbios/processor_information.h"

#include <optional>
#include <string>

#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/smbios/structures.emb.h"
#include "third_party/emboss/runtime/cpp/emboss_prelude.h"

namespace ecclesia {

namespace {

// From the SMBIOS Spec.
enum ProcessorFamily {
  OTHER = 0x1,
  UNKNOWN = 0x2,
  INTEL_XEON = 0xB3,
  // Beware that this file is open sourced, so avoid leaking processor family
  // types that are confidential to the public!
};

}  // namespace

CpuSignature ProcessorInformation::GetSignaturex86() const {
  CpuSignature signature;

  auto view = this->GetMessageView();
  signature.vendor = this->GetString(view.manufacturer_snum().Read());
  signature.type = view.processor_id_x86().processor_type().Read();
  // From Intel's instruction set reference for CPUID
  signature.family = (view.processor_id_x86().family_id_ext().Read() << 4) +
                     view.processor_id_x86().family_id().Read();
  signature.model = (view.processor_id_x86().model_ext().Read() << 4) +
                    view.processor_id_x86().model().Read();
  signature.stepping = view.processor_id_x86().stepping_id().Read();

  return signature;
}

CpuSignature ProcessorInformation::GetSignatureAmd() const {
  CpuSignature signature;

  auto view = this->GetMessageView();
  signature.vendor = this->GetString(view.manufacturer_snum().Read());
  signature.type = view.processor_id_x86().processor_type().Read();

  // From https://en.wikichip.org/wiki/amd/cpuid
  int family_id = view.processor_id_x86().family_id().Read();
  int family_id_ext = view.processor_id_x86().family_id_ext().Read();
  int model = view.processor_id_x86().model().Read();
  int model_ext = view.processor_id_x86().model_ext().Read();
  signature.family = family_id < 0x0F ? family_id : family_id + family_id_ext;
  signature.model = family_id < 0x0F ? model : ((model_ext << 4) | model);
  signature.stepping = view.processor_id_x86().stepping_id().Read();
  return signature;
}

std::optional<CpuSignature> ProcessorInformation::GetSignature() const {
  if (IsIntelProcessor()) {
    return this->GetSignaturex86();
  }
  if (IsAmdProcessor()) {
    return this->GetSignatureAmd();
  }
  return std::nullopt;
}

std::string ProcessorInformation::GetManufacturer() const {
  return std::string(GetString(GetMessageView().manufacturer_snum().Read()));
}

uint64_t ProcessorInformation::GetProcessorId() const {
  return GetMessageView().processor_id_uint64().Read();
}

bool ProcessorInformation::IsIntelProcessor() const {
  return absl::StrContains(
      GetString(GetMessageView().manufacturer_snum().Read()), "Intel");
}

bool ProcessorInformation::IsAmdProcessor() const {
  return absl::StrContains(
      GetString(GetMessageView().manufacturer_snum().Read()),
      "Advanced Micro Devices");
}

}  // namespace ecclesia
