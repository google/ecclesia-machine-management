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

#ifndef ECCLESIA_MAGENT_REDFISH_INDUS_PCIE_SLOTS_H_
#define ECCLESIA_MAGENT_REDFISH_INDUS_PCIE_SLOTS_H_

#include <string>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "ecclesia/magent/redfish/core/redfish_keywords.h"
#include "ecclesia/magent/redfish/core/resource.h"
#include "ecclesia/magent/sysmodel/x86/sysmodel.h"
#include "single_include/nlohmann/json.hpp"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"

namespace ecclesia {
class PCIeSlots : public Resource {
 public:
  explicit PCIeSlots(SystemModel *system_model)
      : Resource(kPCIeSlotsUri), system_model_(system_model) {}

 private:
  void Get(tensorflow::serving::net_http::ServerRequestInterface *req,
           const ParamsType &params) override {
    nlohmann::json json;
    AddStaticFields(&json);

    JSONResponseOK(json, req);
  }

  void AddStaticFields(nlohmann::json *json) {
    (*json)[kOdataType] = "#PCIeSlots.v1_4_0.PCIeSlots";
    (*json)[kOdataId] = std::string(Uri());
    (*json)[kOdataContext] = "/redfish/v1/$metadata#PCIeSlots.PCIeSlots";
    (*json)[kName] = "PCIeSlots";
  }

  SystemModel *const system_model_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_REDFISH_INDUS_PCIE_SLOTS_H_
