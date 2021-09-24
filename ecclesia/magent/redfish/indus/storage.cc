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

#include "ecclesia/magent/redfish/indus/storage.h"

#include <optional>
#include <string>
#include <type_traits>
#include <variant>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "ecclesia/magent/redfish/core/redfish_keywords.h"
#include "ecclesia/magent/redfish/core/resource.h"
#include "ecclesia/magent/sysmodel/x86/nvme.h"
#include "ecclesia/magent/sysmodel/x86/sysmodel.h"
#include "single_include/nlohmann/json.hpp"
#include "tensorflow_serving/util/net_http/server/public/response_code_enum.h"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"

namespace ecclesia {

void Storage::Get(tensorflow::serving::net_http::ServerRequestInterface *req,
                  const ParamsType &params) {
  std::string storage_index = std::get<std::string>(params[0]);
  nlohmann::json json;
  json[kOdataType] = "#Storage.v1_9_0.Storage";
  json[kOdataId] = std::string(req->uri_path());
  json[kOdataContext] = "/redfish/v1/$metadata#Storage.Storage";
  auto nvme_plugin = system_model_->GetNvmeByPhysLocation(storage_index);
  if (!nvme_plugin.has_value()) {
    req->ReplyWithStatus(
        tensorflow::serving::net_http::HTTPStatusCode::NOT_FOUND);
    return;
  }
  // Fill in the json response
  json[kName] = nvme_plugin->access_intf->GetKernelName();

  // Create Controllers
  nlohmann::json controllers_obj;
  controllers_obj[kOdataId] = absl::StrCat(req->uri_path(), "/Controllers");
  json[kControllers] = controllers_obj;

  AddDrives(&json, req->uri_path());
  JSONResponseOK(json, req);
}

}  // namespace ecclesia
