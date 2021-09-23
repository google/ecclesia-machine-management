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

#include "ecclesia/lib/acpi/mcfg.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include "ecclesia/lib/acpi/mcfg.emb.h"
#include "ecclesia/lib/acpi/system_description_table.emb.h"
#include "runtime/cpp/emboss_cpp_util.h"
#include "runtime/cpp/emboss_prelude.h"

namespace ecclesia {

bool McfgReader::ValidateSignature() const {
  return GetHeaderView().signature().Read() == Mcfg::kAcpiMcfgSignature;
}

bool McfgReader::ValidateRevision() const {
  return GetHeaderView().revision().Read() == Mcfg::kAcpiMcfgSupportedRevision;
}

std::vector<McfgSegmentView> McfgReader::GetSegments() const {
  std::vector<McfgSegmentView> result;

  auto header_view = GetHeaderView();
  const uint8_t *header_bytes = header_view.BackingStorage().data();
  for (size_t i = McfgHeaderView::SizeInBytes();
       i < header_view.length().Read(); i += McfgSegmentView::SizeInBytes()) {
    result.push_back(
        MakeMcfgSegmentView(header_bytes + i, McfgSegmentView::SizeInBytes()));
  }

  return result;
}

}  // namespace ecclesia
