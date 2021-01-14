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

#ifndef ECCLESIA_MAGENT_REDFISH_INDUS_SESSION_COLLECTION_H_
#define ECCLESIA_MAGENT_REDFISH_INDUS_SESSION_COLLECTION_H_

#include <string>

#include "absl/strings/string_view.h"
#include "ecclesia/magent/redfish/core/json_helper.h"
#include "ecclesia/magent/redfish/core/redfish_keywords.h"
#include "ecclesia/magent/redfish/core/resource.h"
#include "json/value.h"
#include "tensorflow_serving/util/net_http/server/public/response_code_enum.h"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"

namespace ecclesia {

// Collection of Session Resources for client authentication, defined in DSP0268
// See also DSP0266, Section 14.3.3 for details on Redfish session login
// authentication.
class SessionCollection : public Resource {
 public:
  explicit SessionCollection() : Resource(kSessionsUri) {}

 private:
  void Get(tensorflow::serving::net_http::ServerRequestInterface *req,
           const ParamsType &params) override {
    Json::Value json;
    AddStaticFields(&json);
    json[kMembersCount] = 0;
    GetJsonArray(&json, kMembers);

    JSONResponseOK(json, req);
  }

  void Post(tensorflow::serving::net_http::ServerRequestInterface *req,
            const ParamsType &params) override {
    req->ReplyWithStatus(
        tensorflow::serving::net_http::HTTPStatusCode::UNAUTHORIZED);
  }

  void AddStaticFields(Json::Value *json) {
    (*json)[kOdataId] = std::string(Uri());
    (*json)[kOdataType] = "#SessionCollection.SessionCollection";
    (*json)[kOdataContext] =
        "/redfish/v1/$metadata#SessionCollection.SessionCollection";
    (*json)[kName] = "Session Collection";
  }
};

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_REDFISH_INDUS_SESSION_COLLECTION_H_
