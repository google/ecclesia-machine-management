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

#ifndef ECCLESIA_LIB_ACPI_DMAR_H_
#define ECCLESIA_LIB_ACPI_DMAR_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "ecclesia/lib/acpi/dmar.emb.h"
#include "ecclesia/lib/acpi/system_description_table.h"
#include "runtime/cpp/emboss_cpp_util.h"

namespace ecclesia {

// Structure that encapsulates the ACPI multiple APIC description table (DMAR).
class Dmar {
 public:
  explicit Dmar(std::unique_ptr<SystemDescriptionTable> table)
      : table_(std::move(table)) {}
  Dmar(const Dmar&) = delete;
  Dmar& operator=(const Dmar&) = delete;

  DmarHeaderView GetDmarHeader() const {
    return MakeDmarHeaderView(table_->table_data().data(),
                              DmarHeaderView::SizeInBytes());
  }

  // Default sysfs path to the ACPI DMAR.
  static constexpr absl::string_view kSysfsAcpiDmarPath =
      "/sys/firmware/acpi/tables/DMAR";

  // Expected signature of the ACPI DMAR.
  static constexpr uint32_t kAcpiDmarSignature =
      (('D' << 0) | ('M' << 8) | ('A' << 16) | ('R' << 24));

 private:
  std::unique_ptr<SystemDescriptionTable> table_;
};

// Flags of the Dmar.flags field.
enum DmarFlags {
  DMAR_FLAGS_INTERRUPT_REMAPPING_ENABLED = (1 << 0)
};


// Used to pass DmarSraHeader emboss struct and its Make and Validate functions
// as a single template variable.
class DmarSraHeaderDescriptor {
 public:
  using View = DmarSraHeaderView;
  static DmarSraHeaderView MakeView(const char* data, size_t size) {
    return MakeDmarSraHeaderView(data, size);
  }

  // Validate a static allocation entry structure matches the expected_type
  // and is at least minimum_size bytes in size.  structure_name is used to
  // format error messages.
  static bool Validate(View header_view, const uint16_t expected_type,
                       const uint16_t minimum_size,
                       const char* const structure_name);

  // Determine whether the size of this structure is smaller than
  // maximum_sra_size.
  static bool ValidateMaximumSize(View header_view,
                                  const uint32_t maximum_sra_size);
};

// Types of ACPI structures that follow the DMAR structure.
enum DmarSraHeaderType {
  // See DmarHardwareUnitDefinition.
  DMAR_SRA_HEADER_TYPE_HARDWARE_UNIT_DEFINITION = 0,
  // See DmarReservedMemoryRegion.
  DMAR_SRA_HEADER_TYPE_RESERVED_MEMORY_REGION,
  // See DmarRootPortAtsCapability.
  DMAR_SRA_HEADER_TYPE_ROOT_PORT_ATS_CAPABILITY,
  // See DmarRemappingHardwareStaticAffinity.
  DMAR_SRA_HEADER_TYPE_REMAPPING_HARDWARE_STATIC_AFFINITY,
};

// Helper class that reads the ACPI DMAR.
class DmarReader
    : public SystemDescriptionTableReader<DmarSraHeaderDescriptor> {
 public:
  explicit DmarReader(DmarHeaderView dmar)
      : SystemDescriptionTableReader(
            reinterpret_cast<const char*>(dmar.BackingStorage().data()),
            dmar.SizeInBytes()) {}
  DmarReader(const DmarReader&) = delete;
  DmarReader& operator=(const DmarReader&) = delete;

  // Validate the table signature.
  bool ValidateSignature() const override;

  // Validate the DMAR revision to ensure that it's supported by this class.
  bool ValidateRevision() const override;

  // Get the DmarHardwareUnitDefinition structures.
  std::vector<DmarHardwareUnitDefinitionView> GetHardwareUnitDefinition()
      const {
    return GetSraStructuresByTypeAndMinSize<
        DmarHardwareUnitDefinitionView,
        DMAR_SRA_HEADER_TYPE_HARDWARE_UNIT_DEFINITION>();
  }

  // Get the DmarReservedMemoryRegion structures.
  virtual std::vector<DmarReservedMemoryRegionView> GetReservedMemoryRegion()
      const {
    return GetSraStructuresByTypeAndMinSize<
        DmarReservedMemoryRegionView,
        DMAR_SRA_HEADER_TYPE_RESERVED_MEMORY_REGION>();
  }

  // Get the DmarRootPortAtsCapability structures.
  virtual std::vector<DmarRootPortAtsCapabilityView> GetRootPortAtsCapability()
      const {
    return GetSraStructuresByTypeAndMinSize<
        DmarRootPortAtsCapabilityView,
        DMAR_SRA_HEADER_TYPE_ROOT_PORT_ATS_CAPABILITY>();
  }

  // Get the DmarRemappingHardwareStaticAffinity structures.
  virtual std::vector<DmarRemappingHardwareStaticAffinityView>
  GetRemappingHardwareStaticAffinity() const {
    return GetSraStructuresByTypeAndMinSize<
        DmarRemappingHardwareStaticAffinityView,
        DMAR_SRA_HEADER_TYPE_REMAPPING_HARDWARE_STATIC_AFFINITY>();
  }
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_ACPI_DMAR_H_
