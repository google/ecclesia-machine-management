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

#include "ecclesia/magent/redfish/indus/chassis.h"

#include <string>
#include <utility>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "ecclesia/magent/redfish/core/json_helper.h"
#include "ecclesia/magent/redfish/core/redfish_keywords.h"
#include "ecclesia/magent/redfish/core/resource.h"
#include "ecclesia/magent/sysmodel/x86/chassis.h"
#include "ecclesia/magent/sysmodel/x86/fru.h"
#include "ecclesia/magent/sysmodel/x86/sysmodel.h"
#include "single_include/nlohmann/json.hpp"
#include "tensorflow_serving/util/net_http/server/public/response_code_enum.h"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"

namespace ecclesia {
namespace {

absl::optional<SysmodelFru> GetFruInfo(SystemModel *system_model,
                                       absl::string_view fru_name) {
  SysmodelFruReaderIntf *fru_reader = system_model->GetFruReader(fru_name);
  if (!fru_reader) return absl::nullopt;
  absl::optional<SysmodelFru> fru = fru_reader->Read();
  if (!fru.has_value()) return absl::nullopt;
  return std::move(*fru);
}

}  // namespace

void Chassis::Get(tensorflow::serving::net_http::ServerRequestInterface *req,
                  const ParamsType &params) {
  if (params.size() != 1 || !absl::holds_alternative<std::string>(params[0])) {
    return;
  }
  auto chassis_req_name = std::get<std::string>(params[0]);
  std::string chassis_name = chassis_req_name;
  // The conversion below is to seamlessly support the existing name "chassis".
  // This should be cleaned up.
  if (chassis_req_name == "chassis") {
    chassis_name = "Indus";
  }
  absl::optional<ChassisId> found_chassis =
      system_model_->GetChassisByName(chassis_name);
  if (!found_chassis.has_value()) {
    req->ReplyWithStatus(
        tensorflow::serving::net_http::HTTPStatusCode::NOT_FOUND);
    return;
  }
  const ChassisId &chassis_id = found_chassis.value();
  nlohmann::json json;
  json[kOdataType] = "#Chassis.v1_6_0.Chassis";
  json[kOdataId] = req->uri_path();
  json[kOdataContext] = "/redfish/v1/$metadata#Chassis.Chassis";
  json[kChassisType] = GetChassisTypeAsString(chassis_id);

  json[kName] = absl::StrFormat("%s Chassis", ChassisIdToString(chassis_id));
  json[kId] = chassis_req_name;

  std::string fru_name;
  if (chassis_id == ChassisId::kIndus) {
    fru_name = "motherboard";
  } else {
    fru_name = ChassisIdToString(chassis_id);
  }

  auto found_fru = GetFruInfo(system_model_, fru_name);
  if (found_fru.has_value()) {
    const SysmodelFru &fru = found_fru.value();
    json[kPartNumber] = std::string(fru.GetPartNumber());
    json[kSerialNumber] = std::string(fru.GetSerialNumber());
    json[kManufacturer] = std::string(fru.GetManufacturer());
  }

  auto *assembly = GetJsonObject(&json, "Assembly");
  (*assembly)[kOdataId] = absl::StrCat(req->uri_path(), "/Assembly");

  auto *links = GetJsonObject(&json, kLinks);
  auto *computer_systems = GetJsonArray(links, kComputerSystems);
  nlohmann::json linked_computer_systems;
  linked_computer_systems[kOdataId] = kComputerSystemUri;
  computer_systems->push_back(linked_computer_systems);

  if (chassis_id == ChassisId::kSleipnir) {
    auto *sensors = GetJsonObject(&json, kSensor);
    (*sensors)[kOdataId] = absl::StrCat(req->uri_path(), "/Sensors");
  }

  if (chassis_id == ChassisId::kIndus) {
    auto *thermal = GetJsonObject(&json, kThermal);
    (*thermal)[kOdataId] = kThermalUri;
  }

  JSONResponseOK(json, req);
}

}  // namespace ecclesia
