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

#ifndef ECCLESIA_MAGENT_REDFISH_COMMON_STORAGE_CONTROLLER_COLLECTION_H_
#define ECCLESIA_MAGENT_REDFISH_COMMON_STORAGE_CONTROLLER_COLLECTION_H_

#include <string>

#include "absl/strings/str_cat.h"
#include "ecclesia/magent/redfish/core/index_resource.h"
#include "ecclesia/magent/redfish/core/json_helper.h"
#include "ecclesia/magent/redfish/core/redfish_keywords.h"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"

namespace ecclesia {

class StorageControllerCollection : public IndexResource<std::string> {
 public:
  explicit StorageControllerCollection()
      : IndexResource(kStorageControllerCollectionUriPattern) {}

 private:
  void Get(tensorflow::serving::net_http::ServerRequestInterface *req,
           const ParamsType &params) override {
    Json::Value json;

    json[kOdataType] =
        "#StorageControllerCollection.StorageControllerCollection";
    json[kOdataId] = std::string(req->uri_path());
    json[kOdataContext] =
        "/redfish/v1/"
        "$metadata#StorageControllerCollection.StorageControllerCollection";
    json[kName] = "Storage Controller Collection";

    auto *members = GetJsonArray(&json, kMembers);
    AppendCollectionMember(members, absl::StrCat(req->uri_path(), "/0"));
    json[kMembersCount] = 1;

    JSONResponseOK(json, req);
  }
};

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_REDFISH_COMMON_STORAGE_CONTROLLER_COLLECTION_H_
