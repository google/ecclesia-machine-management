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

#include "ecclesia/magent/redfish/indus/sleipnir_sensor.h"

#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "ecclesia/magent/lib/ipmi/interface_options.h"
#include "ecclesia/magent/lib/ipmi/ipmitool.h"
#include "ecclesia/magent/lib/ipmi/sensor.h"
#include "ecclesia/magent/redfish/core/json_helper.h"
#include "ecclesia/magent/redfish/core/redfish_keywords.h"
#include "ecclesia/magent/redfish/core/resource.h"
#include "ecclesia/magent/sysmodel/x86/sleipnir_sensor.h"
#include "ecclesia/magent/sysmodel/x86/sysmodel.h"
#include "single_include/nlohmann/json.hpp"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"

namespace ecclesia {

namespace {
constexpr char kMagentConfigPath[] = "/etc/google/magent/config.pb";

template <typename Tval>
struct ValStr {
  const char *str;
  Tval val;
};

#define VALSTR(val, str) \
  { str, val }
#define VALSTR_END \
  { nullptr }

template <typename Tval, typename Tkey>
const char *ValToStr(const ValStr<Tval> *array, Tkey val,
                     const char *default_string) {
  for (int i = 0; array[i].str; ++i) {
    if (array[i].val == val) {
      return array[i].str;
    }
  }
  return default_string;
}

const ValStr<SensorType> kSensorTypes[] = {
    VALSTR(SENSOR_TYPE_THERMAL, "Temperature"),
    VALSTR(SENSOR_TYPE_VOLTAGE, "Power"),
    VALSTR(SENSOR_TYPE_FANTACH, "Rotational"),
    VALSTR(SENSOR_TYPE_CURRENT, "Current"),
    VALSTR(SENSOR_TYPE_POWER, "Power"),
    VALSTR(SENSOR_TYPE_DUTYCYCLE, "Rotational"),
    VALSTR(SENSOR_TYPE_ENERGY, "EnergyJoule"),
    VALSTR(SENSOR_TYPE_FREQUENCY, "Frequency"),
    VALSTR(SENSOR_TYPE_OEM_STATE, "state"),
    VALSTR(SENSOR_TYPE_TIME, "time"),
    VALSTR(SENSOR_TYPE_PRESENCE, "presence"),
    VALSTR_END,
};

const ValStr<SensorUnit> kSensorUnit[] = {
    VALSTR(SENSOR_UNIT_UNSPECIFIED, "unspecified"),
    VALSTR(SENSOR_UNIT_DEGREES, "Cel"),
    VALSTR(SENSOR_UNIT_MARGIN, "Cel"),
    VALSTR(SENSOR_UNIT_TCONTROL, "Tcontrol"),
    VALSTR(SENSOR_UNIT_VOLTS, "volts"),
    VALSTR(SENSOR_UNIT_RPM, "RPM"),
    VALSTR(SENSOR_UNIT_AMPS, "amps"),
    VALSTR(SENSOR_UNIT_WATTS, "watts"),
    VALSTR(SENSOR_UNIT_PERCENT, "percent"),
    VALSTR(SENSOR_UNIT_JOULES, "joules"),
    VALSTR(SENSOR_UNIT_HERTZ, "hertz"),
    VALSTR(SENSOR_UNIT_SECONDS, "seconds"),
    VALSTR(SENSOR_UNIT_PRESENCE, "presence"),
    VALSTR_END,
};

absl::flat_hash_map<std::string, std::string> related_item_map{
    {"FAN0",
     "/redfish/v1/Chassis/Sleipnir/Assembly#/Assemblies/9/Oem/Google/"
     "Components/0"},
    {"FAN1",
     "/redfish/v1/Chassis/Sleipnir/Assembly#/Assemblies/10/Oem/Google/"
     "Components/0"},
    {"FAN2",
     "/redfish/v1/Chassis/Sleipnir/Assembly#/Assemblies/11/Oem/Google/"
     "Components/0"},
};

}  // namespace

void SleipnirIpmiSensor::Get(
    tensorflow::serving::net_http::ServerRequestInterface *req,
    const ParamsType &params) {
  nlohmann::json json;
  std::string sensor_name = std::get<std::string>(params[0]);

  auto sleipnir_sensors = system_model_->GetAllIpmiSensors();
  for (const auto &sensor : sleipnir_sensors) {
    auto info = sensor.GetInfo();
    if (info.name == sensor_name) {
      ecclesia::Ipmitool ipmi(
          ecclesia::GetIpmiCredentialFromPb(kMagentConfigPath));
      json[kOdataId] = std::string(req->uri_path());
      json[kOdataType] = "#Sensor.v1_2_0.Sensor";
      json[kOdataContext] = "/redfish/v1/$metadata#Sensor.Sensor";
      json[kId] = sensor_name;
      json[kName] = sensor_name;
      json[kReadingType] = ValToStr(kSensorTypes, info.type, "unknown");
      json[kReadingUnits] = ValToStr(kSensorUnit, info.unit, "unknown");
      auto maybe_value = ipmi.ReadSensor(info.id);
      double value = 0;
      if (maybe_value.ok()) {
        value = maybe_value.value();
      }
      json[kReading] = value;

      if (absl::StartsWith(sensor_name, "sleipnir_Fan")) {
        int idx = sensor_name[12] - '0';
        nlohmann::json assoc;
        assoc[kOdataId] = absl::StrCat(
            "/redfish/v1/Chassis/Sleipnir/Assembly#/Assemblies/", 9 + idx);
        GetJsonArray(&json, kRelatedItem)->push_back(assoc);
      }
      break;
    }
    // auto *status = GetJsonObject(&json, kStatus);
    //(*status)[kState] = "Enabled";
    nlohmann::json status;
    status[kState] = kEnabled;
    status[kHealth] = "OK";
    json[kStatus] = status;
  }

  JSONResponseOK(json, req);
}

}  // namespace ecclesia
