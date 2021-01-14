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

#ifndef ECCLESIA_MAGENT_LIB_NVME_IDENTIFY_CONTROLLER_H_
#define ECCLESIA_MAGENT_LIB_NVME_IDENTIFY_CONTROLLER_H_

#include <cstdint>
#include <memory>
#include <string>

#include "absl/numeric/int128.h"
#include "ecclesia/magent/lib/nvme/nvme_types.emb.h"

namespace ecclesia {

// Class to decode NVM-Express IdentifyContoller object.
class IdentifyController {
 public:
  // Factory function to parse raw data buffer into IdentifyController.
  // Returns a new IdentifyController object or nullptr in case of error.
  //
  // Arguments:
  //   buf: Data buffer returned from IdentifyController request.
  static std::unique_ptr<IdentifyController> Parse(const std::string &buf);

  IdentifyController(const IdentifyController &) = delete;
  IdentifyController &operator=(const IdentifyController &) = delete;

  virtual ~IdentifyController() = default;

  // IdentifyController data accessors, as defined by NVM-Express spec 1.3a,
  // section 5.15 'Identify command'.
  uint16_t controller_id() const;
  uint16_t vendor_id() const;
  uint16_t subsystem_vendor_id() const;
  std::string serial_number() const;
  std::string model_number() const;
  std::string firmware_revision() const;
  uint16_t critical_temperature_threshold() const;
  uint16_t warning_temperature_threshold() const;
  // Total usable capacity, in bytes.
  absl::uint128 total_capacity() const;
  // MDTS: Note that this value is in units of the minimum memory
  // page size (CAP.MPSMIN) and is reported as a power of two (2^n).
  uint8_t max_data_transfer_size() const;
  // Maximum value of valid NSID for the NVM subsystem
  uint32_t number_of_namespaces() const;
  // ELPE + 1:  Maximum number of Error Information log entries that are stored
  // by the controller.
  uint16_t max_error_log_page_entries() const;
  // Sanitize capabilities: Bit set to '1' means controller support erase
  // operation. Bit 0: Crypto Erase. Bit 1: Block Erase. Bit 2: Overwrite
  // Sanitize.
  uint32_t sanitize_capabilities() const;

 protected:
  explicit IdentifyController(const std::string &buf);

 private:
  std::string data_;
  IdentifyControllerFormatView identify_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_LIB_NVME_IDENTIFY_CONTROLLER_H_
