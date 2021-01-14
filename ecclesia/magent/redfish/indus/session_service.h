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

#ifndef ECCLESIA_MAGENT_REDFISH_INDUS_SESSION_SERVICE_H_
#define ECCLESIA_MAGENT_REDFISH_INDUS_SESSION_SERVICE_H_

#include <string>

#include "absl/strings/string_view.h"
#include "ecclesia/magent/redfish/core/json_helper.h"
#include "ecclesia/magent/redfish/core/redfish_keywords.h"
#include "ecclesia/magent/redfish/core/resource.h"
#include "json/value.h"
#include "tensorflow_serving/util/net_http/server/public/response_code_enum.h"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"

namespace ecclesia {

// Resource containing Session service properties for a Redfish Service,
// schema defined in DSP0268 and service requirements in DSP0266, Section 14.3.3
class SessionService : public Resource {
 public:
  explicit SessionService() : Resource(kSessionServiceUri) {}

 private:
  void Get(tensorflow::serving::net_http::ServerRequestInterface *req,
           const ParamsType &params) override {
    Json::Value json;
    AddStaticFields(&json);

    JSONResponseOK(json, req);
  }

  void Post(tensorflow::serving::net_http::ServerRequestInterface *req,
            const ParamsType &params) override {
    req->ReplyWithStatus(
        tensorflow::serving::net_http::HTTPStatusCode::UNAUTHORIZED);
  }

  // Update resource properties with standard, fixed values for required fields.
  void AddStaticFields(Json::Value *json) {
    (*json)[kOdataId] = std::string(Uri());
    (*json)[kOdataType] = "#SessionService.v1_1_6.SessionService";
    (*json)[kId] = kSessionService;
    (*json)[kName] = "Session Service";
    (*json)[kServiceEnabled] = false;
    auto *serviceStatus = GetJsonObject(json, kStatus);
    (*serviceStatus)[kState] = "Disabled";
    (*serviceStatus)[kHealth] = "OK";
    (*GetJsonObject(json, kSessions))[kOdataId] = kSessionsUri;
  }
};

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_REDFISH_INDUS_SESSION_SERVICE_H_
