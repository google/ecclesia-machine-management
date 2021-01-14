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

#ifndef ECCLESIA_MAGENT_LIB_NVME_NVME_DEVICE_H_
#define ECCLESIA_MAGENT_LIB_NVME_NVME_DEVICE_H_

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ecclesia/magent/lib/nvme/controller_registers.h"
#include "ecclesia/magent/lib/nvme/device_self_test_log.h"
#include "ecclesia/magent/lib/nvme/firmware_slot_info.h"
#include "ecclesia/magent/lib/nvme/identify_controller.h"
#include "ecclesia/magent/lib/nvme/identify_namespace.h"
#include "ecclesia/magent/lib/nvme/nvme_access.h"
#include "ecclesia/magent/lib/nvme/nvme_types.emb.h"
#include "ecclesia/magent/lib/nvme/sanitize_log_page.h"
#include "ecclesia/magent/lib/nvme/smart_log_page.h"

namespace ecclesia {

class NvmeDeviceInterface {
 public:
  virtual ~NvmeDeviceInterface() = default;

  // Executes the IdentifyController command on the device.
  virtual absl::StatusOr<std::unique_ptr<IdentifyController>> Identify()
      const = 0;

  // Executes IdentifyNamespaceList, putting the command in buf.
  // Lists valid namespace IDs strictly greater than starting_namespace_id.
  //
  // Note: may not return all the namespaces on the device: it only issues a
  // single command, which has finite capacity.  See EnumerateAllNamespaces().
  virtual absl::StatusOr<std::set<uint32_t>> EnumerateNamespacesAfter(
      uint32_t starting_namespace_id) = 0;

  // Executes IdentifyNamespace command for the provided namespace_id.
  virtual absl::StatusOr<IdentifyNamespace> GetNamespaceInfo(
      uint32_t namespace_id) = 0;

  // Gets the table of supported LBA formats from the device.  Order matters:
  // the index of each entry is the same as reported by the device, so it is
  // suitable for passing to CreateNamespace.
  virtual absl::StatusOr<std::vector<LBAFormatView>>
  GetSupportedLbaFormats() = 0;

  // Executes the SmartLogPage command on the device.
  virtual absl::StatusOr<std::unique_ptr<SmartLogPageInterface>> SmartLog()
      const = 0;

  // Executes the Sanitize Status Log Page command on the device.
  virtual absl::StatusOr<std::unique_ptr<SanitizeLogPageInterface>>
  SanitizeStatusLog() const = 0;

  // Get Firmware Slot Info via NVMe command
  // Get Log Page - Firmware Slot Information (Log Identifier 0x3).
  virtual absl::StatusOr<std::unique_ptr<FirmwareSlotInfo>>
  FirmwareSlotInformation() const = 0;

  // Get Device Self-test log via the NVMe command
  // Get Log Page - Device Self-test (Log Identifier 0x6)
  virtual absl::StatusOr<std::unique_ptr<DeviceSelfTestLog>>
  GetDeviceSelfTestLog() const = 0;

  // Enumerates all namespaces on the device.  May execute multiple
  // IdentifyListNamespaces commands if needed. Returns a set of namespace ids
  // (nsid).
  absl::StatusOr<std::set<uint32_t>> EnumerateAllNamespaces();

  // Enumerates all namespaces on the device, then gets the details about them.
  // Map key is the namespace id (nsid).
  absl::StatusOr<std::map<uint32_t, IdentifyNamespace>>
  EnumerateAllNamespacesAndInfo();

  // Attach/Detach the given controller to/from the given namespace.
  virtual absl::Status AttachNamespace(uint32_t namespace_id,
                                       uint16_t controller_id) = 0;
  virtual absl::Status DetachNamespace(uint32_t namespace_id,
                                       uint16_t controller_id) = 0;

  // Destroy the given namespace.  (You should Detach it first.)
  virtual absl::Status DestroyNamespace(uint32_t namespace_id) = 0;

  // Create a namespace with the given properties.  (Does not Attach it.)
  // Note: lba_format_ix is an index into the table of supported formats, and
  // capacity_lba is in units of the lba size specified in that format.
  // Returns the ID of the newly-created namespace.
  virtual absl::StatusOr<uint32_t> CreateNamespace(uint64_t capacity_lba,
                                                   uint8_t lba_format_ix) = 0;

  // Find an entry in the supported LBA format table with the given LBA size,
  // which must be a power of 2.  The entry must have Metadata Size 0.  The
  // matching entry with the best (closest to 0) Relative Performance is
  // returned.
  absl::StatusOr<uint8_t> IndexOfFormatWithLbaSize(uint64_t lba_size_bytes);

  // Issues Format NVM command (secure erase) against all namespaces.
  virtual absl::Status Format(uint8_t lba_format_ix) = 0;

  // Disable this feature becasue linux/nvme_ioctl.h in kokoro build image
  // ubuntu1604 is not up to date. After we migrate to a more robust build env,
  // this can re-enabled.
  // Rescans the namespaces on the device, for updated information on them.
  // virtual absl::Status RescanNamespaces() = 0;

  // Resets NVMe subsystem on the device.
  virtual absl::Status ResetSubsystem() = 0;

  // Resets NVMe controller on the device.
  virtual absl::Status ResetController() = 0;

  // Crypto erase device with no_dealloc and ause are set to false.
  virtual absl::Status SanitizeCryptoErase() = 0;

  // Block erase device with no_dealloc and ause are set to false.
  virtual absl::Status SanitizeBlockErase() = 0;

  // Overwrite sanitize with owpass set to 1. And no_dealloc, ause
  // are set to false.
  virtual absl::Status SanitizeOverwrite(uint32_t overwrite_pattern) = 0;

  // Exit Failure mode from previous failed unrestricted completion mode
  // Sanitize operation.
  virtual absl::Status SanitizeExitFailureMode() = 0;

  // Get a read-only view to the NVMe controller registers
  virtual absl::StatusOr<ControllerRegisters> GetControllerRegisters()
      const = 0;
};

// Create device interface using given access and transport interface.
std::unique_ptr<NvmeDeviceInterface> CreateNvmeDevice(
    std::unique_ptr<NvmeAccessInterface> access);

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_LIB_NVME_NVME_DEVICE_H_
