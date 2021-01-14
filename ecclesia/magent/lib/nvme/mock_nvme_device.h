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

#ifndef ECCLESIA_MAGENT_LIB_NVME_MOCK_NVME_DEVICE_H_
#define ECCLESIA_MAGENT_LIB_NVME_MOCK_NVME_DEVICE_H_

#include <cstdint>
#include <memory>
#include <set>
#include <vector>

#include "gmock/gmock.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ecclesia/magent/lib/nvme/controller_registers.h"
#include "ecclesia/magent/lib/nvme/device_self_test_log.h"
#include "ecclesia/magent/lib/nvme/firmware_slot_info.h"
#include "ecclesia/magent/lib/nvme/identify_controller.h"
#include "ecclesia/magent/lib/nvme/identify_namespace.h"
#include "ecclesia/magent/lib/nvme/nvme_device.h"
#include "ecclesia/magent/lib/nvme/nvme_types.emb.h"
#include "ecclesia/magent/lib/nvme/sanitize_log_page.h"
#include "ecclesia/magent/lib/nvme/smart_log_page.h"

namespace ecclesia {

class MockNvmeDevice : public NvmeDeviceInterface {
 public:
  MOCK_METHOD(absl::StatusOr<std::unique_ptr<IdentifyController>>, Identify, (),
              (const, override));
  MOCK_METHOD(absl::StatusOr<std::set<uint32_t>>, EnumerateNamespacesAfter,
              (uint32_t starting_namespace_id), (override));
  MOCK_METHOD(absl::StatusOr<IdentifyNamespace>, GetNamespaceInfo,
              (uint32_t namespace_id), (override));
  MOCK_METHOD(absl::StatusOr<std::vector<LBAFormatView>>,
              GetSupportedLbaFormats, (), (override));
  MOCK_METHOD(absl::StatusOr<std::unique_ptr<SmartLogPageInterface>>, SmartLog,
              (), (const, override));
  MOCK_METHOD(absl::StatusOr<std::unique_ptr<SanitizeLogPageInterface>>,
              SanitizeStatusLog, (), (const, override));
  MOCK_METHOD(absl::StatusOr<std::unique_ptr<FirmwareSlotInfo>>,
              FirmwareSlotInformation, (), (const, override));
  MOCK_METHOD(absl::StatusOr<std::unique_ptr<DeviceSelfTestLog>>,
              GetDeviceSelfTestLog, (), (const, override));
  MOCK_METHOD(absl::Status, AttachNamespace,
              (uint32_t namespace_id, uint16_t controller_id), (override));
  MOCK_METHOD(absl::Status, DetachNamespace,
              (uint32_t namespace_id, uint16_t controller_id), (override));
  MOCK_METHOD(absl::Status, DestroyNamespace, (uint32_t namespace_id),
              (override));
  MOCK_METHOD(absl::StatusOr<uint32_t>, CreateNamespace,
              (uint64_t capacity, uint8_t lba_format_ix), (override));
  MOCK_METHOD(absl::Status, Format, (uint8_t lba_format_ix), (override));
  // MOCK_METHOD(absl::Status, RescanNamespaces, (), (override));
  MOCK_METHOD(absl::Status, ResetSubsystem, (), (override));
  MOCK_METHOD(absl::Status, ResetController, (), (override));
  MOCK_METHOD(absl::Status, SanitizeCryptoErase, (), (override));
  MOCK_METHOD(absl::Status, SanitizeBlockErase, (), (override));
  MOCK_METHOD(absl::Status, SanitizeOverwrite, (uint32_t overwrite_pattern),
              (override));
  MOCK_METHOD(absl::Status, SanitizeExitFailureMode, (), (override));
  MOCK_METHOD(absl::StatusOr<ControllerRegisters>, GetControllerRegisters, (),
              (const, override));
};

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_LIB_NVME_MOCK_NVME_DEVICE_H_
