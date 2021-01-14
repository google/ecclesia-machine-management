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

#include "ecclesia/magent/lib/nvme/identify_namespace.h"

#include <cstdint>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "absl/numeric/int128.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/codec/endian.h"
#include "ecclesia/magent/lib/nvme/nvme_types.emb.h"
#include "runtime/cpp/emboss_cpp_util.h"
#include "runtime/cpp/emboss_prelude.h"

namespace ecclesia {

absl::StatusOr<IdentifyNamespace> IdentifyNamespace::Parse(
    absl::string_view buf) {
  if (buf.size() != IdentifyNamespaceFormat::IntrinsicSizeInBytes()) {
    return absl::InvalidArgumentError(
        "Buffer was the wrong size for an IdentifyNamespace command.");
  }

  auto idns = MakeIdentifyNamespaceFormatView(&buf);
  if (!idns.IsComplete()) {
    return absl::InvalidArgumentError(
        "Buffer was the wrong size for an IdentifyNamespace command.");
  }

  // Bits 3:0 indicates the supported LBA formats. Bit 4 indicate if
  // metadata is transferred at the end of the data LBA.
  const auto lba_format_index = (idns.formatted_lba_size().Read()) & 0xf;
  if (lba_format_index > IdentifyNamespaceFormat::lba_format_capacity()) {
    return absl::OutOfRangeError(absl::StrCat(
        "Namespace had invalid formatted_lba_size index: ", lba_format_index));
  }
  auto lba_format = idns.lba_format()[lba_format_index];
  uint8_t data_size_log2 = lba_format.data_size().Read();
  const uint64_t lba_bytes =
      (data_size_log2 > 0) ? (1ULL << data_size_log2) : 0;

  IdentifyNamespace id;
  id.capacity_bytes = idns.capacity().Read() * lba_bytes;
  id.formatted_lba_size_bytes = lba_bytes;
  id.namespace_guid =
      BigEndian::Load128(idns.namespace_guid().BackingStorage().data());

  return id;
}

absl::StatusOr<std::vector<LBAFormatView>>
IdentifyNamespace::GetSupportedLbaFormats(absl::string_view buf) {
  if (buf.size() != IdentifyNamespaceFormat::IntrinsicSizeInBytes()) {
    return absl::InvalidArgumentError(
        "Buffer was the wrong size for an IdentifyNamespace command.");
  }

  auto idns = MakeIdentifyNamespaceFormatView(&buf);
  std::vector<LBAFormatView> supported_formats;
  for (const auto &lba_format : idns.lba_format()) {
    if (lba_format.data_size().Read() == 0) break;
    supported_formats.push_back(lba_format);
  }

  if (supported_formats.empty()) {
    return absl::NotFoundError("Drive reported 0 supported formats.");
  }
  return supported_formats;
}

bool operator!=(const IdentifyNamespace &lhs, const IdentifyNamespace &rhs) {
  return memcmp(&lhs, &rhs, sizeof(lhs));
}

bool operator==(const IdentifyNamespace &lhs, const IdentifyNamespace &rhs) {
  return !(lhs != rhs);
}

absl::StatusOr<std::set<uint32_t>> ParseIdentifyListNamespace(
    absl::string_view buf) {
  if (buf.size() != IdentifyListNamespaceFormat::IntrinsicSizeInBytes()) {
    return absl::InvalidArgumentError(
        "Input buffer was not the correct size for an IdentifyListNamespaces "
        "command.");
  }

  auto idnsf = MakeIdentifyListNamespaceFormatView(&buf);
  std::set<uint32_t> valid_nsids;
  for (const auto &nsid_view : idnsf.nsid()) {
    uint32_t nsid = nsid_view.Read();
    if (nsid == 0) break;
    valid_nsids.insert(nsid);
  }
  return valid_nsids;
}

}  // namespace ecclesia
