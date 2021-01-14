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

#ifndef ECCLESIA_MAGENT_THERMAL_H_
#define ECCLESIA_MAGENT_THERMAL_H_

#include <string>

#include "absl/types/optional.h"
#include "absl/strings/string_view.h"

namespace ecclesia {

class ThermalSensor {
 public:
  ThermalSensor(const absl::string_view name, int upper_threshold_critical)
      : name_(name), upper_threshold_critical_(upper_threshold_critical) {}
  virtual ~ThermalSensor() = default;

  virtual absl::optional<int> Read() = 0;
  const std::string &Name() const { return name_; }
  const int UpperThresholdCritical() const { return upper_threshold_critical_; }

 protected:
  const std::string name_;
  const int upper_threshold_critical_;
};

}  // namespace ecclesia
#endif  // ECCLESIA_MAGENT_THERMAL_H_
