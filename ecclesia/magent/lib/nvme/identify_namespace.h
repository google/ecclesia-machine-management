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

#ifndef ECCLESIA_MAGENT_LIB_NVME_IDENTIFY_NAMESPACE_H_
#define ECCLESIA_MAGENT_LIB_NVME_IDENTIFY_NAMESPACE_H_

#include <cstdint>
#include <set>
#include <vector>

#include "absl/numeric/int128.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ecclesia/magent/lib/nvme/nvme_types.emb.h"

namespace ecclesia {

// Class to decode NVM-Express IdentifyContoller object in Namespace mode.
//
// Rather than store the entire 4k structure which is almost entirely empty,
// we just store the interesting fields.
struct IdentifyNamespace {
  // Factory function to parse raw data buffer into IdentifyNamespace.
  // Returns a new IdentifyNamespace object or nullptr in case of error.
  //
  // Arguments:
  //   buf: Data buffer returned from IdentifyNamespace request.
  static absl::StatusOr<IdentifyNamespace> Parse(absl::string_view buf);

  // Parses the table of supported LBA formats from the device.  Order matters:
  // the index of each entry is the same as reported by the device, so it is
  // suitable for passing to CreateNamespace.
  //
  // Note that the returned values are only views into the underlying buffer and
  // so are only valid as long as the buffer is.
  static absl::StatusOr<std::vector<LBAFormatView>> GetSupportedLbaFormats(
      absl::string_view buf);

  // IdentifyNamespace data accessors, as defined by NVM-Express spec 1.3a,
  // section 5.15 'Identify command'.

  // Note *_bytes vars have been translated from NVMe format (LBAs) to bytes.

  // The usable capacity of the namespace: its "logcial" size.
  uint64_t capacity_bytes;

  // The number of bytes in each logical block.
  uint64_t formatted_lba_size_bytes;

  // A globally unique identifier for the partition.
  absl::uint128 namespace_guid;
};

// Compares all fields for equality.  Useful for std containers and tests.
bool operator==(const IdentifyNamespace& lhs, const IdentifyNamespace& rhs);
bool operator!=(const IdentifyNamespace& lhs, const IdentifyNamespace& rhs);

// Utility function to decode IdentifyController object in Namespace List mode.
absl::StatusOr<std::set<uint32_t>> ParseIdentifyListNamespace(
    absl::string_view buf);

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_LIB_NVME_IDENTIFY_NAMESPACE_H_
