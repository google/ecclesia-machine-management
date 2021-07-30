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

#ifndef ECCLESIA_LIB_ACPI_MCFG_H_
#define ECCLESIA_LIB_ACPI_MCFG_H_

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "ecclesia/lib/acpi/mcfg.emb.h"
#include "ecclesia/lib/acpi/system_description_table.h"

namespace ecclesia {
// Structure that encapsulates the ACPI multiple APIC description table (MADT).
class Mcfg {
 public:
  explicit Mcfg(std::unique_ptr<SystemDescriptionTable> table)
      : table_(std::move(table)) {}
  Mcfg(const Mcfg&) = delete;
  Mcfg& operator=(const Mcfg&) = delete;

  McfgHeaderView GetMcfgHeader() const {
    return MakeMcfgHeaderView(table_->table_data().data(),
                              McfgHeaderView::SizeInBytes());
  }

  // Default sysfs path to the ACPI MCFG.
  static constexpr absl::string_view kSysfsAcpiMcfgPath =
      "/sys/firmware/acpi/tables/MCFG";

  // Expected signature of the ACPI MCFG.
  static constexpr uint32_t kAcpiMcfgSignature =
      (('M' << 0) | ('C' << 8) | ('F' << 16) | ('G' << 24));
  static constexpr uint32_t kAcpiMcfgSupportedRevision = 1;

 private:
  std::unique_ptr<SystemDescriptionTable> table_;
};

// Helper class that reads the ACPI MCFG.
class McfgReader : public SystemDescriptionTableReader<SraHeaderDescriptor> {
 public:
  explicit McfgReader(McfgHeaderView mcfg)
      : SystemDescriptionTableReader(
            reinterpret_cast<const char*>(mcfg.BackingStorage().data()),
            mcfg.SizeInBytes()) {}
  McfgReader(const McfgReader&) = delete;
  McfgReader& operator=(const McfgReader&) = delete;

  // Validate the table signature/revision.
  bool ValidateSignature() const override;
  bool ValidateRevision() const override;

  // Get the McfgSegment structures.
  virtual std::vector<McfgSegmentView> GetSegments() const;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_ACPI_MCFG_H_
