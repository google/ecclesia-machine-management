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

#ifndef ECCLESIA_MAGENT_REDFISH_INDUS_EVENT_SERVICE_CLEAR_H_
#define ECCLESIA_MAGENT_REDFISH_INDUS_EVENT_SERVICE_CLEAR_H_

#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "ecclesia/magent/redfish/core/json_helper.h"
#include "ecclesia/magent/redfish/core/redfish_keywords.h"
#include "ecclesia/magent/redfish/core/resource.h"
#include "ecclesia/magent/redfish/indus/event_service.h"
#include "json/value.h"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"

namespace ecclesia {

class EventServiceClear : public Resource {
 public:
  EventServiceClear() : Resource(
      absl::StrCat(kEventServiceUri, "/Actions/EventService.",
                   EventService::kClearAction)) {}

 private:
  void Get(tensorflow::serving::net_http::ServerRequestInterface *req,
           const ParamsType &params) override {
    req->ReplyWithStatus(
        tensorflow::serving::net_http::HTTPStatusCode::METHOD_NA);
  }

  void Post(tensorflow::serving::net_http::ServerRequestInterface *req,
            const ParamsType &params) override {

    req->ReplyWithStatus(
        tensorflow::serving::net_http::HTTPStatusCode::IM_A_TEAPOT);
  }
};

}  // namespace ecclesia
#endif  // ECCLESIA_MAGENT_REDFISH_INDUS_EVENT_SERVICE_CLEAR_H_
