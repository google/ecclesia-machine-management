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

#include "ecclesia/lib/redfish/redpath/engine/resource_identifier/extractors/pcie_device.h"

#include <memory>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/property_definitions.h"
#include "ecclesia/lib/redfish/redpath/engine/resource_identifier/extractor.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {

absl::StatusOr<IdentifierExtractorIntf::ResourceIdentifier>
PcieDeviceIdentifierExtractor::Extract(const RedfishObject& obj,
                                       const Deps& deps) const {
  if (deps.interface == nullptr) {
    return absl::InvalidArgumentError("Redfish interface is null");
  }

  nlohmann::json redfish_json = obj.GetContentAsJson();
  auto odata_type_it = redfish_json.find(PropertyOdataType::Name);
  if (odata_type_it == redfish_json.end() || !odata_type_it->is_string() ||
      !absl::EndsWith(odata_type_it->get<std::string>(),
                      kRfPropertyPcieDevice)) {
    return absl::NotFoundError("PCIeDevice not found in query result data");
  }

  auto links_it = redfish_json.find(kRfPropertyLinks);
  if (links_it == redfish_json.end()) {
    return absl::NotFoundError("Links not found in query result data");
  }

  auto chassis_it = links_it->find(kRfPropertyChassis);
  if (chassis_it == links_it->end()) {
    return absl::NotFoundError("Chassis not found in query result data");
  }
  if (!chassis_it->is_array() || chassis_it->empty()) {
    return absl::NotFoundError("Chassis is not a array or is empty");
  }

  nlohmann::json chassis_link_obj = chassis_it->at(0);
  auto odata_id_it = chassis_link_obj.find(PropertyOdataId::Name);
  if (odata_id_it == chassis_link_obj.end() || !odata_id_it->is_string()) {
    return absl::NotFoundError("OdataId not found in Chassis");
  }

  std::string chassis_uri = odata_id_it->get<std::string>();
  // It's ok to use optional freshness here because we are only interested in
  // the identifier of the related item which is not expected to change once
  // created.
  GetParams params{.freshness = GetParams::Freshness::kOptional};
  if (deps.timeout_manager != nullptr) {
    params.timeout_manager = deps.timeout_manager;
  }
  RedfishVariant variant = deps.interface->CachedGetUri(chassis_uri, params);
  if (!variant.status().ok()) {
    return absl::FailedPreconditionError(
        absl::StrCat("Failed to get chassis: ", chassis_uri,
                     " with status: ", variant.status().message()));
  }

  std::unique_ptr<RedfishObject> chassis_obj = variant.AsObject();
  if (chassis_obj == nullptr) {
    return absl::NotFoundError("Chassis object is null");
  }

  return chassis_extractor_.Extract(*chassis_obj, deps);
}

}  // namespace ecclesia
