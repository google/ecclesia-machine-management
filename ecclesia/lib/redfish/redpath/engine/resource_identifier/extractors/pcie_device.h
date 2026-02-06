/*
 * Copyright 2026 Google LLC
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

#ifndef ECCLESIA_LIB_REDFISH_REDPATH_ENGINE_RESOURCE_IDENTIFIER_EXTRACTORS_PCIE_DEVICE_H_
#define ECCLESIA_LIB_REDFISH_REDPATH_ENGINE_RESOURCE_IDENTIFIER_EXTRACTORS_PCIE_DEVICE_H_

#include "absl/log/die_if_null.h"
#include "absl/status/statusor.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/redpath/engine/resource_identifier/extractor.h"

namespace ecclesia {

// This class is used to extract the identifier of a PCIeDevice. This is done by
// getting the identifier of the parent chassis.
class PcieDeviceIdentifierExtractor : public IdentifierExtractorIntf {
 public:
  explicit PcieDeviceIdentifierExtractor(
      const IdentifierExtractorIntf* chassis_extractor)
      : chassis_extractor_(*ABSL_DIE_IF_NULL(chassis_extractor)) {}

  absl::StatusOr<IdentifierExtractorIntf::ResourceIdentifier> Extract(
      const RedfishObject& obj, const Deps& deps) const override;

 private:
  const IdentifierExtractorIntf& chassis_extractor_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_REDPATH_ENGINE_RESOURCE_IDENTIFIER_EXTRACTORS_PCIE_DEVICE_H_
