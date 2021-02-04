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

#include "ecclesia/lib/io/smbus/kernel_dev.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/sysinfo.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ecclesia/lib/cleanup/cleanup.h"
#include "ecclesia/lib/io/ioctl.h"
#include "ecclesia/lib/io/smbus/smbus.h"
#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/lib/logging/posix.h"

namespace ecclesia {

namespace {

// The default root hierarchy to look for i2c-* files.
constexpr char kDevRoot[] = "/dev";

int SmbusIoctl(IoctlInterface *ioctl_intf, int fd, uint8_t read_write,
               uint8_t command, int size, union i2c_smbus_data *data) {
  struct i2c_smbus_ioctl_data args;

  args.read_write = read_write;
  args.command = command;
  args.size = size;
  args.data = data;
  return ioctl_intf->Call(fd, I2C_SMBUS, &args);
}

}  // namespace

KernelSmbusAccess::KernelSmbusAccess(std::string dev_dir,
                                     IoctlInterface *ioctl_intf)
    : dev_dir_(std::move(dev_dir)), ioctl_(ioctl_intf) {
  if (dev_dir_.empty()) {
    dev_dir_ = kDevRoot;
  }
}

// Format the correct string and open the bus master device file for specified
// bus location.
// Returns file descriptor on success and -errno on error.
int KernelSmbusAccess::OpenI2CMasterFile(const SmbusBus &bus) const {
  std::string dev_filename =
      absl::StrFormat("%s/i2c-%d", dev_dir_, bus.value());

  int ret = open(dev_filename.c_str(), O_RDWR);
  if (ret < 0) {
    ret = -errno;
    PosixErrorLog() << "Unable to open " << dev_filename;
  }
  return ret;
}

// Open the device file for the device at 'loc' and set the requested
// slave address.
// Returns file descriptor on success and -errno on error.
int KernelSmbusAccess::OpenI2CSlaveFile(const SmbusLocation &loc) const {
  int ret = OpenI2CMasterFile(loc.bus());
  if (ret < 0) {
    return ret;
  }
  int fd = ret;

  if (ioctl_->Call(fd, I2C_SLAVE, loc.address().value()) < 0) {
    auto fd_closer = LambdaCleanup([fd]() { close(fd); });
    ret = -errno;
    PosixErrorLog() << "SMBus device " << loc << ": "
                    << "Unable to set slave address to 0x" << std::hex
                    << loc.address().value();
    return ret;
  }
  return fd;
}

// Check the functionality of the adapter driver of the i2c slave at 'fd'
// for the given I2C_FUNC_* flags in 'flags'
// Returns 0 if requested functionality is supported, -EOPNOTSUPP if not
// supported and -errno on error
int KernelSmbusAccess::CheckFunctionality(int fd, uint32_t flags) const {
  int ret = 0;
  uint32_t funcs = 0;

  int result = ioctl_->Call(fd, I2C_FUNCS, &funcs);
  if (result < 0) {
    ret = -errno;
  } else if (!(funcs & flags)) {
    ret = -EOPNOTSUPP;
  }
  return ret;
}

absl::Status KernelSmbusAccess::ProbeDevice(const SmbusLocation &loc) const {
  // This is the same device probing logic used in i2cdetect.
  absl::Status status;
  if ((loc.address().value() >= 0x30 && loc.address().value() <= 0x37) ||
      (loc.address().value() >= 0x50 && loc.address().value() <= 0x5f)) {
    // Quick write can corrupt Atmel EEPROMs, so use a short read instead.
    uint8_t data;
    status = Read8(loc, 0, &data);
  } else {
    // Read byte can lock the bus for write-only devices.
    // Same with a write quick of '1', see http://b/2374009 for details.
    status = WriteQuick(loc, 0);
  }

  if (!status.ok()) {
    ErrorLog() << status.message();
    return absl::NotFoundError(absl::StrFormat(
        "SMBus device %s: probe found no device.", absl::FormatStreamed(loc)));
  }

  return status;
}

absl::Status KernelSmbusAccess::WriteQuick(const SmbusLocation &loc,
                                           uint8_t data) const {
  // Open connection to i2c slave.
  int fd = OpenI2CSlaveFile(loc);
  if (fd < 0) {
    return absl::InternalError(
        absl::StrFormat("Open device %s failed.", absl::FormatStreamed(loc)));
  }

  auto fd_closer = LambdaCleanup([fd]() { close(fd); });

  absl::Status status;
  int result =
      SmbusIoctl(ioctl_, fd, data, I2C_SMBUS_WRITE, I2C_SMBUS_QUICK, nullptr);

  if (result < 0) {
    status = absl::InternalError(absl::StrFormat(
        "WriteQuick to device %s failed.", absl::FormatStreamed(loc)));
  }

  return status;
}

absl::Status KernelSmbusAccess::SendByte(const SmbusLocation &loc,
                                         uint8_t data) const {
  // Open connection to i2c slave.
  int fd = OpenI2CSlaveFile(loc);
  if (fd < 0) {
    return absl::InternalError(
        absl::StrFormat("Open device %s failed.", absl::FormatStreamed(loc)));
  }

  auto fd_closer = LambdaCleanup([fd]() { close(fd); });

  absl::Status status;
  int result =
      SmbusIoctl(ioctl_, fd, I2C_SMBUS_WRITE, data, I2C_SMBUS_BYTE, nullptr);

  if (result < 0) {
    status = absl::InternalError(absl::StrFormat(
        "SendByte to device %s failed.", absl::FormatStreamed(loc)));
  }

  return status;
}

absl::Status KernelSmbusAccess::ReceiveByte(const SmbusLocation &loc,
                                            uint8_t *data) const {
  // Open connection to i2c slave.
  int fd = OpenI2CSlaveFile(loc);
  if (fd < 0) {
    return absl::InternalError(
        absl::StrFormat("Open device %s failed.", absl::FormatStreamed(loc)));
  }

  auto fd_closer = LambdaCleanup([fd]() { close(fd); });

  absl::Status status;
  union i2c_smbus_data i2c_data{0};
  if (SmbusIoctl(ioctl_, fd, I2C_SMBUS_READ, 0, I2C_SMBUS_BYTE, &i2c_data) <
      0) {
    status = absl::InternalError(absl::StrFormat(
        "ReceiveByte from device %s failed.", absl::FormatStreamed(loc)));
  } else {
    *data = static_cast<uint8_t>(i2c_data.byte);
  }

  return status;
}

absl::Status KernelSmbusAccess::Write8(const SmbusLocation &loc, int command,
                                       uint8_t data) const {
  // Open connection to i2c slave.
  int fd = OpenI2CSlaveFile(loc);
  if (fd < 0) {
    return absl::InternalError(
        absl::StrFormat("Open device %s failed.", absl::FormatStreamed(loc)));
  }

  auto fd_closer = LambdaCleanup([fd]() { close(fd); });

  absl::Status status;
  union i2c_smbus_data i2c_data{};
  i2c_data.byte = data;
  if (SmbusIoctl(ioctl_, fd, I2C_SMBUS_WRITE, command, I2C_SMBUS_BYTE_DATA,
                 &i2c_data) < 0) {
    status = absl::InternalError(absl::StrFormat("Write8 to device %s failed.",
                                                 absl::FormatStreamed(loc)));
  }

  return status;
}

absl::Status KernelSmbusAccess::Read8(const SmbusLocation &loc, int command,
                                      uint8_t *data) const {
  // Open connection to i2c slave.
  int fd = OpenI2CSlaveFile(loc);
  if (fd < 0) {
    return absl::InternalError(
        absl::StrFormat("Open device %s failed.", absl::FormatStreamed(loc)));
  }

  auto fd_closer = LambdaCleanup([fd]() { close(fd); });

  absl::Status status;
  union i2c_smbus_data i2c_data{};
  if (SmbusIoctl(ioctl_, fd, I2C_SMBUS_READ, command, I2C_SMBUS_BYTE_DATA,
                 &i2c_data) < 0) {
    status = absl::InternalError(absl::StrFormat("Read8 from device %s failed.",
                                                 absl::FormatStreamed(loc)));
  } else {
    *data = static_cast<uint8_t>(i2c_data.byte);
  }

  return status;
}

absl::Status KernelSmbusAccess::Write16(const SmbusLocation &loc, int command,
                                        uint16_t data) const {
  // Open connection to i2c slave.
  int fd = OpenI2CSlaveFile(loc);
  if (fd < 0) {
    return absl::InternalError(
        absl::StrFormat("Open device %s failed.", absl::FormatStreamed(loc)));
  }

  auto fd_closer = LambdaCleanup([fd]() { close(fd); });

  absl::Status status;
  union i2c_smbus_data i2c_data{};
  i2c_data.word = data;
  if (SmbusIoctl(ioctl_, fd, I2C_SMBUS_WRITE, command, I2C_SMBUS_WORD_DATA,
                 &i2c_data) < 0) {
    status = absl::InternalError(absl::StrFormat(
        "Write16 from device %s failed.", absl::FormatStreamed(loc)));
  }

  return status;
}

absl::Status KernelSmbusAccess::Read16(const SmbusLocation &loc, int command,
                                       uint16_t *data) const {
  // Open connection to i2c slave.
  int fd = OpenI2CSlaveFile(loc);
  if (fd < 0) {
    return absl::InternalError(
        absl::StrFormat("Open device %s failed.", absl::FormatStreamed(loc)));
  }

  auto fd_closer = LambdaCleanup([fd]() { close(fd); });

  absl::Status status;
  union i2c_smbus_data i2c_data{};
  if (SmbusIoctl(ioctl_, fd, I2C_SMBUS_READ, command, I2C_SMBUS_WORD_DATA,
                 &i2c_data) < 0) {
    status = absl::InternalError(absl::StrFormat(
        "Read16 from device %s failed.", absl::FormatStreamed(loc)));
  } else {
    *data = static_cast<uint16_t>(i2c_data.word);
  }

  return status;
}

absl::Status KernelSmbusAccess::WriteBlockI2C(
    const SmbusLocation &loc, int command,
    absl::Span<const unsigned char> data) const {
  // Linux interface only supports up to 32 bytes.
  if (data.size() > I2C_SMBUS_BLOCK_MAX) {
    return absl::InternalError(
        absl::StrFormat("Can not write %d to device %s, "
                        "Linux interface only supports up to 32 bytes.",
                        data.size(), absl::FormatStreamed(loc)));
  }

  // Open connection to i2c slave.
  int fd = OpenI2CSlaveFile(loc);
  if (fd < 0) {
    return absl::InternalError(
        absl::StrFormat("Open device %s failed.", absl::FormatStreamed(loc)));
  }

  auto fd_closer = LambdaCleanup([fd]() { close(fd); });

  absl::Status status;

  union i2c_smbus_data i2c_data{};
  memcpy(&i2c_data.block[1], data.data(), data.size());
  i2c_data.block[0] = data.size();

  if (SmbusIoctl(ioctl_, fd, I2C_SMBUS_WRITE, command, I2C_SMBUS_I2C_BLOCK_DATA,
                 &i2c_data) < 0) {
    status = absl::InternalError(
        absl::StrFormat("WriteBlock size of %d to device %s failed.",
                        data.size(), absl::FormatStreamed(loc)));
  }

  return status;
}

absl::Status KernelSmbusAccess::ReadBlockI2C(const SmbusLocation &loc,
                                             int command,
                                             absl::Span<unsigned char> data,
                                             size_t *len) const {
  // Linux interface only supports up to 32 bytes.
  if (data.size() > I2C_SMBUS_BLOCK_MAX) {
    return absl::InternalError(
        absl::StrFormat("Can not write %d to device %s, "
                        "Linux interface only supports up to 32 bytes.",
                        data.size(), absl::FormatStreamed(loc)));
  }

  // Open connection to i2c slave.
  int fd = OpenI2CSlaveFile(loc);
  if (fd < 0) {
    return absl::InternalError(
        absl::StrFormat("Open device %s failed.", absl::FormatStreamed(loc)));
  }

  auto fd_closer = LambdaCleanup([fd]() { close(fd); });

  // Check that driver can support I2C block reads
  int ret = CheckFunctionality(fd, I2C_FUNC_SMBUS_READ_I2C_BLOCK);
  if (ret != 0) {
    return absl::UnimplementedError(
        absl::StrFormat("Device %s does not support I2c ReadBlock.",
                        absl::FormatStreamed(loc)));
  }

  absl::Status status;
  union i2c_smbus_data i2c_data{};
  if (SmbusIoctl(ioctl_, fd, I2C_SMBUS_READ, command, I2C_SMBUS_I2C_BLOCK_DATA,
                 &i2c_data) < 0) {
    status = absl::InternalError(
        absl::StrFormat("ReadBlock size of %d from device %s failed.",
                        data.size(), absl::FormatStreamed(loc)));

  } else {
    *len = std::min(static_cast<size_t>(i2c_data.block[0]), data.size());
    memcpy(data.data(), &i2c_data.block[1], *len);
  }

  return status;
}

bool KernelSmbusAccess::SupportBlockRead(const SmbusLocation &loc) const {
  int fd = OpenI2CSlaveFile(loc);
  if (fd < 0) return false;

  auto fd_closer = LambdaCleanup([fd]() { close(fd); });
  return CheckFunctionality(fd, I2C_FUNC_SMBUS_READ_I2C_BLOCK) == 0;
}

}  // namespace ecclesia
