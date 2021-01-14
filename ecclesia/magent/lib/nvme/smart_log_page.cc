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

#include "ecclesia/magent/lib/nvme/smart_log_page.h"

#include <cstdint>
#include <memory>
#include <string>

#include "absl/numeric/int128.h"
#include "ecclesia/lib/codec/endian.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/magent/lib/nvme/nvme_types.emb.h"
#include "runtime/cpp/emboss_cpp_util.h"
#include "runtime/cpp/emboss_prelude.h"

namespace ecclesia {

std::unique_ptr<SmartLogPageInterface> SmartLogPage::Parse(
    const std::string &buf) {
  if (buf.size() != SmartLogPageFormat::IntrinsicSizeInBytes()) return nullptr;
  return std::unique_ptr<SmartLogPage>(new SmartLogPage(buf));
}

SmartLogPage::SmartLogPage(const std::string &buf)
    : data_(buf), smart_log_(&data_) {}

uint8_t SmartLogPage::critical_warning() const {
  return smart_log_.critical_warning().Read();
}

uint16_t SmartLogPage::composite_temperature_kelvins() const {
  return smart_log_.temperature().Read();
}

uint8_t SmartLogPage::available_spare() const {
  return smart_log_.available_spare().Read();
}

uint8_t SmartLogPage::available_spare_threshold() const {
  return smart_log_.available_spare_threshold().Read();
}

uint8_t SmartLogPage::percent_used() const {
  return smart_log_.percent_used().Read();
}

uint8_t SmartLogPage::endurance_group_critical_warning() const {
  return smart_log_.endurance_cw().Read();
}

absl::uint128 SmartLogPage::data_units_read() const {
  return LittleEndian::Load128(
      smart_log_.data_units_read().BackingStorage().data());
}

absl::uint128 SmartLogPage::data_units_written() const {
  return LittleEndian::Load128(
      smart_log_.data_units_written().BackingStorage().data());
}

absl::uint128 SmartLogPage::host_reads() const {
  return LittleEndian::Load128(smart_log_.host_reads().BackingStorage().data());
}

absl::uint128 SmartLogPage::host_writes() const {
  return LittleEndian::Load128(
      smart_log_.host_writes().BackingStorage().data());
}

absl::uint128 SmartLogPage::controller_busy_time_minutes() const {
  return LittleEndian::Load128(
      smart_log_.ctrl_busy_time().BackingStorage().data());
}

absl::uint128 SmartLogPage::power_cycles() const {
  return LittleEndian::Load128(
      smart_log_.power_cycles().BackingStorage().data());
}

absl::uint128 SmartLogPage::power_on_hours() const {
  return LittleEndian::Load128(
      smart_log_.power_on_hours().BackingStorage().data());
}

absl::uint128 SmartLogPage::unsafe_shutdowns() const {
  return LittleEndian::Load128(
      smart_log_.unsafe_shutdowns().BackingStorage().data());
}

absl::uint128 SmartLogPage::media_errors() const {
  return LittleEndian::Load128(
      smart_log_.media_errors().BackingStorage().data());
}

absl::uint128 SmartLogPage::num_err_log_entries() const {
  return LittleEndian::Load128(
      smart_log_.num_err_log_entries().BackingStorage().data());
}

uint32_t SmartLogPage::warning_temp_time_minutes() const {
  return smart_log_.warning_temp_time().Read();
}

uint32_t SmartLogPage::critical_comp_time_minutes() const {
  return smart_log_.critical_comp_time().Read();
}

uint16_t SmartLogPage::thermal_sensor_kelvins(int sensor) const {
  CheckCondition(sensor < SmartLogPageFormat::num_temperature_sensors());
  return smart_log_.temp_sensor()[sensor].Read();
}

uint32_t SmartLogPage::thermal_transition_count(int limit) const {
  CheckCondition(limit < SmartLogPageFormat::num_thermal_management_entries());
  return smart_log_.thm_temp_trans_count()[limit].Read();
}

uint32_t SmartLogPage::thermal_transition_minutes(int limit) const {
  CheckCondition(limit < SmartLogPageFormat::num_thermal_management_entries());
  return smart_log_.thm_temp_total_time()[limit].Read();
}

}  // namespace ecclesia
