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

#ifndef ECCLESIA_MAGENT_LIB_IPMI_IPMI_MOCK_H_
#define ECCLESIA_MAGENT_LIB_IPMI_IPMI_MOCK_H_

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "gmock/gmock.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "ecclesia/magent/lib/ipmi/ipmi.h"
#include "ecclesia/magent/lib/ipmi/sensor.h"

namespace ecclesia {

class MockIpmiInterface : public IpmiInterface {
 public:
  MOCK_METHOD(std::vector<BmcFruInterfaceInfo>, GetAllFrus, (), (override));
  MOCK_METHOD(std::vector<BmcSensorInterfaceInfo>, GetAllSensors, (),
              (override));
  MOCK_METHOD(absl::StatusOr<BmcSensorInterfaceInfo>, GetSensor,
              (SensorNum sensor_num), (override));
  MOCK_METHOD(absl::StatusOr<double>, ReadSensor, (SensorNum sensor_num),
              (override));
  MOCK_METHOD(absl::Status, ReadFru,
              (uint16_t, size_t, absl::Span<unsigned char>), (override));
  MOCK_METHOD(absl::Status, GetFruSize, (uint16_t, uint16_t *), (override));
};

// This is to faciliate mocking up ReadFru method.
ACTION_P(IpmiReadFru, data) {
  unsigned char *output_data = arg2.data();
  const size_t len = arg2.size();
  memcpy(output_data, data, len);
  return absl::OkStatus();
}
}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_LIB_IPMI_IPMI_MOCK_H_
