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

#ifndef ECCLESIA_MAGENT_REDFISH_COMMON_STORAGE_COLLECTION_H_
#define ECCLESIA_MAGENT_REDFISH_COMMON_STORAGE_COLLECTION_H_

#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "ecclesia/magent/redfish/core/json_helper.h"
#include "ecclesia/magent/redfish/core/redfish_keywords.h"
#include "ecclesia/magent/redfish/core/resource.h"
#include "ecclesia/magent/sysmodel/x86/pci_storage.h"
#include "ecclesia/magent/sysmodel/x86/sysmodel.h"
#include "single_include/nlohmann/json.hpp"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"
#include "re2/re2.h"

namespace ecclesia {

class StorageCollection : public Resource {
 public:
  explicit StorageCollection(SystemModel *system_model)
      : Resource(kStorageCollectionUri), system_model_(system_model) {}

 private:
  void Get(tensorflow::serving::net_http::ServerRequestInterface *req,
           const ParamsType &params) override {
    nlohmann::json json;
    AddStaticFields(&json);
    std::vector<std::string> nvme_locations =
        system_model_->GetNvmePhysLocations();
    auto *members = GetJsonArray(&json, kMembers);
    int num_members = 0;
    for (const auto &location : nvme_locations) {
      if (RE2::FullMatch(location, ".*U2_\\d")) {
        AppendCollectionMember(members, absl::StrCat(Uri(), "/", location));
        num_members++;
      }
    }

    std::vector<PciStorageLocation> storage_locations =
        system_model_->GetPciStorageLocations();
    for (const auto &loc : storage_locations) {
      AppendCollectionMember(members,
                             absl::StrCat(Uri(), "/", loc.physical_location));
      num_members++;
    }

    json[kMembersCount] = num_members;
    JSONResponseOK(json, req);
  }

  void AddStaticFields(nlohmann::json *json) {
    (*json)[kOdataType] = "#StorageCollection.StorageCollection";
    (*json)[kOdataId] = std::string(Uri());
    (*json)[kOdataContext] =
        "/redfish/v1/$metadata#StorageCollection.StorageCollection";
    (*json)[kName] = "Storage Collection";
  }

  SystemModel *const system_model_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_REDFISH_COMMON_STORAGE_COLLECTION_H_
