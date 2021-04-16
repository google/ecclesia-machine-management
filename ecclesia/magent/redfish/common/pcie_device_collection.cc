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

#include "ecclesia/magent/redfish/common/pcie_device_collection.h"

#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_cat.h"
#include "absl/types/optional.h"
#include "ecclesia/lib/io/pci/location.h"
#include "ecclesia/magent/redfish/core/redfish_keywords.h"
#include "ecclesia/magent/redfish/core/resource.h"
#include "ecclesia/magent/sysmodel/x86/sysmodel.h"
#include "json/value.h"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"

namespace ecclesia {

Json::Value GetPcieDeviceUrlsAsJsonArray(SystemModel *system_model) {
  Json::Value array(Json::arrayValue);

  std::vector<PciDbdfLocation> pcie_locations =
      system_model->GetPcieDeviceLocations();

  // The pcie_locations can contain multiple locations that correspond to the
  // same PCIe device (with different PCI functions). This hash set will filter
  // the locations and make sure only the unique PCIe devices are kept.
  absl::flat_hash_set<PciDbdLocation> pcie_dev_ids;
  for (const PciDbdfLocation &pcie_location : pcie_locations) {
    pcie_dev_ids.insert(PciDbdLocation(pcie_location));
  }

  for (const auto &pcie_dev_id : pcie_dev_ids) {
    Json::Value entry;
    entry[kOdataId] =
        absl::StrCat(kPCIeDeviceCollectionUri, "/", pcie_dev_id.ToString());
    array.append(entry);
  }

  return array;
}

void PCIeDeviceCollection::Get(
    tensorflow::serving::net_http::ServerRequestInterface *req,
    const ParamsType &params) {
  Json::Value json;
  AddStaticFields(&json);

  Json::Value members = GetPcieDeviceUrlsAsJsonArray(system_model_);
  json[kMembers] = members;
  // The members JSON has no reason to be non-array. But still check it for
  // safety.
  if (members.isArray()) {
    json[kMembersCount] = members.size();
  }

  JSONResponseOK(json, req);
}

}  // namespace ecclesia
