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

#ifndef ECCLESIA_MAGENT_LIB_NVME_NVME_LINUX_ACCESS_H_
#define ECCLESIA_MAGENT_LIB_NVME_NVME_LINUX_ACCESS_H_

#include <memory>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ecclesia/magent/lib/nvme/controller_registers.h"
#include "ecclesia/magent/lib/nvme/nvme_access.h"
#include "ecclesia/magent/lib/nvme/nvme_device.h"

struct nvme_passthru_cmd;

namespace ecclesia {

class NvmeLinuxAccess : public NvmeAccessInterface {
 public:
  explicit NvmeLinuxAccess(const std::string &devpath) : devpath_(devpath) {}

  // Executes a single NVM-Express command.
  absl::Status ExecuteAdminCommand(nvme_passthru_cmd *cmd) const override;

  // Disable this feature becasue linux/nvme_ioctl.h in kokoro build image
  // ubuntu1604 is not up to date. After we migrate to a more robust build env,
  // this can re-enabled.
  // Fetches the most recent namespace data from the device.
  // absl::Status RescanNamespaces() override;

  // Resets NVMe subsystem on the device.
  absl::Status ResetSubsystem() override;

  // Resets NVMe controller on the device.
  absl::Status ResetController() override;

  absl::StatusOr<ControllerRegisters> GetControllerRegisters() const override;

 private:
  const std::string devpath_;
};

// Create device interface using Linux kernel IOCTL and PCie transport.
std::unique_ptr<NvmeDeviceInterface> CreateNvmeLinuxDevice(
    const std::string &device_path);

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_LIB_NVME_NVME_LINUX_ACCESS_H_
