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

#include "ecclesia/magent/sysmodel/x86/sleipnir_sensor.h"

#include <optional>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "ecclesia/magent/lib/ipmi/ipmi.h"
#include "ecclesia/magent/lib/ipmi/sensor.h"
#include "re2/re2.h"

namespace ecclesia {
namespace {

static LazyRE2 kFanRegex = {"^Fan\\d+_\\d+_RPM"};
static LazyRE2 kKLRegex = {"^KL\\d+"};
static LazyRE2 knvmeRegex = {"^nvme\\d+"};

}  // namespace

SleipnirSensor::SleipnirSensor(
    const IpmiInterface::BmcSensorInterfaceInfo &sensor) {
  sleipnir_sensor_info_.id = sensor.id;
  sleipnir_sensor_info_.name = absl::StrCat("sleipnir_", sensor.name);
  sleipnir_sensor_info_.type = sensor.type;
  sleipnir_sensor_info_.unit = sensor.unit;
}

std::vector<SleipnirSensor> CreateSleipnirSensors(
    absl::Span<const IpmiInterface::BmcSensorInterfaceInfo> ipmi_sensors) {
  std::vector<SleipnirSensor> sensors;
  for (const auto &sensor : ipmi_sensors) {
    if ((RE2::FullMatch(sensor.name, *kFanRegex) && !sensor.settable) ||
        RE2::FullMatch(sensor.name, *kKLRegex) ||
        RE2::FullMatch(sensor.name, *knvmeRegex)) {
      sensors.emplace_back(sensor);
    }
  }
  return sensors;
}

}  // namespace ecclesia
