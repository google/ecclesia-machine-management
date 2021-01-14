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

#ifndef ECCLESIA_MAGENT_REDFISH_INDUS_DRIVE_H_
#define ECCLESIA_MAGENT_REDFISH_INDUS_DRIVE_H_

#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>

#include "absl/numeric/int128.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/magent/lib/nvme/identify_controller.h"
#include "ecclesia/magent/redfish/core/index_resource.h"
#include "ecclesia/magent/redfish/core/json_helper.h"
#include "ecclesia/magent/redfish/core/redfish_keywords.h"
#include "ecclesia/magent/redfish/core/resource.h"
#include "ecclesia/magent/sysmodel/x86/nvme.h"
#include "ecclesia/magent/sysmodel/x86/sysmodel.h"
#include "json/value.h"
#include "tensorflow_serving/util/net_http/server/public/response_code_enum.h"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"

namespace ecclesia {

class Drive : public IndexResource<std::string> {
 public:
  explicit Drive(SystemModel *system_model)
      : IndexResource(kDriveUriPattern), system_model_(system_model) {}

 private:
  void Get(tensorflow::serving::net_http::ServerRequestInterface *req,
           const ParamsType &params) override {
    std::string storage_index = std::get<std::string>(params[0]);
    auto drive = system_model_->GetNvmeByPhysLocation(storage_index);
    if (!drive.has_value()) {
      req->ReplyWithStatus(
          tensorflow::serving::net_http::HTTPStatusCode::NOT_FOUND);
      return;
    }
    // Fill in the json response
    Json::Value json;
    json[kOdataType] = "#Drive.v1_11_0.Drive";
    json[kOdataId] = std::string(req->uri_path());
    json[kOdataContext] = "/redfish/v1/$metadata#Drive.Drive";
    NvmeInterface *nvme_intf = drive->access_intf;
    json[kName] = nvme_intf->GetKernelName();
    json[kMediaType] = "SSD";
    json[kProtocol] = "NVMe";
    auto maybe_identity = nvme_intf->Identify();
    if (maybe_identity.ok()) {
      json[kPartNumber] = maybe_identity.value()->model_number();
      json[kSerialNumber] = maybe_identity.value()->serial_number();
      json[kCapacityBytes] =
          static_cast<uint64_t>(maybe_identity.value()->total_capacity());
    } else {
      ErrorLog() << "Failed to get Identity for Drive: "
                 << nvme_intf->GetKernelName();
    }

    auto *assembly = GetJsonObject(&json, kAssembly);
    (*assembly)[kOdataId] = absl::StrCat(req->uri_path(), "/", kAssembly);

    JSONResponseOK(json, req);
  }

  SystemModel *const system_model_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_REDFISH_INDUS_DRIVE_H_
