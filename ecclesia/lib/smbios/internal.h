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

#ifndef ECCLESIA_LIB_SMBIOS_INTERNAL_H_
#define ECCLESIA_LIB_SMBIOS_INTERNAL_H_

#include <assert.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/lib/smbios/structures.emb.h"

namespace ecclesia {

// Information regarding a SMBIOS structure in memory
struct SmbiosStructureInfo {
  uint8_t *formatted_data_start;
  uint8_t *unformed_data_start;
  std::size_t structure_size;
};

/* A single SMBIOS structure */
class TableEntry {
 public:
  // The constructor takes a reference to the SmbiosStructureInfo that has
  // pointers to the different sections of an SMBIOS table entry
  TableEntry(const SmbiosStructureInfo &info) {
    data_ = std::vector(info.formatted_data_start, info.unformed_data_start);
    // Extract the list of character strings from the unformed section
    char *start = reinterpret_cast<char *>(info.unformed_data_start);
    while (*start != '\0') {
      strings_.emplace_back(start);
      // Move to the next character string
      // Since std::string constructed from a char* doesn't include the
      // terminating null char, add 1 to size
      start += strings_.back().size() + 1;
    }
  }

  // Need to allow move since the table entries are stored in a vector.
  // Note that any previously obtained view objects should still be valid, since
  // the underlaying storage is std::vector<>.
  TableEntry(const TableEntry &) = delete;
  TableEntry &operator=(const TableEntry &) = delete;
  TableEntry(TableEntry &&) = default;
  TableEntry &operator=(TableEntry &&) = default;

  SmbiosStructureView GetSmbiosStructureView() const {
    auto message_view = MakeSmbiosStructureView(data_.data(), data_.size());
    assert(message_view.Ok());
    return message_view;
  }

  // Get a string form the unformed section of the SMBIOS structure. Note that
  // asking for the 0th string will return an empty string. The specification
  // requires non-zero string numbers
  absl::string_view GetString(std::size_t num) const {
    if (num == 0) return absl::string_view();
    if (num > strings_.size()) {
      ErrorLog() << "string number " << num << " out of bounds";
      return absl::string_view();
    }
    return strings_[num - 1];
  }

 private:
  std::vector<uint8_t> data_;  // Formatted data
  std::vector<std::string> strings_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_SMBIOS_INTERNAL_H_
