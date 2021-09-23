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

#ifndef ECCLESIA_MAGENT_REDFISH_COMMON_MEMORY_H_
#define ECCLESIA_MAGENT_REDFISH_COMMON_MEMORY_H_

#include <cstdint>
#include <string>
#include <type_traits>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "ecclesia/magent/redfish/core/index_resource.h"
#include "ecclesia/magent/redfish/core/json_helper.h"
#include "ecclesia/magent/redfish/core/redfish_keywords.h"
#include "ecclesia/magent/redfish/core/resource.h"
#include "ecclesia/magent/sysmodel/x86/dimm.h"
#include "ecclesia/magent/sysmodel/x86/sysmodel.h"
#include "single_include/nlohmann/json.hpp"
#include "tensorflow_serving/util/net_http/server/public/response_code_enum.h"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"

namespace ecclesia {

class Memory : public IndexResource<int> {
 public:
  explicit Memory(SystemModel *system_model)
      : IndexResource(kMemoryUriPattern), system_model_(system_model) {}

 private:
  void Get(tensorflow::serving::net_http::ServerRequestInterface *req,
           const ParamsType &params) override {
    // Expect to be passed in the dimm index
    if (!ValidateResourceIndex(params[0], system_model_->NumDimms())) {
      req->ReplyWithStatus(
          tensorflow::serving::net_http::HTTPStatusCode::NOT_FOUND);
      return;
    }
    // Fill in the json response
    auto dimm = system_model_->GetDimm(std::get<int>(params[0]));
    if (!dimm.has_value()) {
      req->ReplyWithStatus(
          tensorflow::serving::net_http::HTTPStatusCode::NOT_FOUND);
      return;
    }
    const auto &dimm_info = dimm->GetDimmInfo();
    nlohmann::json json;
    json[kOdataType] = "#Memory.v1_8_0.Memory";
    json[kOdataId] = std::string(req->uri_path());
    json[kOdataContext] = "/redfish/v1/$metadata#Memory.Memory";
    json[kId] = dimm_info.slot_name;
    json[kName] = dimm_info.slot_name;
    if (dimm_info.present) {
      json[kCapacityMiB] = dimm_info.size_mb;
      json[kLogicalSizeMiB] = dimm_info.size_mb;
      json[kManufacturer] = dimm_info.manufacturer;
      json[kMemoryDeviceType] = dimm_info.type;
      json[kOperatingSpeedMhz] = dimm_info.configured_speed_mhz;
      json[kPartNumber] = dimm_info.part_number;
      json[kSerialNumber] = dimm_info.serial_number;
      auto *assembly = GetJsonObject(&json, kAssembly);
      (*assembly)[kOdataId] = absl::StrCat(req->uri_path(), "/", kAssembly);
    }
    auto *metrics = GetJsonObject(&json, kMetrics);
    (*metrics)[kOdataId] = absl::StrCat(req->uri_path(), "/", kMemoryMetrics);

    auto *status = GetJsonObject(&json, kStatus);
    (*status)[kState] = dimm_info.present ? kEnabled : kAbsent;

    JSONResponseOK(json, req);
  }

  SystemModel *const system_model_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_REDFISH_COMMON_MEMORY_H_
