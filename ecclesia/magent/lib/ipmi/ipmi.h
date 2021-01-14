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

// Library for retrieving data through ipmi

#ifndef ECCLESIA_MAGENT_LIB_IPMI_IPMI_H_
#define ECCLESIA_MAGENT_LIB_IPMI_IPMI_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

#include "absl/status/status.h"
#include "absl/types/span.h"

namespace ecclesia {

class IpmiInterface {
 public:
  struct EntityIdentifier {
    uint8_t entity_id;
    uint8_t entity_instance;

    bool operator==(const EntityIdentifier &other) const {
      return std::tie(entity_id, entity_instance) ==
             std::tie(other.entity_id, other.entity_instance);
    }
    bool operator!=(const EntityIdentifier &other) const {
      return !(*this == other);
    }
    template <typename H>
    friend H AbslHashValue(H h, const EntityIdentifier &e) {
      return H::combine(std::move(h), e.entity_id, e.entity_instance);
    }
  };

  // Data struct containing information uniquely identifying a fru exported by
  // BMC
  struct BmcFruInterfaceInfo {
    uint16_t record_id;
    EntityIdentifier entity;
    std::string name;
    // This fru_id can be used as identification in ReadFru method.
    uint16_t fru_id;
  };

  IpmiInterface() {}
  virtual ~IpmiInterface() {}

  virtual std::vector<BmcFruInterfaceInfo> GetAllFrus() = 0;

  // Reads FRU raw data
  // We will read bytes starting from offset from fru identified by fru_id
  // The number of bytes read is equal to the size of data.
  virtual absl::Status ReadFru(uint16_t fru_id, size_t offset,
                               absl::Span<unsigned char> data) = 0;

  // Gets the FRU size
  virtual absl::Status GetFruSize(uint16_t fru_id, uint16_t *size) = 0;
};

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_LIB_IPMI_IPMI_H_
