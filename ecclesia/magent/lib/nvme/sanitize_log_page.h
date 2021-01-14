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

#ifndef ECCLESIA_MAGENT_LIB_NVME_SANITIZE_LOG_PAGE_H_
#define ECCLESIA_MAGENT_LIB_NVME_SANITIZE_LOG_PAGE_H_

#include <cstdint>
#include <memory>
#include <string>

#include "absl/status/statusor.h"
#include "ecclesia/magent/lib/nvme/nvme_types.emb.h"

namespace ecclesia {

class SanitizeLogPageInterface {
 public:
  virtual ~SanitizeLogPageInterface() = default;
  // Sanitize Log accessors, as defined by the NVM-Express spec 1.3,
  // section 5.14.1.9.2 'Sanitize Status (Log Identifier 81h)'

  // Indicates the fraction complete of the sanitize operations.
  // The value is a numerator of the fraction complete that has 65536 as its
  // denominator.
  virtual uint16_t progress() const = 0;

  // The status of the most recent sanitize operations.
  // 000b - The NVMe subsystem has never been sanitized.
  // 001b - The most recent sanitize operations completed successfully.
  // 010b - A sanitize operations is currently in progress.
  // 011b - The most recent sanitize operation failed.
  virtual uint8_t recent_sanitize_status() const = 0;

  // The number of completed overwrite sanitize operations.
  virtual uint8_t completed_overwrite_passes() const = 0;

  // Whether the storage in the NVM subsystem has been written to since
  // the most recent successful sanitize operation or manufactured.
  virtual bool written_after_sanitize() const = 0;

  // The Sanitize command information retain in Dword10 (Section 5.24).
  // 001b - Exit Failure Mode
  // 010b - Block Erase
  // 011b - Overwrite Sanitize
  // 100b - Crypto Erase.
  virtual uint8_t sanitize_action() const = 0;

  // Whether sanitize operation is performed in unrestricted copmletion mode.
  virtual bool allow_unrestricted_sanitize_exit() const = 0;

  // The number of overwrite passes.
  virtual uint8_t overwrite_pass_count() const = 0;

  // Whether the Overwrite Pattern is inverted between passes.
  virtual bool overwrite_invert_pattern() const = 0;

  // Whether the controller deallocate any logical blocks as a result of
  // successfully completing the sanitize operation.
  virtual bool no_dealloc() const = 0;

  virtual uint32_t estimate_overwrite_time() const = 0;
  virtual uint32_t estimate_block_erase_time() const = 0;
  virtual uint32_t estimate_crypto_erase_time() const = 0;
};

// Class to decode NVM-Express Sanitize Status Log Page.
class SanitizeLogPage : public SanitizeLogPageInterface {
 public:
  // Factory function to parse raw data buffer into SanitizeLogPage.
  // Returns a new SanitizeLogPage object or nullptr in case of error.
  //
  // Arguments:
  //   buf: Data buffer returned from Sanitize Status Log Page read request
  static absl::StatusOr<std::unique_ptr<SanitizeLogPageInterface>> Parse(
      const std::string &buf);

  SanitizeLogPage(const SanitizeLogPage &) = delete;
  SanitizeLogPage &operator=(const SanitizeLogPage &) = delete;
  uint16_t progress() const override;

  uint8_t recent_sanitize_status() const override;
  uint8_t completed_overwrite_passes() const override;
  bool written_after_sanitize() const override;

  uint8_t sanitize_action() const override;
  bool allow_unrestricted_sanitize_exit() const override;
  uint8_t overwrite_pass_count() const override;
  bool overwrite_invert_pattern() const override;
  bool no_dealloc() const override;

  uint32_t estimate_overwrite_time() const override;
  uint32_t estimate_block_erase_time() const override;
  uint32_t estimate_crypto_erase_time() const override;

 protected:
  explicit SanitizeLogPage(const std::string &buf);

 private:
  std::string data_;
  SanitizeLogPageFormatView sanitize_log_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_LIB_NVME_SANITIZE_LOG_PAGE_H_
