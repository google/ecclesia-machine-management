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

#ifndef ECCLESIA_MAGENT_REDFISH_INDUS_SERVICE_ROOT_H_
#define ECCLESIA_MAGENT_REDFISH_INDUS_SERVICE_ROOT_H_

#include <string>

#include "ecclesia/magent/redfish/common/service_root.h"
#include "json/value.h"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"

namespace ecclesia {

class ServiceRootIndus : public ServiceRoot {
 public:
  ServiceRootIndus() : ServiceRoot() {}

 private:
  void Get(tensorflow::serving::net_http::ServerRequestInterface *req,
           const ParamsType &params) override {
    Json::Value json = BasicRoot();
    json[kEventService][kOdataId] = kEventServiceUri;
    json[kOem][kGoogle][kTopologyRepresentation] = kTopologyRepresentationV1;
    JSONResponseOK(json, req);
  }
};

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_REDFISH_INDUS_SERVICE_ROOT_H_
