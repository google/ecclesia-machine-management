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

#ifndef ECCLESIA_MAGENT_SYSMODEL_INDUS_NVME_H_
#define ECCLESIA_MAGENT_SYSMODEL_INDUS_NVME_H_

#include <vector>

#include "absl/status/statusor.h"
#include "absl/types/optional.h"
#include "ecclesia/lib/io/pci/discovery.h"
#include "ecclesia/magent/sysmodel/x86/nvme.h"

namespace ecclesia {

class IndusNvmeDiscover : public NvmeDiscoverInterface {
 public:
  explicit IndusNvmeDiscover(PciTopologyInterface *pci_topology)
      : pci_topology_(pci_topology) {}

  absl::StatusOr<std::vector<NvmeLocation>> GetAllNvmeLocations()
      const override;

 private:
  PciTopologyInterface *const pci_topology_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_SYSMODEL_INDUS_NVME_H_
