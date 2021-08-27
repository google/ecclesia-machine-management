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

#ifndef ECCLESIA_MAGENT_REDFISH_COMMON_SERVICE_ROOT_H_
#define ECCLESIA_MAGENT_REDFISH_COMMON_SERVICE_ROOT_H_

#include <string>

#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/property_values.h"
#include "ecclesia/magent/redfish/core/json_helper.h"
#include "ecclesia/magent/redfish/core/redfish_keywords.h"
#include "ecclesia/magent/redfish/core/resource.h"
#include "ecclesia/magent/redfish/core/service_root_resource.h"
#include "single_include/nlohmann/json.hpp"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"

namespace ecclesia {

class ServiceRoot : public ServiceRootResource {
 public:
  ServiceRoot() : ServiceRootResource(kServiceRootUri) {}

 protected:
  nlohmann::json BasicRoot() const {
    nlohmann::json json;
    json[kOdataType] = "#ServiceRoot.v1_5_0.ServiceRoot";
    json[kOdataId] = std::string(Uri());
    json[kOdataContext] = "/redfish/v1/$metadata#ServiceRoot.ServiceRoot";
    json[kId] = "RootService";
    json[kName] = "Root Service";
    json["RedfishVersion"] = "1.6.1";
    (*GetJsonObject(GetJsonObject(&json, kOem),
                    kGoogle))[kTopologyRepresentation] =
        kTopologyRepresentationV1;
    (*GetJsonObject(&json, kSystems))[kOdataId] = kComputerSystemCollectionUri;
    (*GetJsonObject(&json, kChassis))[kOdataId] = kChassisCollectionUri;
    (*GetJsonObject(&json, kUpdateService))[kOdataId] = kUpdateServiceUri;
    (*GetJsonObject(&json, kSessionService))[kOdataId] = kSessionServiceUri;
    nlohmann::json *links = GetJsonObject(&json, kLinks);
    (*GetJsonObject(links, kSessions))[kOdataId] = kSessionsUri;
    return json;
  }

 private:
  void Get(tensorflow::serving::net_http::ServerRequestInterface *req,
           const ParamsType &params) override {
    JSONResponseOK(BasicRoot(), req);
  }
};

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_REDFISH_COMMON_SERVICE_ROOT_H_
