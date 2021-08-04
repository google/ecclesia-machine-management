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

#ifndef ECCLESIA_LIB_ACPI_SYSTEM_DESCRIPTION_TABLE_H_
#define ECCLESIA_LIB_ACPI_SYSTEM_DESCRIPTION_TABLE_H_

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "ecclesia/lib/acpi/system_description_table.emb.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/lib/status/macros.h"

namespace ecclesia {

// Reads ACPI system description table from a file and manages allocated memory
// for the data structure.
class SystemDescriptionTable {
 public:
  // Size of common header for all ACPI system description tables.
  static constexpr size_t kHeaderSize =
      SystemDescriptionTableHeaderView::SizeInBytes();

  SystemDescriptionTable(const SystemDescriptionTable&) = delete;
  SystemDescriptionTable& operator=(const SystemDescriptionTable&) = delete;

  virtual ~SystemDescriptionTable() { }

  // Call this to get an Emboss struct view into the SystemDescriptionTable
  // header.
  SystemDescriptionTableHeaderView GetHeaderView() const {
    return MakeSystemDescriptionTableHeaderView(table_data_.data(),
                                                table_data_.size());
  }

  // Computes checksum from underlying table data.
  uint8_t CalculateChecksum() const;
  // Returns signature string taken from underlying table data.
  std::string GetSignatureString() const;

  const std::string& table_data() const { return table_data_; }

  // Read a system description table from sysfs_acpi_table_path returning a
  // SystemDescriptionTable containing the read data if successful, or error
  // status if not.
  static absl::StatusOr<std::unique_ptr<SystemDescriptionTable>> ReadFromFile(
      absl::string_view sysfs_acpi_table_path);

  // Create a table with a copy of `data` that is stored internally.
  static std::unique_ptr<SystemDescriptionTable> CreateTableFromData(
      const char* data, size_t size);

 private:
  // Use ReadFromFile to create instances of this class.
  SystemDescriptionTable() = default;

  std::string table_data_;
};

// Used to pass SraHeader emboss struct and its Make and Validate functions as a
// single template variable.
class SraHeaderDescriptor {
 public:
  using View = SraHeaderView;
  static SraHeaderView MakeView(const char* data, size_t size) {
    return MakeSraHeaderView(data, size);
  }

  // Validate that a static allocation entry structure matches the expected_type
  // and is at least minimum_size bytes in size.  structure_name is used to
  // format error messages.
  static bool Validate(SraHeaderView header_view, const uint8_t expected_type,
                       const uint8_t minimum_size,
                       absl::string_view structure_name);

  // Determine whether the size of the structure is smaller than
  // maximum_sra_size.
  static bool ValidateMaximumSize(SraHeaderView header_view,
                                  const uint32_t maximum_sra_size);
};

// Class template for Reading a system description table.
//
// The template variable SraHeaderT represents the header of SRA structures,
// which may contain different structrues for differenet types of tables.
template <class SraHeaderDescType>
class SystemDescriptionTableReader {
 public:
  using View = typename SraHeaderDescType::View;

  // Class used in conjunction with GetSraStructures() to filter SraHeader
  // structures.
  class SraHeaderFilter {
   public:
    SraHeaderFilter() { }
    virtual ~SraHeaderFilter() { }

    // Return false to ignore the static resource allocation structure, true
    // to include the structure.
    virtual bool Filter(View  /*sra_header*/) { return true; }
  };

  // Filter SraHeader structures returned by GetSraStructures() by type and
  // minimum structure size.
  class SraHeaderFilterByTypeAndMinSize : public SraHeaderFilter {
   public:
    explicit SraHeaderFilterByTypeAndMinSize(const uint8_t type,
                                             const uint8_t minimum_size)
        : type_(type), minimum_size_(minimum_size) {}
    ~SraHeaderFilterByTypeAndMinSize() override {}

    bool Filter(View sra_header) override {
      return sra_header.struct_type().Read() == type_ &&
             sra_header.length().Read() >= minimum_size_;
    }

   private:
    uint8_t type_;
    uint8_t minimum_size_;
  };

  // Filter SraHeader structures of type header_type of minimum size
  // minimum_header_size and those SraHeaderData structures marked enabled as
  // indicated by `flag_func(sra_header)` matching the enabled_flag.
  template <typename SraHeaderDataType>
  class SraHeaderFilterByEnabled : public SraHeaderFilterByTypeAndMinSize {
   public:
    SraHeaderFilterByEnabled(uint8_t header_type, uint8_t minimum_header_size,
                             uint32_t enabled_flag)
        : SraHeaderFilterByTypeAndMinSize(header_type, minimum_header_size),
          enabled_flag_(enabled_flag) {}

    bool Filter(View sra_header) override {
      return SraHeaderFilterByTypeAndMinSize::Filter(sra_header) &&
             (SraHeaderDataType(sra_header.BackingStorage()).flags().Read() &
              enabled_flag_);
    }

   private:
    uint32_t enabled_flag_;
  };

  // header_size is the size of the table header prior to any static
  // allocation entries.  header_size should be lesser than or equal to
  // the size of the entire table.
  template <typename HeaderType>
  SystemDescriptionTableReader(const HeaderType& header,
                               const uint32_t header_size)
      : table_data_(reinterpret_cast<const char*>(&header)),
        header_size_(header_size) {}
  SystemDescriptionTableReader(const char* table_data,
                               const uint32_t header_size)
      : table_data_(table_data), header_size_(header_size) {}
  SystemDescriptionTableReader(const SystemDescriptionTableReader&) = delete;
  SystemDescriptionTableReader& operator=(
      const SystemDescriptionTableReader&) = delete;
  virtual ~SystemDescriptionTableReader() { }

  static View MakeView(const char* data, size_t size) {
    return SraHeaderDescType::MakeView(data, size);
  }

  // Validate the table.
  virtual bool Validate() const {
    return ValidateSignature() && ValidateRevision() &&
        0 == CalculateChecksum();
  }

  // Calculate the checksum of the table.  A valid structure should yield a
  // checksum of 0.
  virtual uint8_t CalculateChecksum() const {
    return CreateTable()->CalculateChecksum();
  }

  // Retrieve a pointer to the first static resource allocation structure.
  // If no structure is found nullptr is returned.
  template <typename SraHeaderDataViewType = View>
  absl::optional<SraHeaderDataViewType> GetFirstSraStructure() const {
    View sra_header =
        MakeView(table_data_ + header_size(), View::SizeInBytes());
    View sra_structure =
        MakeView(table_data_ + header_size(), sra_header.length().Read());
    if (!SraHeaderDescType::ValidateMaximumSize(
            sra_structure, GetHeaderView().length().Read() - header_size())) {
      return absl::nullopt;
    }
    return SraHeaderDataViewType(sra_structure.BackingStorage());
  }

  // Given a pointer to a static resource allocation structure in the table
  // referenced by this class get a pointer to the next static resource
  // allocation structure.
  template <typename SraHeaderDataViewType = View>
  absl::optional<SraHeaderDataViewType> GetNextSraStructure(
      SraHeaderDataViewType sra_header) const {
    const uint32_t hdr_size = header_size();
    const char* sra_start = table_data_ + hdr_size;
    const char* sra_end = table_data_ + GetHeaderView().length().Read();
    const char* sra =
        reinterpret_cast<const char*>(sra_header.BackingStorage().data());
    uint32_t maximum_sra_size =
        GetHeaderView().length().Read() -
        (hdr_size + static_cast<uint32_t>(sra - sra_start));
    if (sra < sra_start || sra >= sra_end ||
        !SraHeaderDescType::ValidateMaximumSize(sra_header, maximum_sra_size) ||
        sra_header.length().Read() == 0) {
      ErrorLog() << absl::StrFormat(
          "Static resource allocation structure %p out of range of %s "
          "%p table",
          sra_header.BackingStorage().data(),
          CreateTable()->GetSignatureString(), table_data_);
      return absl::nullopt;
    }
    maximum_sra_size -= sra_header.length().Read();
    if (maximum_sra_size < View::SizeInBytes()) {
      return absl::nullopt;
    }
    uint32_t next_sra_length =
        MakeView(sra + sra_header.length().Read(), View::SizeInBytes())
            .length().Read();
    if (maximum_sra_size < next_sra_length) {
      return absl::nullopt;
    }
    View next_sra_header = MakeView(sra + sra_header.length().Read(),
                                    next_sra_length);
    if (!SraHeaderDescType::ValidateMaximumSize(next_sra_header,
                                                maximum_sra_size)) {
      return absl::nullopt;
    }
    return SraHeaderDataViewType(next_sra_header.BackingStorage());
  }

  // Get the static resource allocation structures following the table.
  // This function returns a vector containing views to the static resource
  // allocation structures.
  template <typename SraHeaderDataViewType = View>
  std::vector<SraHeaderDataViewType> GetSraStructures(
      SraHeaderFilter& filter) const {
    std::vector<SraHeaderDataViewType> result;
    for (absl::optional<View> sra_header = GetFirstSraStructure(); sra_header;
         sra_header = GetNextSraStructure(*sra_header)) {
      if (filter.Filter(*sra_header)) {
        result.push_back(SraHeaderDataViewType(sra_header->BackingStorage()));
      }
    }
    return result;
  }

  // Get the SraHeaderDataViewType structures following the table that match
  // header type and are at least `SraHeaderDataViewType::SizeInBytes()` bytes
  // in size.
  template <typename SraHeaderDataViewType, uint8_t header_type>
  std::vector<SraHeaderDataViewType> GetSraStructuresByTypeAndMinSize() const {
    SraHeaderFilterByTypeAndMinSize filter_by_type_and_min_size(
        header_type, SraHeaderDataViewType::SizeInBytes());
    std::vector<View> structures =
        GetSraStructures(filter_by_type_and_min_size);
    std::vector<SraHeaderDataViewType> result;
    result.reserve(structures.size());
    std::transform(structures.begin(), structures.end(),
                   std::back_inserter(result), [](View view) {
                     return SraHeaderDataViewType(view.BackingStorage());
                   });
    return result;
  }

  // Get the SRA structures following the table that match `header_type`.  If
  // `only_enabled` is true, the returned structures have the `enabled_flag` set
  // in the `SraHeaderType.flag` field.
  template <typename SraHeaderDataViewType, uint8_t header_type>
  std::vector<SraHeaderDataViewType> GetSraStructuresByType(
      const bool only_enabled, const uint32_t enabled_flag) const {
    if (only_enabled) {
      SraHeaderFilterByEnabled<SraHeaderDataViewType> filter_by_enabled(
          header_type, SraHeaderDataViewType::SizeInBytes(), enabled_flag);
      return GetSraStructures<SraHeaderDataViewType>(filter_by_enabled);
    }
    return GetSraStructuresByTypeAndMinSize<SraHeaderDataViewType,
                                            header_type>();
  }

  virtual SystemDescriptionTableHeaderView GetHeaderView() const {
    return MakeSystemDescriptionTableHeaderView(table_data_, header_size_);
  }

  std::unique_ptr<SystemDescriptionTable> CreateTable() const {
    return SystemDescriptionTable::CreateTableFromData(
        table_data_, GetHeaderView().length().Read());
  }

  virtual uint32_t header_size() const { return header_size_; }

 protected:
  // Validate the table signature.
  // Readers classes specific to system description tables should override
  // this method.
  virtual bool ValidateSignature() const = 0;

  // Validate the table revision to ensure that it's supported by this class.
  // Readers classes specific to system description tables should override
  // this method.
  virtual bool ValidateRevision() const = 0;

 private:
  const char* table_data_;
  // Size of the table header prior to static resource allocation entries.
  // This is header_->length for tables without static resource allocation
  // entries.
  uint32_t header_size_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_ACPI_SYSTEM_DESCRIPTION_TABLE_H_
