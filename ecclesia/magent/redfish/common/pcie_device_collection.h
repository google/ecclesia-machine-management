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

#ifndef ECCLESIA_MAGENT_REDFISH_COMMON_PCIE_DEVICE_COLLECTION_H_
#define ECCLESIA_MAGENT_REDFISH_COMMON_PCIE_DEVICE_COLLECTION_H_

#include <optional>
#include <string>

#include "absl/strings/string_view.h"
#include "ecclesia/magent/redfish/core/redfish_keywords.h"
#include "ecclesia/magent/redfish/core/resource.h"
#include "ecclesia/magent/sysmodel/x86/sysmodel.h"
#include "single_include/nlohmann/json.hpp"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"

namespace ecclesia {

// A helper function to get a JSON array of PCIeDevice URLs from the system
// model. The return value looks like:
// [
//   {
//      "@odata.id" : "/redfish/v1/Systems/system/PCIeDevices/0000:af:00"
//   },
//   {
//      "@odata.id" : "/redfish/v1/Systems/system/PCIeDevices/0000:ae:02"
//   },
//.  ...
// ]
nlohmann::json GetPcieDeviceUrlsAsJsonArray(SystemModel *system_model);

class PCIeDeviceCollection : public Resource {
 public:
  PCIeDeviceCollection(SystemModel *system_model)
      : Resource(kPCIeDeviceCollectionUri), system_model_(system_model) {}

 private:
  void Get(tensorflow::serving::net_http::ServerRequestInterface *req,
           const ParamsType &params) override;

  void AddStaticFields(nlohmann::json *json) {
    (*json)[kOdataType] = "#PCIeDeviceCollection.PCIeDeviceCollection";
    (*json)[kOdataId] = std::string(Uri());
    (*json)[kOdataContext] =
        "/redfish/v1/"
        "$metadata#PCIeDeviceCollection.PCIeDeviceCollection";
    (*json)[kName] = "PCIe Device Collection";
  }

  SystemModel *const system_model_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_REDFISH_COMMON_PCIE_DEVICE_COLLECTION_H_
