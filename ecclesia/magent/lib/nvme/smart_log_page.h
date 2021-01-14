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

#ifndef ECCLESIA_MAGENT_LIB_NVME_SMART_LOG_PAGE_H_
#define ECCLESIA_MAGENT_LIB_NVME_SMART_LOG_PAGE_H_

#include <cstdint>
#include <memory>
#include <string>

#include "absl/numeric/int128.h"
#include "ecclesia/magent/lib/nvme/nvme_types.emb.h"

namespace ecclesia {

// Interface to decode NVM-Express SMART Log Page.
class SmartLogPageInterface {
 public:
  virtual ~SmartLogPageInterface() = default;

  // SMART data accessors, as defined by the NVM-Express spec 1.3a,
  // section 5.14.1.2 'SMART / Health Information (Log Identifier 02h)'
  virtual uint8_t critical_warning() const = 0;
  virtual uint16_t composite_temperature_kelvins() const = 0;
  virtual uint8_t available_spare() const = 0;
  virtual uint8_t available_spare_threshold() const = 0;
  virtual uint8_t percent_used() const = 0;
  virtual uint8_t endurance_group_critical_warning() const = 0;

  // This value is reported in thousands 512 byte data units (i.e., a value of
  // 1 corresponds to 1000 units of 512 bytes read/written); this value does
  // not include metadata and is rounded up.
  virtual absl::uint128 data_units_read() const = 0;
  virtual absl::uint128 data_units_written() const = 0;

  // Contains the number of read/write commands completed by the controller.
  virtual absl::uint128 host_reads() const = 0;
  virtual absl::uint128 host_writes() const = 0;

  virtual absl::uint128 controller_busy_time_minutes() const = 0;
  virtual absl::uint128 power_cycles() const = 0;
  virtual absl::uint128 power_on_hours() const = 0;
  virtual absl::uint128 unsafe_shutdowns() const = 0;
  virtual absl::uint128 media_errors() const = 0;
  virtual absl::uint128 num_err_log_entries() const = 0;
  virtual uint32_t warning_temp_time_minutes() const = 0;
  virtual uint32_t critical_comp_time_minutes() const = 0;
  virtual uint16_t thermal_sensor_kelvins(int sensor) const = 0;
  virtual uint32_t thermal_transition_count(int limit) const = 0;
  virtual uint32_t thermal_transition_minutes(int limit) const = 0;
};

// Class to decode NVM-Express SMART Log Page.
class SmartLogPage : public SmartLogPageInterface {
 public:
  // Factory function to parse raw data buffer into SmartLogPage.
  // Returns a new SmartLogPage object or nullptr in case of error.
  //
  // Arguments:
  //   buf: Data buffer returned from SMART Log Page read request
  static std::unique_ptr<SmartLogPageInterface> Parse(const std::string &buf);

  SmartLogPage(const SmartLogPage &) = delete;
  SmartLogPage &operator=(const SmartLogPage &) = delete;
  uint8_t critical_warning() const override;
  uint16_t composite_temperature_kelvins() const override;
  uint8_t available_spare() const override;
  uint8_t available_spare_threshold() const override;
  uint8_t percent_used() const override;
  uint8_t endurance_group_critical_warning() const override;
  absl::uint128 data_units_read() const override;
  absl::uint128 data_units_written() const override;
  absl::uint128 host_reads() const override;
  absl::uint128 host_writes() const override;
  absl::uint128 controller_busy_time_minutes() const override;
  absl::uint128 power_cycles() const override;
  absl::uint128 power_on_hours() const override;
  absl::uint128 unsafe_shutdowns() const override;
  absl::uint128 media_errors() const override;
  absl::uint128 num_err_log_entries() const override;
  uint32_t warning_temp_time_minutes() const override;
  uint32_t critical_comp_time_minutes() const override;
  uint16_t thermal_sensor_kelvins(int sensor) const override;
  uint32_t thermal_transition_count(int limit) const override;
  uint32_t thermal_transition_minutes(int limit) const override;

 protected:
  explicit SmartLogPage(const std::string &buf);

 private:
  std::string data_;
  SmartLogPageFormatView smart_log_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_LIB_NVME_SMART_LOG_PAGE_H_
