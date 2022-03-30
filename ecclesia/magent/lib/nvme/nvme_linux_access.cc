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

#include "ecclesia/magent/lib/nvme/nvme_linux_access.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/nvme_ioctl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <memory>
#include <string>
#include <utility>

#include "absl/cleanup/cleanup.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "ecclesia/lib/file/mmap.h"
#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/magent/lib/nvme/controller_registers.h"
#include "ecclesia/magent/lib/nvme/nvme_access.h"
#include "ecclesia/magent/lib/nvme/nvme_device.h"
#include "re2/re2.h"

namespace ecclesia {

absl::Status NvmeLinuxAccess::ExecuteAdminCommand(
    nvme_passthru_cmd *cmd) const {
  int fd = open(devpath_.c_str(), O_RDONLY, 0);
  if (fd < 0) {
    return absl::Status(absl::StatusCode::kFailedPrecondition,
                        absl::StrFormat("Couldn't open device at path %s: %s",
                                        devpath_, strerror(errno)));
  }
  absl::Cleanup fd_closer = [fd]() { close(fd); };

  struct stat nvme_stat;
  int err = fstat(fd, &nvme_stat);
  if (err < 0) {
    return absl::InternalError(
        absl::StrCat("fail to check file status with POSIX errno: ", errno));
  }
  if (!S_ISCHR(nvme_stat.st_mode) && !S_ISBLK(nvme_stat.st_mode)) {
    return absl::Status(absl::StatusCode::kFailedPrecondition,
                        "not a block or character device");
  }
  err = ioctl(fd, NVME_IOCTL_ADMIN_CMD, cmd);
  if (err < 0) {
    return absl::InternalError(
        absl::StrCat("ioctl failed with POSIX errno: ", errno));
  } else if (err != 0) {
    return absl::InternalError(
        absl::StrCat("ioctl failed with Generic Command Status number: ", err));
  }
  return absl::OkStatus();
}

absl::Status NvmeLinuxAccess::ResetSubsystem() {
  InfoLog() << "Resetting NVMe subsystem for " << devpath_;
  int fd = open(devpath_.c_str(), O_RDWR, 0);
  if (fd < 0) {
    return absl::InternalError(
        absl::StrCat("Failed to open block device path: ", devpath_));
  }
  absl::Cleanup fd_closer = [fd]() { close(fd); };
  const auto ret_code = ioctl(fd, NVME_IOCTL_SUBSYS_RESET);
  if (ret_code != 0) {
    return absl::InternalError(absl::StrCat(
        "Failed to call NVME_IOCTL_SUBSYS_RESET on device path: ", devpath_,
        " with POSIX errno:  ", ret_code));
  }
  return absl::OkStatus();
}

absl::Status NvmeLinuxAccess::ResetController() {
  InfoLog() << "Resetting NVMe primary controller for " << devpath_;
  int fd = open(devpath_.c_str(), O_RDWR, 0);
  if (fd < 0) {
    return absl::InternalError(
        absl::StrCat("Failed to open block device path: ", devpath_));
  }
  absl::Cleanup fd_closer = [fd]() { close(fd); };
  const auto ret_code = ioctl(fd, NVME_IOCTL_RESET);
  if (ret_code != 0) {
    return absl::InternalError(absl::StrCat(
        "Failed to call NVME_IOCTL_RESET on device path: ", devpath_,
        " with POSIX errno:  ", ret_code));
  }
  return absl::OkStatus();
}

absl::StatusOr<ControllerRegisters> NvmeLinuxAccess::GetControllerRegisters()
    const {
  static const LazyRE2 kRegexp = {"(nvme\\d+)"};
  std::string kernel_name;
  if (!RE2::PartialMatch(devpath_, *kRegexp, &kernel_name)) {
    return absl::NotFoundError(absl::StrFormat(
        "Failed to identify the kernel name from device path %s", devpath_));
  }
  // We want to mmap the resource0 file for the PCI node associated with the
  // NVMe device. On linux it can be found in
  // "/sys/class/nvme/<kernel_name>/device/resource0"
  const std::string pci_resource0_path =
      absl::StrFormat("/sys/class/nvme/%s/device/resource0", kernel_name);

  // mmapping the first 4k is sufficient to read any controller registers we
  // may be interested in.
  constexpr int kSize = 4096;
  constexpr int kOffset = 0;
  auto maybe_mmap = MappedMemory::Create(pci_resource0_path, kOffset, kSize,
                                         MappedMemory::Type::kReadOnly);
  assert(maybe_mmap.ok());
  MappedMemory mmap = std::move(*maybe_mmap);

  return ControllerRegisters::Parse(mmap.MemoryAsStringView());
}

std::unique_ptr<NvmeDeviceInterface> CreateNvmeLinuxDevice(
    const std::string &device_path) {
  auto access = std::make_unique<NvmeLinuxAccess>(device_path);
  return CreateNvmeDevice(std::move(access));
}

}  // namespace ecclesia
