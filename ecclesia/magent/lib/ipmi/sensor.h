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

#ifndef ECCLESIA_MAGENT_LIB_IPMI_SENSOR_H_
#define ECCLESIA_MAGENT_LIB_IPMI_SENSOR_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "absl/time/time.h"

namespace ecclesia {

struct SensorNum {
  // ipmi logical unit unmber.
  uint8_t lun;
  uint8_t id;

  // Support for absl::Hash.
  template <typename H>
  friend H AbslHashValue(H h, SensorNum v) {
    return H::combine(std::move(h), v.lun, v.id);
  }

  explicit operator int() const { return (lun << 8 | id); }
};

inline bool operator==(const SensorNum &lhs, const SensorNum &rhs) {
  return lhs.lun == rhs.lun && lhs.id == rhs.id;
}

inline bool operator!=(const SensorNum &lhs, const SensorNum &rhs) {
  return !(lhs == rhs);
}

inline bool operator<(const SensorNum &lhs, const SensorNum &rhs) {
  return (lhs.lun < rhs.lun) || (lhs.lun == rhs.lun && lhs.id < rhs.id);
}
enum SensorType : int {
  SENSOR_TYPE_THERMAL = 1,
  SENSOR_TYPE_VOLTAGE = 2,
  SENSOR_TYPE_FANTACH = 3,
  SENSOR_TYPE_CURRENT = 4,
  SENSOR_TYPE_POWER = 5,
  SENSOR_TYPE_FREQUENCY = 6,
  SENSOR_TYPE_DUTYCYCLE = 7,
  SENSOR_TYPE_ENERGY = 8,
  SENSOR_TYPE_OEM_STATE = 9,
  SENSOR_TYPE_TIME = 11,
  SENSOR_TYPE_PRESENCE = 12
};

enum SensorUnit : int {
  SENSOR_UNIT_UNSPECIFIED = 0,
  SENSOR_UNIT_DEGREES = 1,
  SENSOR_UNIT_MARGIN = 2,
  SENSOR_UNIT_TCONTROL = 3,
  SENSOR_UNIT_VOLTS = 4,
  SENSOR_UNIT_RPM = 5,
  SENSOR_UNIT_AMPS = 6,
  SENSOR_UNIT_WATTS = 7,
  SENSOR_UNIT_HERTZ = 8,
  SENSOR_UNIT_PERCENT = 9,
  SENSOR_UNIT_JOULES = 10,
  SENSOR_UNIT_SECONDS = 11,
  SENSOR_UNIT_PRESENCE = 12
};

struct SensorKind {
  SensorType type;
  SensorUnit unit;

  static constexpr SensorKind thermal() {
    return {SENSOR_TYPE_THERMAL, SENSOR_UNIT_DEGREES};
  }
  static constexpr SensorKind thermal_margin() {
    return {SENSOR_TYPE_THERMAL, SENSOR_UNIT_MARGIN};
  }
  static constexpr SensorKind current() {
    return {SENSOR_TYPE_CURRENT, SENSOR_UNIT_AMPS};
  }
  static constexpr SensorKind voltage() {
    return {SENSOR_TYPE_VOLTAGE, SENSOR_UNIT_VOLTS};
  }
  static constexpr SensorKind power() {
    return {SENSOR_TYPE_POWER, SENSOR_UNIT_WATTS};
  }
  static constexpr SensorKind energy() {
    return {SENSOR_TYPE_ENERGY, SENSOR_UNIT_JOULES};
  }
  static constexpr SensorKind dutycycle() {
    return {SENSOR_TYPE_DUTYCYCLE, SENSOR_UNIT_PERCENT};
  }
  static constexpr SensorKind fantach() {
    return {SENSOR_TYPE_FANTACH, SENSOR_UNIT_RPM};
  }
  static constexpr SensorKind frequency() {
    return {SENSOR_TYPE_FREQUENCY, SENSOR_UNIT_HERTZ};
  }
  static constexpr SensorKind seconds() {
    return {SENSOR_TYPE_TIME, SENSOR_UNIT_SECONDS};
  }
  static constexpr SensorKind presence() {
    return {SENSOR_TYPE_PRESENCE, SENSOR_UNIT_PRESENCE};
  }
};

inline bool operator==(const SensorKind &lhs, const SensorKind &rhs) {
  return lhs.type == rhs.type && lhs.unit == rhs.unit;
}

inline bool operator!=(const SensorKind &lhs, const SensorKind &rhs) {
  return !(lhs == rhs);
}

// A single sensor reading.
struct SensorReading {
  // The value read from the sensor.  Interpretation of this depends on the
  // sensor type and units.
  double reading;
  // Time at which the sensor reading was obtained.
  absl::Time timestamp;
};

// This is an abstract base class, which can be subclassed by specific
// types of IpmiSensor device classes.
class IpmiSensor {
 public:
  // Import the types/units defined in the .proto.
  using Type = SensorType;
  using Unit = SensorUnit;

  IpmiSensor(const std::string &name, Type type, Unit units)
      : name_(name), type_(type), unit_(units) {}

  IpmiSensor(const std::string &name, const SensorKind &kind)
      : name_(name), type_(kind.type), unit_(kind.unit) {}

  virtual ~IpmiSensor() {}

  // Get the legacy (compat) name.
  virtual const std::string &GetName() const { return name_; }

  // Get the type.
  virtual Type GetType() const { return type_; }

  // Get the units.
  virtual Unit GetUnit() const { return unit_; }

  // Reports a possibly cached sensor reading. For sensors with cached readings,
  // this function always reads from a cache which is periodically updated
  // when the sensor is polled. For sensors with no cached readings, this
  // polls the sensor directly and returns the current time for both
  // timestamp and next_timestamp.
  // Returns: true on success.
  virtual bool Read(SensorReading *reading) const = 0;

 private:
  std::string name_;
  Type type_;
  Unit unit_;

  IpmiSensor(const IpmiSensor &) = delete;
  IpmiSensor &operator=(const IpmiSensor &) = delete;
};

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_LIB_IPMI_SENSOR_H_
