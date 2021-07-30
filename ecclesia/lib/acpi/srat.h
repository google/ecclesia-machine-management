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

#ifndef ECCLESIA_LIB_ACPI_SRAT_H_
#define ECCLESIA_LIB_ACPI_SRAT_H_

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "ecclesia/lib/acpi/srat.emb.h"
#include "ecclesia/lib/acpi/system_description_table.h"

namespace ecclesia {

class Srat {
 public:
  explicit Srat(std::unique_ptr<SystemDescriptionTable> table)
      : table_(std::move(table)) {}
  Srat(const Srat&) = delete;
  Srat& operator=(const Srat&) = delete;

  SratHeaderView GetSratHeader() const {
    return MakeSratHeaderView(table_->table_data().data(),
                              SratHeaderView::SizeInBytes());
  }

  // Default sysfs path to the ACPI SRAT.
  static constexpr absl::string_view kSysfsAcpiSratPath =
      "/sys/firmware/acpi/tables/SRAT";

  // Expected signature of the ACPI SRAT.
  static constexpr uint32_t kAcpiSratSignature =
      (('S' << 0) | ('R' << 8) | ('A' << 16) | ('T' << 24));

 private:
  std::unique_ptr<SystemDescriptionTable> table_;
};

// Types of static resource allocation structures assigned to the flag field of
// the SraHeader structure.
enum SratSraHeaderType {
  // SratSraHeader is a header of SraProcessorApicAffinity.
  SRAT_SRA_HEADER_PROCESSOR_APIC_AFFINITY = 0,
  // SratSraHeader is a header of MemoryAffinity.
  SRAT_SRA_HEADER_MEMORY_AFFINITY = 1,
  // SratSraHeader is a header of SraProcessorx2ApicAffinity.
  SRAT_SRA_HEADER_PROCESSOR_X2APIC_AFFINITY = 2,
  // SratSraHeader is a header of GiccAffinity.
  SRAT_SRA_HEADER_GICC_AFFINITY = 3,
};


// Valid flags for ProcessorApicAffinity.flags.
enum SratProcessorApicAffinityFlags {
  // If this flag isn't set the associated structure should be ignored.
  SRAT_PROCESSOR_APIC_AFFINITY_FLAGS_ENABLED = (1 << 0),
};

// Valid flags for SraMemoryAffinity.flags.
enum SratMemoryAffinityFlags {
  // If this flag isn't set the associated structure should be ignored.
  SRAT_MEMORY_AFFINITY_FLAGS_ENABLED = (1 << 0),
  // Whether the memory is hot pluggable.
  SRAT_MEMORY_AFFINITY_FLAGS_HOT_PLUGGABLE = (1 << 1),
  // Whether the memory region references non-volatile memory.
  SRAT_MEMORY_AFFINITY_FLAGS_NON_VOLATILE = (1 << 2),
};

// Class which parses an ACPI system resource affinity table (SRAT).
class SratReader : public SystemDescriptionTableReader<SraHeaderDescriptor> {
 public:
  explicit SratReader(SratHeaderView srat)
      : SystemDescriptionTableReader(
            reinterpret_cast<const char*>(srat.BackingStorage().data()),
            srat.SizeInBytes()) {}
  SratReader(const SratReader&) = delete;
  SratReader& operator=(const SratReader&) = delete;

  // Validate the SRAT signature.
  bool ValidateSignature() const override;

  // Validate the SRAT revision to ensure that it's supported by this class.
  bool ValidateRevision() const override;

  // Get the Processor Local APIC/SAPIC Affinity Structures following the SRAT.
  // If only_enabled is true, only the enabled structures are returned.
  // This function adds pointers to the static resource allocation structures
  // to the supplied vector and returns the number of structures added to the
  // vector.
  virtual std::vector<SratProcessorApicAffinityView> GetProcessorApicAffinity(
      const bool only_enabled) const {
    return GetSraStructuresByType<SratProcessorApicAffinityView,
                                  SRAT_SRA_HEADER_PROCESSOR_APIC_AFFINITY>(
        only_enabled, SRAT_PROCESSOR_APIC_AFFINITY_FLAGS_ENABLED);
  }

  // Get the Memory Affinity structures following the SRAT.
  // This function adds pointers to the static resource allocation structures
  // to the supplied vector and returns the number of structures added to the
  // supplied vector.
  virtual std::vector<SratMemoryAffinityView> GetMemoryAffinity(
      const bool only_enabled) const {
    return GetSraStructuresByType<SratMemoryAffinityView,
                                  SRAT_SRA_HEADER_MEMORY_AFFINITY>(
        only_enabled, SRAT_MEMORY_AFFINITY_FLAGS_ENABLED);
  }

  // Get the Processor Local x2APIC Affinity structures following the SRAT.
  // If only_enabled is true, only the enabled structures are returned.
  // This function adds pointers to the static resource allocation structures
  // to the supplied vector and returns the number of structures added to the
  // vector.
  virtual std::vector<SratProcessorx2ApicAffinityView>
  GetProcessorx2ApicAffinity(const bool only_enabled) const {
    return GetSraStructuresByType<SratProcessorx2ApicAffinityView,
                                  SRAT_SRA_HEADER_PROCESSOR_X2APIC_AFFINITY>(
        only_enabled, SRAT_PROCESSOR_APIC_AFFINITY_FLAGS_ENABLED);
  }

  // Revision of the ACPI SRAT supported by this class.
  static constexpr uint8_t kMaximumSupportedAcpiSratRevision = 3;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_ACPI_SRAT_H_
