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

#ifndef ECCLESIA_MAGENT_SYSMODEL_X86_PCI_STORAGE_H_
#define ECCLESIA_MAGENT_SYSMODEL_X86_PCI_STORAGE_H_

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "ecclesia/lib/io/pci/location.h"

namespace ecclesia {

struct PciStorageLocation {
  PciDbdfLocation pci_location = PciDbdfLocation::Make<0, 0, 0, 0>();
  std::string physical_location;

  bool operator==(const PciStorageLocation &other) const {
    return std::tie(pci_location, physical_location) ==
           std::tie(other.pci_location, other.physical_location);
  }
  bool operator!=(const PciStorageLocation &other) const {
    return !(*this == other);
  }
};

class PciStorageDiscoverInterface {
 public:
  virtual ~PciStorageDiscoverInterface() = default;
  virtual absl::StatusOr<std::vector<PciStorageLocation>>
  GetAllPciStorageLocations() const = 0;
};

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_SYSMODEL_X86_PCI_STORAGE_H_
