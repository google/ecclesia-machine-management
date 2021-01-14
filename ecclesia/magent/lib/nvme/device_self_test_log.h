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

#ifndef ECCLESIA_MAGENT_LIB_NVME_DEVICE_SELF_TEST_LOG_H_
#define ECCLESIA_MAGENT_LIB_NVME_DEVICE_SELF_TEST_LOG_H_

#include <assert.h>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "ecclesia/magent/lib/nvme/nvme_types.emb.h"
#include "runtime/cpp/emboss_cpp_util.h"
#include "runtime/cpp/emboss_prelude.h"

namespace ecclesia {

// This class represents one result for a completed/aborted device self-test.
class DeviceSelfTestResult {
 public:
  // Factory function to parse raw data buffer into DeviceSelfTestResult
  static absl::StatusOr<DeviceSelfTestResult> Parse(absl::string_view buf) {
    if (buf.size() != DeviceSelfTestResultFormat::IntrinsicSizeInBytes()) {
      return absl::InternalError("Unexpected error.");
    }
    return DeviceSelfTestResult(buf);
  }

  DeviceSelfTestResult(const DeviceSelfTestResult &) = delete;
  DeviceSelfTestResult &operator=(const DeviceSelfTestResult &) = delete;

  DeviceSelfTestResult(DeviceSelfTestResult &&) = default;
  DeviceSelfTestResult &operator=(DeviceSelfTestResult &&) = default;

  // Indicates Self-test code value that was specified in the device self-test
  // command that started the device self-test operation.
  enum SelfTestCode : uint8_t {
    kShortTest = 0x1,
    kExtendedTest = 0x2,
    kVendorSpecific = 0xE,
  };

  // Result of the device self-test operation
  enum Result : uint8_t {
    kSuccess = 0x0,
    kAbortedBySelfTest = 0x1,
    kAbortedByControllerReset = 0x2,
    kAbortedByNamespaceRemoval = 0x3,
    kAbortedByFormatNvm = 0x4,
    kFatalError = 0x5,
    kFailedWithUnknownSegment = 0x6,
    kFailedWithKnownSegment = 0x7,
    kAbortedUnknownReason = 0x8,
    kAbortedBySanitize = 0x9,
    kEntryUnused = 0xF,
  };

  // Accessors

  SelfTestCode DeviceSelfTestCode() const {
    return static_cast<SelfTestCode>(self_test_result_.status().Read() >> 4);
  }

  Result SelfTestResult() const {
    return static_cast<Result>(self_test_result_.status().Read() & 0xF);
  }

  // Failing segment number if known.
  absl::optional<uint8_t> FailedSegmentNumber() const {
    if (SelfTestResult() != kFailedWithKnownSegment) return absl::nullopt;
    return self_test_result_.segment_number().Read();
  }

  uint64_t PowerOnHours() const {
    return self_test_result_.power_on_hours().Read();
  }

  // Namespace the Failing LBA occurred on if known.
  absl::optional<uint32_t> FailingNamespace() const {
    if (!self_test_result_.nsid_valid().Read()) return absl::nullopt;
    return self_test_result_.nsid().Read();
  }

  // One of the failing LBAs if known.
  absl::optional<uint64_t> FailingLBA() const {
    if (!self_test_result_.flba_valid().Read()) return absl::nullopt;
    return self_test_result_.flba().Read();
  }

  // Status code type if known.
  absl::optional<uint8_t> StatusCodeType() const {
    if (!self_test_result_.sct_valid().Read()) return absl::nullopt;
    return self_test_result_.status_code_type().Read();
  }

  // Status code if known.
  absl::optional<uint8_t> StatusCode() const {
    if (!self_test_result_.sc_valid().Read()) return absl::nullopt;
    return self_test_result_.status_code().Read();
  }

  uint16_t VendorSpecificField() const {
    return self_test_result_.vendor_specific().Read();
  }

  static std::string ToString(Result result) {
    switch (result) {
      case kSuccess:
        return "SUCCESS";
      case kAbortedBySelfTest:
        return "ABORTED_BY_SELF_TEST";
      case kAbortedByControllerReset:
        return "ABORTED_BY_CONTROLLER_RESET";
      case kAbortedByNamespaceRemoval:
        return "ABORTED_BY_NAMESPACE_REMOVAL";
      case kAbortedByFormatNvm:
        return "ABORTED_BY_FORMAT_NVM";
      case kFatalError:
        return "FATAL_ERROR";
      case kFailedWithUnknownSegment:
        return "FAILED_WITH_UNKNOWN_SEGMENT";
      case kFailedWithKnownSegment:
        return "FAILED_WITH_KNOWN_SEGMENT";
      case kAbortedUnknownReason:
        return "ABORTED_UNKNOWN_REASON";
      case kAbortedBySanitize:
        return "ABORTED_BY_SANITIZE";
      case kEntryUnused:
        return "ENTRY_UNUSED";
    }
    return "RESERVED";
  }

  static std::string ToString(SelfTestCode code) {
    switch (code) {
      case kShortTest:
        return "SHORT_TEST";
      case kExtendedTest:
        return "EXTENDED_TEST";
      case kVendorSpecific:
        return "VENDOR_SPECIFIC";
    }
    return "RESERVED";
  }

 protected:
  explicit DeviceSelfTestResult(absl::string_view buf)
      : data_(buf), self_test_result_(&data_) {}

 private:
  std::string data_;
  DeviceSelfTestResultFormatView self_test_result_;
};

// This class represents a device self-test log. One log can have upto 20
// self-test results.
class DeviceSelfTestLog {
 public:
  static absl::StatusOr<std::unique_ptr<DeviceSelfTestLog>> Parse(
      absl::string_view buf) {
    if (buf.size() != DeviceSelfTestLogFormat::IntrinsicSizeInBytes()) {
      return absl::InternalError("Unexpected error.");
    }
    return absl::WrapUnique(new DeviceSelfTestLog(buf));
  }

  enum CurrentSelfTestStatusResult : uint8_t {
    kNoTestInProgress = 0x0,
    kShortTestInProgress = 0x1,
    kExtendedTestInProgress = 0x2,
    kVendorSpecific = 0xE
  };

  CurrentSelfTestStatusResult CurrentSelfTestStatus() const {
    return static_cast<enum CurrentSelfTestStatusResult>(
        log_.current_self_test_operation().Read() & 0xF);
  }

  uint8_t CurrentSelfTestCompletionPercentage() const {
    return log_.current_self_test_completion().Read();
  }

  std::vector<DeviceSelfTestResult> CompletedSelfTests() const {
    std::vector<DeviceSelfTestResult> results;
    for (const auto &result : log_.results()) {
      absl::StatusOr<DeviceSelfTestResult> test_result =
          DeviceSelfTestResult::Parse(
              result.BackingStorage().ToString<absl::string_view>());
      // The only way this can fail is if we pass the wrong size, and we
      // hard-coded the correct size, so this should be safe.
      assert(test_result.status() == absl::OkStatus());
      // If an entry is unused, stop parsing the rest of the results since they
      // will also be unused.
      if (test_result->SelfTestResult() != DeviceSelfTestResult::kEntryUnused) {
        results.push_back(std::move(*test_result));
      } else {
        break;
      }
    }
    return results;
  }

  static std::string ToString(enum CurrentSelfTestStatusResult status) {
    switch (status) {
      case kNoTestInProgress:
        return "NO_TEST_IN_PROGRESS";
      case kShortTestInProgress:
        return "SHORT_TEST_IN_PROGRESS";
      case kExtendedTestInProgress:
        return "EXTENDED_TEST_IN_PROGRESS";
      case kVendorSpecific:
        return "VENDOR_SPECIFIC";
    }
    return "RESERVED";
  }

 protected:
  explicit DeviceSelfTestLog(absl::string_view buf)
      : data_(buf), log_(&data_) {}

 private:
  const std::string data_;
  DeviceSelfTestLogFormatView log_;
};
}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_LIB_NVME_DEVICE_SELF_TEST_LOG_H_
