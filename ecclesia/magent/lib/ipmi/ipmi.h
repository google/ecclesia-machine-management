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

// Library for retrieving data through ipmi

#ifndef ECCLESIA_MAGENT_LIB_IPMI_IPMI_H_
#define ECCLESIA_MAGENT_LIB_IPMI_IPMI_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "ecclesia/magent/lib/ipmi/sensor.h"

namespace ecclesia {

class IpmiInterface {
 public:
  struct EntityIdentifier {
    uint8_t entity_id;
    uint8_t entity_instance;

    bool operator==(const EntityIdentifier &other) const {
      return std::tie(entity_id, entity_instance) ==
             std::tie(other.entity_id, other.entity_instance);
    }
    bool operator!=(const EntityIdentifier &other) const {
      return !(*this == other);
    }
    template <typename H>
    friend H AbslHashValue(H h, const EntityIdentifier &e) {
      return H::combine(std::move(h), e.entity_id, e.entity_instance);
    }
  };

  // Data struct containing information uniquely identifying a fru exported by
  // BMC
  struct BmcFruInterfaceInfo {
    uint16_t record_id;
    EntityIdentifier entity;
    std::string name;
    // This fru_id can be used as identification in ReadFru method.
    uint16_t fru_id;
  };

  struct BmcSensorInterfaceInfo {
    SensorNum id;
    std::string name;
    IpmiSensor::Type type;
    IpmiSensor::Unit unit;
    bool settable;
    EntityIdentifier entity_id;
    std::optional<double> min_threshold;
    std::optional<double> max_threshold;
    friend bool operator==(const BmcSensorInterfaceInfo &o1,
                           const BmcSensorInterfaceInfo &o2) {
      return o1.id == o2.id && o1.name == o2.name && o1.type == o2.type &&
             o1.unit == o2.unit && o1.settable == o2.settable &&
             o1.min_threshold == o2.min_threshold &&
             o1.max_threshold == o2.max_threshold;
    }
    friend bool operator!=(const BmcSensorInterfaceInfo &o1,
                           const BmcSensorInterfaceInfo &o2) {
      return !(o1 == o2);
    }
  };

  IpmiInterface() {}
  virtual ~IpmiInterface() {}

  virtual std::vector<BmcFruInterfaceInfo> GetAllFrus() = 0;

  virtual std::vector<BmcSensorInterfaceInfo> GetAllSensors() = 0;

  virtual absl::StatusOr<BmcSensorInterfaceInfo> GetSensor(
      SensorNum sensor_num) = 0;

  virtual absl::StatusOr<double> ReadSensor(SensorNum sensor_num) = 0;

  // Reads FRU raw data
  // We will read bytes starting from offset from fru identified by fru_id
  // The number of bytes read is equal to the size of data.
  virtual absl::Status ReadFru(uint16_t fru_id, size_t offset,
                               absl::Span<unsigned char> data) = 0;

  // Gets the FRU size
  virtual absl::Status GetFruSize(uint16_t fru_id, uint16_t *size) = 0;
};

struct IpmiCommandIdentifier {
  uint8_t netfn;
  uint8_t lun;
  uint8_t command;
  bool operator==(const IpmiCommandIdentifier &other) const {
    return std::tie(netfn, lun, command) ==
           std::tie(other.netfn, other.lun, other.command);
  }
  bool operator!=(const IpmiCommandIdentifier &other) const {
    return !(*this == other);
  }
  template <typename H>
  friend H AbslHashValue(H h, const IpmiCommandIdentifier &id) {
    return H::combine(std::move(h), id.netfn, id.lun, id.command);
  }
};

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_LIB_IPMI_IPMI_H_
