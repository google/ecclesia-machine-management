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

#include "ecclesia/magent/lib/nvme/nvme_device.h"

#include <linux/nvme_ioctl.h>

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "ecclesia/magent/lib/nvme/controller_registers.h"
#include "ecclesia/magent/lib/nvme/device_self_test_log.h"
#include "ecclesia/magent/lib/nvme/firmware_slot_info.h"
#include "ecclesia/magent/lib/nvme/identify_controller.h"
#include "ecclesia/magent/lib/nvme/identify_namespace.h"
#include "ecclesia/magent/lib/nvme/nvme_access.h"
#include "ecclesia/magent/lib/nvme/nvme_types.emb.h"
#include "ecclesia/magent/lib/nvme/sanitize_log_page.h"
#include "ecclesia/magent/lib/nvme/smart_log_page.h"
#include "runtime/cpp/emboss_cpp_util.h"
#include "runtime/cpp/emboss_prelude.h"

namespace ecclesia {

class NvmeDevice : public NvmeDeviceInterface {
 public:
  explicit NvmeDevice(std::unique_ptr<NvmeAccessInterface> access)
      : access_(std::move(access)) {}

  absl::StatusOr<std::unique_ptr<IdentifyController>> Identify()
      const override {
    static constexpr uint8_t kNvmeOpcodeAdminIdentify = 0x06;
    static constexpr uint8_t kCnsIdentifyController = 0x01;

    // A buffer to hold the response structure which will be parsed.
    std::string buffer(IdentifyControllerFormat::IntrinsicSizeInBytes(), '\0');

    nvme_passthru_cmd cmd = {
        .opcode = kNvmeOpcodeAdminIdentify,
        .nsid = 0,
        .addr = reinterpret_cast<uint64_t>(buffer.c_str()),
        .data_len = IdentifyControllerFormat::IntrinsicSizeInBytes(),
        .cdw10 = kCnsIdentifyController,
        .cdw11 = 0,
        .cdw12 = 0,
        .cdw13 = 0,
        .timeout_ms = kTimeoutMs,
    };

    if (auto status = access_->ExecuteAdminCommand(&cmd); !status.ok()) {
      return absl::Status(
          status.code(),
          absl::StrCat(status.message(), ";",
                       "Failed to execute IdentifyController command"));
    }

    auto identify = IdentifyController::Parse(buffer);

    if (identify == nullptr) {
      return absl::InternalError(
          "Failed to parse returned IdentifyController response.");
    }

    return identify;
  }

  absl::StatusOr<std::set<uint32_t>> EnumerateNamespacesAfter(
      uint32_t starting_namespace_id) override {
    static constexpr uint8_t kNvmeOpcodeAdminIdentify = 0x06;
    static constexpr uint8_t kCnsIdentifyListNs = 0x02;

    // A buffer to hold the response structure which will be parsed.
    std::string buffer(IdentifyListNamespaceFormat::IntrinsicSizeInBytes(),
                       '\0');

    nvme_passthru_cmd cmd = {
        .opcode = kNvmeOpcodeAdminIdentify,
        .nsid = 0,
        .addr = reinterpret_cast<uint64_t>(buffer.c_str()),
        .data_len = IdentifyListNamespaceFormat::IntrinsicSizeInBytes(),
        .cdw10 = kCnsIdentifyListNs,
        .cdw11 = 0,
        .cdw12 = 0,
        .cdw13 = 0,
        .timeout_ms = kTimeoutMs,
    };

    if (auto status = access_->ExecuteAdminCommand(&cmd); !status.ok()) {
      return absl::Status(
          status.code(),
          absl::StrCat(status.message(), ";",
                       "Failed to execute IdentifyListNamespaces command"));
    }

    return ParseIdentifyListNamespace(buffer);
  }

  absl::StatusOr<std::string> InternalIdentifyNamespace(uint32_t namespace_id) {
    static constexpr uint8_t kNvmeOpcodeAdminIdentify = 0x06;
    static constexpr uint8_t kCnsIdentifyNs = 0x00;

    // A buffer to hold the response structure which will be parsed.
    std::string buffer(IdentifyNamespaceFormat::IntrinsicSizeInBytes(), '\0');

    nvme_passthru_cmd cmd = {
        .opcode = kNvmeOpcodeAdminIdentify,
        .nsid = namespace_id,
        .addr = reinterpret_cast<uint64_t>(buffer.c_str()),
        .data_len = IdentifyNamespaceFormat::IntrinsicSizeInBytes(),
        .cdw10 = kCnsIdentifyNs,
        .cdw11 = 0,
        .cdw12 = 0,
        .cdw13 = 0,
        .timeout_ms = kTimeoutMs,
    };

    if (auto status = access_->ExecuteAdminCommand(&cmd); !status.ok()) {
      std::stringstream stream;
      stream << "Failed to execute IdentifyNamespace command for namespace_id "
             << namespace_id << " (0x" << std::hex << namespace_id << ")";
      return absl::Status(status.code(),
                          absl::StrCat(status.message(), ";", stream.str()));
    }

    return buffer;
  }

  absl::StatusOr<IdentifyNamespace> GetNamespaceInfo(
      uint32_t namespace_id) override {
    auto ret = InternalIdentifyNamespace(namespace_id);
    if (!ret.ok()) return ret.status();
    return IdentifyNamespace::Parse(ret.value());
  }

  absl::StatusOr<std::vector<LBAFormatView>> GetSupportedLbaFormats() override {
    static constexpr uint32_t kAllNamespaces = 0xFFFFFFFF;
    auto ret = InternalIdentifyNamespace(kAllNamespaces);
    if (!ret.ok()) return ret.status();
    return IdentifyNamespace::GetSupportedLbaFormats(ret.value());
  }

  absl::StatusOr<std::unique_ptr<SmartLogPageInterface>> SmartLog()
      const override {
    static constexpr uint32_t kAllNamespaces = -1;
    auto ret =
        GetLogPage(LogPageIdentifier::kSmart,
                   SmartLogPageFormat::IntrinsicSizeInBytes(), kAllNamespaces);
    if (!ret.ok()) {
      return absl::Status(
          ret.status().code(),
          absl::StrCat(ret.status().message(), ";",
                       "Failed to execute kSmartLogPage command."));
    }

    auto smart_log = SmartLogPage::Parse(ret.value());
    if (smart_log == nullptr) {
      return absl::InternalError(
          "Failed to parse returned IdentifyController response.");
    }
    return smart_log;
  }

  absl::StatusOr<std::unique_ptr<SanitizeLogPageInterface>> SanitizeStatusLog()
      const override {
    static constexpr uint32_t kAllNamespaces = -1;
    auto ret = GetLogPage(LogPageIdentifier::kSanitizeStatus,
                          SanitizeLogPageFormat::IntrinsicSizeInBytes(),
                          kAllNamespaces);
    if (!ret.ok()) {
      return absl::Status(
          ret.status().code(),
          absl::StrCat(ret.status().message(), ";",
                       "Failed to execute kSanitizeLogPage command."));
    }

    return SanitizeLogPage::Parse(ret.value());
  }

  absl::StatusOr<std::unique_ptr<FirmwareSlotInfo>> FirmwareSlotInformation()
      const override;

  absl::StatusOr<std::unique_ptr<DeviceSelfTestLog>> GetDeviceSelfTestLog()
      const override;

  absl::Status AttachNamespace(uint32_t namespace_id,
                               uint16_t controller_id) override {
    static constexpr uint8_t kNvmeOpcodeAdminAttachNs = 0x15;
    static constexpr uint8_t kAttachType = 0x00;

    std::string buffer(ControllerListFormat::IntrinsicSizeInBytes(), '\0');
    auto controller_list = MakeControllerListFormatView(&buffer);
    controller_list.num_identifiers().Write(1);
    controller_list.identifiers()[0].Write(controller_id);

    nvme_passthru_cmd cmd = {
        .opcode = kNvmeOpcodeAdminAttachNs,
        .nsid = namespace_id,
        .addr = reinterpret_cast<uint64_t>(&buffer[0]),
        .data_len = static_cast<uint32_t>(buffer.size()),
        .cdw10 = kAttachType,
        .cdw11 = 0,
        .cdw12 = 0,
        .cdw13 = 0,
        .timeout_ms = kTimeoutMs,
    };

    if (auto status = access_->ExecuteAdminCommand(&cmd); !status.ok()) {
      return absl::Status(
          status.code(),
          absl::StrCat(
              status.message(), ";",
              "Failed to execute AttachNamespace command for namespace_id ",
              namespace_id));
    }

    return absl::OkStatus();
  }

  absl::Status DetachNamespace(uint32_t namespace_id,
                               uint16_t controller_id) override {
    static constexpr uint8_t kNvmeOpcodeAdminAttachNs = 0x15;
    static constexpr uint8_t kAttachType = 0x01;

    std::string buffer(ControllerListFormat::IntrinsicSizeInBytes(), '\0');
    auto controller_list = MakeControllerListFormatView(&buffer);
    controller_list.num_identifiers().Write(1);
    controller_list.identifiers()[0].Write(controller_id);

    nvme_passthru_cmd cmd = {
        .opcode = kNvmeOpcodeAdminAttachNs,
        .nsid = namespace_id,
        .addr = reinterpret_cast<uint64_t>(&buffer[0]),
        .data_len = static_cast<uint32_t>(buffer.size()),
        .cdw10 = kAttachType,
        .cdw11 = 0,
        .cdw12 = 0,
        .cdw13 = 0,
        .timeout_ms = kTimeoutMs,
    };

    if (auto status = access_->ExecuteAdminCommand(&cmd); !status.ok()) {
      return absl::Status(
          status.code(),
          absl::StrCat(
              status.message(), ";",
              "Failed to execute DetachNamespace command for namespace_id ",
              namespace_id));
    }

    return absl::OkStatus();
  }

  absl::Status DestroyNamespace(uint32_t namespace_id) override {
    static constexpr uint8_t kNvmeOpcodeAdminNsMgt = 0x0d;
    constexpr uint8_t kMgtType = 0x01;

    nvme_passthru_cmd cmd = {
        .opcode = kNvmeOpcodeAdminNsMgt,
        .nsid = namespace_id,
        .addr = 0,
        .data_len = 0,
        .cdw10 = kMgtType,
        .cdw11 = 0,
        .cdw12 = 0,
        .cdw13 = 0,
        .timeout_ms = kTimeoutMs,
    };

    if (auto status = access_->ExecuteAdminCommand(&cmd); !status.ok()) {
      std::stringstream stream;
      stream << "Failed to execute DestroyNamespace command for namespace_id "
             << namespace_id << " (0x" << std::hex << namespace_id << ")";
      return absl::Status(status.code(),
                          absl::StrCat(status.message(), ";", stream.str()));
    }

    return absl::OkStatus();
  }

  absl::StatusOr<uint32_t> CreateNamespace(uint64_t capacity_lba,
                                           uint8_t lba_format_ix) override {
    if (lba_format_ix >= 16) {
      return absl::InternalError("Invalid LBA format index.");
    }

    static constexpr uint8_t kNvmeOpcodeAdminNsMgt = 0x0d;
    static constexpr uint8_t kMgtType = 0x00;

    std::string buffer(NamespaceManagementFormat::IntrinsicSizeInBytes(), '\0');
    auto nms = MakeNamespaceManagementFormatView(&buffer);
    nms.size().Write(capacity_lba);
    nms.capacity().Write(capacity_lba);
    nms.formatted_lba_size().Write(lba_format_ix);

    nvme_passthru_cmd cmd = {
        .opcode = kNvmeOpcodeAdminNsMgt,
        .nsid = 0,
        .addr = reinterpret_cast<uint64_t>(&buffer[0]),
        .data_len = static_cast<uint32_t>(buffer.size()),
        .cdw10 = kMgtType,
        .cdw11 = 0,
        .cdw12 = 0,
        .cdw13 = 0,
        .timeout_ms = kTimeoutMs,
    };

    if (auto status = access_->ExecuteAdminCommand(&cmd); !status.ok()) {
      return absl::Status(
          status.code(),
          absl::StrCat(status.message(), ";",
                       "Failed to execute CreateNamespace command."));
    }

    // cmd.result contains the ID of the newly-created namespace.
    return cmd.result;
  }

  absl::Status Format(uint8_t lba_format_ix) override {
    if (lba_format_ix >= 16) {
      return absl::InternalError("Invalid LBA format index.");
    }

    static constexpr uint8_t kNvmeOpcodeFormatNvm = 0x80;
    static constexpr uint32_t kAllNamespaces = 0xFFFFFFFF;
    static constexpr uint32_t kCryptoEraseMask = 1 << 10;

    nvme_passthru_cmd cmd = {
        .opcode = kNvmeOpcodeFormatNvm,
        .nsid = kAllNamespaces,
        .addr = 0,
        .data_len = 0,
        .cdw10 = lba_format_ix | kCryptoEraseMask,
        .cdw11 = 0,
        .cdw12 = 0,
        .cdw13 = 0,
        .timeout_ms = kTimeoutMs,
    };

    if (auto status = access_->ExecuteAdminCommand(&cmd); !status.ok()) {
      return absl::Status(status.code(),
                          absl::StrCat(status.message(), ";",
                                       "Failed to execute FormatNVM command."));
    }

    return absl::OkStatus();
  }

  // absl::Status RescanNamespaces() override {
  //  return access_->RescanNamespaces();
  // }

  absl::Status ResetSubsystem() override { return access_->ResetSubsystem(); }

  absl::Status ResetController() override { return access_->ResetController(); }

  absl::Status SanitizeCryptoErase() override {
    static constexpr uint8_t kNvmeOpcodeSanitizeNvm = 0x84;
    nvme_passthru_cmd cmd = {
        .opcode = kNvmeOpcodeSanitizeNvm,
        .nsid = 0,
        .addr = 0,
        .data_len = 0,
        .cdw10 = static_cast<uint32_t>(SanitizeAction::CRYPTO_ERASE),
        .cdw11 = 0,
    };

    if (auto status = access_->ExecuteAdminCommand(&cmd); !status.ok()) {
      return absl::Status(
          status.code(),
          absl::StrCat(status.message(), ";",
                       "Failed to execute CryptoSanitize command."));
    }

    return absl::OkStatus();
  }

  absl::Status SanitizeBlockErase() override {
    static constexpr uint8_t kNvmeOpcodeSanitizeNvm = 0x84;
    nvme_passthru_cmd cmd = {
        .opcode = kNvmeOpcodeSanitizeNvm,
        .nsid = 0,
        .addr = 0,
        .data_len = 0,
        .cdw10 = static_cast<uint32_t>(SanitizeAction::BLOCK_ERASE),
        .cdw11 = 0,
    };

    if (auto status = access_->ExecuteAdminCommand(&cmd); !status.ok()) {
      return absl::Status(
          status.code(),
          absl::StrCat(status.message(), ";",
                       "Failed to execute BlockSanitize command."));
    }

    return absl::OkStatus();
  }

  absl::Status SanitizeOverwrite(uint32_t overwrite_pattern) override {
    static constexpr uint8_t kNvmeOpcodeSanitizeNvm = 0x84;
    static constexpr uint32_t kOverwritePassCountMask = 1 << 4;
    nvme_passthru_cmd cmd = {
        .opcode = kNvmeOpcodeSanitizeNvm,
        .nsid = 0,
        .addr = 0,
        .data_len = 0,
        .cdw10 = static_cast<uint32_t>(kOverwritePassCountMask) |
                 static_cast<uint32_t>(SanitizeAction::OVERWRITE),
        .cdw11 = overwrite_pattern,
    };

    if (auto status = access_->ExecuteAdminCommand(&cmd); !status.ok()) {
      return absl::Status(
          status.code(),
          absl::StrCat(status.message(), ";",
                       "Failed to execute SanitizeOverwrite command."));
    }

    return absl::OkStatus();
  }

  absl::Status SanitizeExitFailureMode() override {
    static constexpr uint8_t kNvmeOpcodeSanitizeNvm = 0x84;
    nvme_passthru_cmd cmd = {
        .opcode = kNvmeOpcodeSanitizeNvm,
        .nsid = 0,
        .addr = 0,
        .data_len = 0,
        .cdw10 = static_cast<uint32_t>(SanitizeAction::EXIT_FAILURE_MODE),
        .cdw11 = 0,
    };

    if (auto status = access_->ExecuteAdminCommand(&cmd); !status.ok()) {
      return absl::Status(
          status.code(),
          absl::StrCat(status.message(), ";",
                       "Failed to execute SanitizeExitFailureMode command."));
    }

    return absl::OkStatus();
  }

  absl::StatusOr<ControllerRegisters> GetControllerRegisters() const override {
    return access_->GetControllerRegisters();
  }

 private:
  enum LogPageIdentifier : uint8_t {
    kErrorInformation = 0x1,
    kSmart = 0x2,
    kFirmwareSlotInformation = 0x3,
    kDeviceSelfTest = 0x6,
    kTelemetryHostInitiated = 0x7,
    kSanitizeStatus = 0x81,
  };

  // Execute the GetLogPage command.
  absl::StatusOr<std::string> GetLogPage(LogPageIdentifier id, uint32_t len,
                                         uint32_t nsid) const;

  // GetLogPage with an offset within the log page to get the data from.
  absl::Status GetLogPageWithOffset(LogPageIdentifier id, uint32_t len,
                                    uint32_t nsid, uint32_t offset,
                                    uint8_t *buf) const;

  // Some basic sanity check so commands don't go forever.
  // Note that commands may get blocked behind others in the queue (e.g.
  // Format), so it's unwise to use a short timeout on Identify: b/134591769.
  static constexpr int kTimeoutMs = 10 * 60 * 1000;

  // Interface for sending commands to the device (e.g. via Linux ioctl).
  const std::unique_ptr<NvmeAccessInterface> access_;
};

absl::StatusOr<std::set<uint32_t>>
NvmeDeviceInterface::EnumerateAllNamespaces() {
  // Starting at nsid 0 ensures we get all of them.
  uint32_t next_first_nsid = 0;

  // Holds all the NSIDs reported by the device, across all commands.
  std::set<uint32_t> all_nsids;

  // Repeatedly fetch buffers full of NSIDs until we've seen them all.
  while (true) {
    // Get the next crop of NSIDs, starting at the last-known NSID.
    auto ret = EnumerateNamespacesAfter(next_first_nsid);
    if (!ret.ok()) {
      return absl::Status(ret.status().code(),
                          absl::StrCat(ret.status().message(), ";",
                                       "Unable to fetch NSIDs starting with ",
                                       next_first_nsid));
    }
    auto cmd_nsids = std::move(ret.value());

    // Copy all the newly-found NSIDs to all_nsids.
    all_nsids.insert(cmd_nsids.begin(), cmd_nsids.end());

    // If this list was not full, we know there are no more to fetch.
    if (cmd_nsids.size() < IdentifyListNamespaceFormat::capacity()) {
      break;
    }

    // The next NSID we need to check is the one just past the last one we saw.
    next_first_nsid = *cmd_nsids.rbegin();
  }

  return all_nsids;
}

absl::StatusOr<std::map<uint32_t, IdentifyNamespace>>
NvmeDeviceInterface::EnumerateAllNamespacesAndInfo() {
  auto ret = EnumerateAllNamespaces();
  if (!ret.ok()) {
    return absl::Status(ret.status().code(), ret.status().message());
  }
  auto namespaces = std::move(ret.value());

  std::map<uint32_t, IdentifyNamespace> ns_with_info;

  for (uint32_t nsid : namespaces) {
    auto ret = GetNamespaceInfo(nsid);
    if (!ret.ok()) {
      return absl::Status(
          ret.status().code(),
          absl::StrCat(ret.status().message(), ";",
                       "Could not get info about namespace ", nsid));
    }
    auto ns_info = std::move(ret.value());
    ns_with_info.insert({nsid, ns_info});
  }

  return ns_with_info;
}

absl::StatusOr<uint8_t> NvmeDeviceInterface::IndexOfFormatWithLbaSize(
    uint64_t lba_size_bytes) {
  auto ret = GetSupportedLbaFormats();
  if (!ret.ok()) {
    return absl::Status(
        ret.status().code(),
        absl::StrCat(ret.status().message(), ";",
                     "Failed to get list of LBA formats the device supports."));
  }
  auto supported_formats = std::move(ret.value());
  if (supported_formats.size() > 16) {
    return absl::InternalError("Too many supported formats!");
  }

  int8_t best_matching_format_index = -1;

  for (int8_t i = 0; i < supported_formats.size(); ++i) {
    const LBAFormatView &format = supported_formats[i];
    uint64_t data_size_bytes = 1ULL << format.data_size().Read();
    if (format.metadata_size().Read() == 0 &&
        lba_size_bytes == data_size_bytes) {
      if (best_matching_format_index < 0) {
        best_matching_format_index = i;
      } else if (format.relative_performance().Read() <
                 supported_formats[best_matching_format_index]
                     .relative_performance()
                     .Read()) {
        best_matching_format_index = i;
      }
    }
  }

  if (best_matching_format_index < 0) {
    return absl::NotFoundError(
        absl::StrCat("No format found matching lba_size ", lba_size_bytes));
  }

  return best_matching_format_index;
}

absl::Status NvmeDevice::GetLogPageWithOffset(LogPageIdentifier id,
                                              uint32_t len, uint32_t nsid,
                                              uint32_t offset,
                                              uint8_t *buf) const {
  static constexpr uint8_t kNvmeOpcodeGetLogPage = 0x2;
  const uint32_t numd = (len >> 2) - 1;
  const uint16_t numdl = numd & 0xffff;
  const uint16_t numdu = numd >> 16;

  nvme_passthru_cmd cmd = {
      .opcode = kNvmeOpcodeGetLogPage,
      .nsid = nsid,
      .addr = reinterpret_cast<uint64_t>(buf),
      .data_len = len,
      .cdw10 = static_cast<uint32_t>(id | numdl << 16),
      .cdw11 = numdu,
      .cdw12 = offset,
      .cdw13 = 0,
      .timeout_ms = kTimeoutMs,
  };

  if (auto status = access_->ExecuteAdminCommand(&cmd); !status.ok()) {
    return absl::Status(status.code(),
                        absl::StrCat(status.message(), ";",
                                     "Failed to execute GetLogPage command."));
  }

  return absl::OkStatus();
}

absl::StatusOr<std::string> NvmeDevice::GetLogPage(LogPageIdentifier id,
                                                   uint32_t len,
                                                   uint32_t nsid) const {
  // In reality this value should be derived from controller PCIe register
  // CAP.MPSMIN. Until we add the ability to read CAP.MPSMIN, we can safely
  // assume the minimum possible value.
  static constexpr size_t kMinimumMemoryPageSize = 4096;
  size_t max_data_transfer_size = 0;
  // Determine the maximum data transfer size. If "len" is greater than this,
  // break up the transfer into chunks.
  if (len <= kMinimumMemoryPageSize) {
    // If the requested len is less than 4K, we can avoid an ioctl to determine
    // the actual max_data_transfer_size, since we can get the requested data in
    // one shot.
    max_data_transfer_size = kMinimumMemoryPageSize;
  } else {
    auto ret = Identify();
    if (!ret.ok()) {
      return absl::Status(
          ret.status().code(),
          absl::StrCat(ret.status().message(), ";",
                       "Failed to get Identify Controller structure."));
    }
    auto identify = std::move(ret.value());
    max_data_transfer_size =
        (static_cast<size_t>(0x1) << identify->max_data_transfer_size()) *
        kMinimumMemoryPageSize;
  }

  size_t offset = 0;
  std::string buffer(len, '\0');
  do {
    size_t chunk_size =
        len <= max_data_transfer_size ? len : max_data_transfer_size;
    if (auto status = GetLogPageWithOffset(
            id, chunk_size, nsid, offset,
            reinterpret_cast<uint8_t *>(buffer.data() + offset));
        !status.ok()) {
      return absl::Status(
          status.code(),
          absl::StrCat(status.message(), ";",
                       absl::StrFormat("Failed to get %d bytes from offset %d "
                                       "for log page identifier "
                                       "0x%x.",
                                       chunk_size, offset, id)));
    }
    len -= chunk_size;
    offset += chunk_size;
  } while (len > 0);
  return buffer;
}

absl::StatusOr<std::unique_ptr<FirmwareSlotInfo>>
NvmeDevice::FirmwareSlotInformation() const {
  static constexpr uint32_t kAllNamespaces = -1;
  auto ret = GetLogPage(LogPageIdentifier::kFirmwareSlotInformation,
                        FirmwareSlotInfoFormat::IntrinsicSizeInBytes(),
                        kAllNamespaces);
  if (!ret.ok()) {
    return absl::Status(ret.status().code(), ret.status().message());
  }
  auto buffer = std::move(ret.value());
  return FirmwareSlotInfo::Parse(buffer);
}

absl::StatusOr<std::unique_ptr<DeviceSelfTestLog>>
NvmeDevice::GetDeviceSelfTestLog() const {
  static constexpr uint32_t kAllNamespaces = -1;
  auto ret = GetLogPage(LogPageIdentifier::kDeviceSelfTest,
                        DeviceSelfTestLogFormat::IntrinsicSizeInBytes(),
                        kAllNamespaces);
  if (!ret.ok()) {
    return absl::Status(ret.status().code(), ret.status().message());
  }
  auto buffer = std::move(ret.value());
  return DeviceSelfTestLog::Parse(buffer);
}

// Create device interface using given access and transport interface.
std::unique_ptr<NvmeDeviceInterface> CreateNvmeDevice(
    std::unique_ptr<NvmeAccessInterface> access) {
  return std::unique_ptr<NvmeDeviceInterface>(
      new NvmeDevice(std::move(access)));
}

}  // namespace ecclesia
