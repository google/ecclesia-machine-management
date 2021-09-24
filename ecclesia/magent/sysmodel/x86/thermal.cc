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

#include "ecclesia/magent/sysmodel/x86/thermal.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "ecclesia/lib/io/pci/config.h"
#include "ecclesia/lib/io/pci/pci.h"
#include "ecclesia/lib/io/pci/region.h"
#include "ecclesia/lib/io/pci/sysfs.h"
#include "ecclesia/magent/sysmodel/thermal.h"

namespace ecclesia {

PciThermalSensor::PciThermalSensor(const PciSensorParams &params)
    : PciThermalSensor(params, SysfsPciDevice::TryCreateDevice(params.loc)) {}

PciThermalSensor::PciThermalSensor(const PciSensorParams &params,
                                   std::unique_ptr<PciDevice> device)
    : ThermalSensor(params.name, params.upper_threshold_critical),
      offset_(params.offset),
      device_(std::move(device)) {}

std::optional<int> PciThermalSensor::Read() {
  if (device_) {
    auto maybe_uint16 = device_->ConfigSpace()->Region()->Read16(offset_);
    if (maybe_uint16.ok()) {
      return maybe_uint16.value();
    }
  }
  return std::nullopt;
}

std::vector<PciThermalSensor> CreatePciThermalSensors(
    const absl::Span<const PciSensorParams> param_set) {
  std::vector<PciThermalSensor> sensors;
  for (const auto &param : param_set) {
    sensors.emplace_back(param);
  }
  return sensors;
}

}  // namespace ecclesia
