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

#ifndef ECCLESIA_MAGENT_SYSMODEL_X86_SLEIPNIR_SENSOR_H_
#define ECCLESIA_MAGENT_SYSMODEL_X86_SLEIPNIR_SENSOR_H_

#include <string>
#include <vector>

#include "ecclesia/magent/lib/ipmi/ipmi.h"

namespace ecclesia {

struct SleipnirSensorInfo {
  SensorNum id;
  std::string name;
  IpmiSensor::Type type;
  IpmiSensor::Unit unit;
  bool settable;
};

class SleipnirSensor {
 public:
  explicit SleipnirSensor(const IpmiInterface::BmcSensorInterfaceInfo &sensor);

  // Allow the object to be copyable
  // Make sure that copy construction is relatively light weight.
  // In cases where it is not feasible to copy construct data members,it may
  // make sense to wrap the data member in a shared_ptr.
  SleipnirSensor(const SleipnirSensor &sensor) = default;
  SleipnirSensor &operator=(const SleipnirSensor &sensor) = default;

  const SleipnirSensorInfo &GetSleipnirSensorInfo() const {
    return sleipnir_sensor_info_;
  }

  const SleipnirSensorInfo &GetInfo() const { return sleipnir_sensor_info_; }

 private:
  SleipnirSensorInfo sleipnir_sensor_info_;
};

std::vector<SleipnirSensor> CreateSleipnirSensors(
    absl::Span<const IpmiInterface::BmcSensorInterfaceInfo> ipmi_sensors);

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_SYSMODEL_X86_SLEIPNIR_SENSOR_H_
