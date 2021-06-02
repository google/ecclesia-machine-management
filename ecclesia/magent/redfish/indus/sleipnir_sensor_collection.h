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

#ifndef ECCLESIA_MAGENT_REDFISH_COMMON_SENSOR_COLLECTION_H_
#define ECCLESIA_MAGENT_REDFISH_COMMON_SENSOR_COLLECTION_H_

#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "ecclesia/magent/redfish/core/json_helper.h"
#include "ecclesia/magent/redfish/core/redfish_keywords.h"
#include "ecclesia/magent/redfish/core/resource.h"
#include "ecclesia/magent/sysmodel/x86/sysmodel.h"
#include "json/value.h"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"
#include "re2/re2.h"

namespace ecclesia {

class SensorCollection : public Resource {
 public:
  explicit SensorCollection(SystemModel *system_model)
      : Resource(kSleipnirSensorCollectionUri), system_model_(system_model) {}

 private:
  void Get(tensorflow::serving::net_http::ServerRequestInterface *req,
           const ParamsType &params) override {
    Json::Value json;
    AddStaticFields(&json);
    auto sleipnir_sensors = system_model_->GetAllIpmiSensors();
    int num_sensors = sleipnir_sensors.size();
    json[kMembersCount] = num_sensors;
    auto *members = GetJsonArray(&json, kMembers);
    for (const auto &sensor : sleipnir_sensors) {
      auto info = sensor.GetInfo();
      AppendCollectionMember(members, absl::StrCat(Uri(), "/", info.name));
    }
    JSONResponseOK(json, req);
  }

  void AddStaticFields(Json::Value *json) {
    (*json)[kOdataId] = std::string(Uri());
    (*json)[kOdataType] = "#SensorCollection.SensorCollection";
    (*json)[kOdataContext] =
        "/redfish/v1/$metadata#SensorCollection.SensorCollection";
    (*json)[kName] = "Sensor Collection";
  }
  SystemModel *const system_model_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_REDFISH_COMMON_SENSOR_COLLECTION_H_
