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

// SMBus access routines using the kernel /dev interface.

#ifndef ECCLESIA_LIB_IO_SMBUS_KERNEL_DEV_H_
#define ECCLESIA_LIB_IO_SMBUS_KERNEL_DEV_H_

#include <cstddef>
#include <cstdint>
#include <string>

#include "absl/status/status.h"
#include "absl/types/span.h"
#include "ecclesia/lib/io/ioctl.h"
#include "ecclesia/lib/io/smbus/smbus.h"

namespace ecclesia {

// SMBus access interface using the kernel /dev/i2c-* files.
class KernelSmbusAccess : public SmbusAccessInterface {
 public:
  KernelSmbusAccess(std::string dev_dir, IoctlInterface *ioctl_intf);

  KernelSmbusAccess(const KernelSmbusAccess &) = delete;
  KernelSmbusAccess &operator=(const KernelSmbusAccess &) = delete;

  absl::Status ProbeDevice(const SmbusLocation &loc) const override;
  absl::Status WriteQuick(const SmbusLocation &loc,
                          uint8_t data) const override;
  absl::Status SendByte(const SmbusLocation &loc, uint8_t data) const override;
  absl::Status ReceiveByte(const SmbusLocation &loc,
                           uint8_t *data) const override;

  absl::Status Write8(const SmbusLocation &loc, int command,
                      uint8_t data) const override;
  absl::Status Read8(const SmbusLocation &loc, int command,
                     uint8_t *data) const override;
  absl::Status Write16(const SmbusLocation &loc, int command,
                       uint16_t data) const override;
  absl::Status Read16(const SmbusLocation &loc, int command,
                      uint16_t *data) const override;

  absl::Status WriteBlockI2C(
      const SmbusLocation &loc, int command,
      absl::Span<const unsigned char> data) const override;
  absl::Status ReadBlockI2C(const SmbusLocation &loc, int command,
                            absl::Span<unsigned char> data,
                            size_t *len) const override;
  bool SupportBlockRead(const SmbusLocation &loc) const override;

 private:
  int OpenI2CSlaveFile(const SmbusLocation &loc) const;
  int OpenI2CMasterFile(const SmbusBus &bus) const;
  int CheckFunctionality(int fd, uint32_t flags) const;

  std::string dev_dir_;
  IoctlInterface *ioctl_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_IO_SMBUS_KERNEL_DEV_H_
