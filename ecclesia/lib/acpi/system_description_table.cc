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

#include "ecclesia/lib/acpi/system_description_table.h"

#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <utility>

#include "absl/cleanup/cleanup.h"
#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/acpi/system_description_table.emb.h"
#include "ecclesia/lib/acpi/system_description_table.h"
#include "ecclesia/lib/logging/logging.h"

namespace ecclesia {

uint8_t SystemDescriptionTable::CalculateChecksum() const {
  uint8_t checksum = 0;
  const uint32_t table_size = GetHeaderView().length().Read();
  for (uint32_t i = 0; i < table_size; i++) {
    checksum += table_data_[i];
  }
  return checksum;
}

std::string SystemDescriptionTable::GetSignatureString() const {
  uint32_t signature = GetHeaderView().signature().Read();
  std::string signature_string(reinterpret_cast<const char *>(&signature),
                               sizeof(signature));
  return signature_string;
}

absl::StatusOr<std::unique_ptr<SystemDescriptionTable>>
SystemDescriptionTable::ReadFromFile(absl::string_view sysfs_acpi_table_path) {
  std::ifstream file(std::string(sysfs_acpi_table_path).c_str());
  if (!file.is_open()) {
    return absl::NotFoundError(
        absl::StrFormat("unable to open %s", sysfs_acpi_table_path));
  }
  std::string table_data;
  std::string buf(4096, '\0');
  while (file.read(buf.data(), buf.size())) {
    table_data.append(buf, 0, file.gcount());
  }
  table_data.append(buf, 0, file.gcount());
  size_t table_data_size = table_data.size();

  if (table_data_size < kHeaderSize) {
    return absl::InternalError(
        absl::StrFormat("%s too small, unable to read %u bytes.",
                        sysfs_acpi_table_path, kHeaderSize));
  }

  // Can't use make_unique() because the constructor is private.
  std::unique_ptr<SystemDescriptionTable> result(new SystemDescriptionTable);
  result->table_data_ = std::move(table_data);

  // Make a local header view that is writeable.
  auto header_view =
      MakeSystemDescriptionTableHeaderView(result->table_data_.data(),
                                           result->table_data_.size());
  if (header_view.length().Read() > table_data_size) {
    WarningLog() << absl::StrFormat(
        "Table %s specified a size %d larger than the data read from "
        "%s %u.",
        result->GetSignatureString(), header_view.length().Read(),
        sysfs_acpi_table_path, table_data_size);
    header_view.length().Write(table_data_size);
  }

  return result;
}

std::unique_ptr<SystemDescriptionTable>
SystemDescriptionTable::CreateTableFromData(const char* data, size_t size) {
  // Can't use make_unique() because the constructor is private.
  std::unique_ptr<SystemDescriptionTable> result(new SystemDescriptionTable);
  result->table_data_ = std::string(data, size);
  return result;
}

bool SraHeaderDescriptor::Validate(SraHeaderView header_view,
                                   const uint8_t expected_type,
                                   const uint8_t minimum_size,
                                   absl::string_view structure_name) {
  Check(minimum_size >= SraHeaderView::SizeInBytes(),
        "Not enough bytes for SRA header");
  uint8_t type = header_view.struct_type().Read();
  if (type != expected_type) {
    ErrorLog() << absl::StrFormat(
        "Unexpected header type %d vs. %d for %s structure %p.", type,
        expected_type, structure_name, header_view.BackingStorage().data());
    return false;
  }
  uint8_t length = header_view.length().Read();
  if (length < minimum_size) {
    ErrorLog() << absl::StrFormat(
        "%s structure %p too small %d vs. %d bytes.", structure_name,
        header_view.BackingStorage().data(), length, minimum_size);
    return false;
  }
  return true;
}

bool SraHeaderDescriptor::ValidateMaximumSize(SraHeaderView header_view,
                                              const uint32_t maximum_sra_size) {
  uint8_t length = header_view.length().Read();
  if (header_view.SizeInBytes() > maximum_sra_size ||
      length > maximum_sra_size) {
    ErrorLog() << absl::StrFormat(
        "Static resource allocation structure size %d "
        "exceeds the maximum size %d",
        length, static_cast<int>(maximum_sra_size));
    return false;
  }
  return true;
}

}  // namespace ecclesia
