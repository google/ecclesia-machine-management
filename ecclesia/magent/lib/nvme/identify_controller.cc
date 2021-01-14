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

#include "ecclesia/magent/lib/nvme/identify_controller.h"

#include <cstdint>
#include <memory>
#include <string>

#include "absl/numeric/int128.h"
#include "absl/strings/ascii.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/codec/endian.h"
#include "ecclesia/magent/lib/nvme/nvme_types.emb.h"
#include "runtime/cpp/emboss_cpp_util.h"
#include "runtime/cpp/emboss_prelude.h"

namespace ecclesia {

std::unique_ptr<IdentifyController> IdentifyController::Parse(
    const std::string &buf) {
  if (buf.size() != IdentifyControllerFormat::IntrinsicSizeInBytes())
    return nullptr;
  return std::unique_ptr<IdentifyController>(new IdentifyController(buf));
}

IdentifyController::IdentifyController(const std::string &buf)
    : data_(buf), identify_(&data_) {}

uint16_t IdentifyController::controller_id() const {
  return identify_.cntlid().Read();
}

uint16_t IdentifyController::vendor_id() const {
  return identify_.vendor_id().Read();
}

uint16_t IdentifyController::subsystem_vendor_id() const {
  return identify_.subsystem_vendor_id().Read();
}

std::string IdentifyController::serial_number() const {
  auto serial = identify_.serial_number().ToString<absl::string_view>();
  return std::string(absl::StripAsciiWhitespace(serial));
}

std::string IdentifyController::model_number() const {
  auto model = identify_.model_number().ToString<absl::string_view>();
  return std::string(absl::StripAsciiWhitespace(model));
}

std::string IdentifyController::firmware_revision() const {
  auto version = identify_.firmware_revision().ToString<absl::string_view>();
  return std::string(absl::StripAsciiWhitespace(version));
}

uint16_t IdentifyController::critical_temperature_threshold() const {
  return identify_.cctemp().Read();
}

uint16_t IdentifyController::warning_temperature_threshold() const {
  return identify_.wctemp().Read();
}

absl::uint128 IdentifyController::total_capacity() const {
  return LittleEndian::Load128(identify_.tnvmcap().BackingStorage().data());
}

uint8_t IdentifyController::max_data_transfer_size() const {
  return identify_.mdts().Read();
}

uint32_t IdentifyController::number_of_namespaces() const {
  return identify_.nn().Read();
}

uint16_t IdentifyController::max_error_log_page_entries() const {
  return static_cast<uint16_t>(identify_.elpe().Read()) + 1;
}

uint32_t IdentifyController::sanitize_capabilities() const {
  return identify_.sanicap().Read();
}

}  // namespace ecclesia
