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

#ifndef ECCLESIA_MAGENT_REDFISH_INDUS_STORAGE_H_
#define ECCLESIA_MAGENT_REDFISH_INDUS_STORAGE_H_

#include <optional>
#include <string>
#include <variant>

#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "ecclesia/magent/redfish/core/index_resource.h"
#include "ecclesia/magent/redfish/core/json_helper.h"
#include "ecclesia/magent/redfish/core/redfish_keywords.h"
#include "ecclesia/magent/redfish/core/resource.h"
#include "ecclesia/magent/sysmodel/x86/sysmodel.h"
#include "single_include/nlohmann/json.hpp"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"

namespace ecclesia {

class Storage : public IndexResource<std::string> {
 public:
  explicit Storage(SystemModel *system_model)
      : IndexResource(kStorageUriPattern), system_model_(system_model) {}

 private:
  void Get(tensorflow::serving::net_http::ServerRequestInterface *req,
           const ParamsType &params) override;

  // This helper function add Drive links to the Storage resource. We assume
  // there is only one drive for each Storage, even though the "Drives" array
  // supports mutiple elements.
  static void AddDrives(nlohmann::json *json, absl::string_view url) {
    auto *drives = GetJsonArray(json, kDrives);
    nlohmann::json drive;
    drive[kOdataId] = absl::StrFormat("%s/%s/%d", url, kDrives, 0);
    drives->push_back(drive);
  }

  SystemModel *const system_model_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_REDFISH_INDUS_STORAGE_H_
