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

#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/sysinfo.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/types/span.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/io/ioctl.h"
#include "ecclesia/lib/io/smbus/smbus.h"
#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/logging.h"

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Return;

namespace ecclesia {
namespace {

// This allows us to mock calls to ioctl().
class MockIoctl : public IoctlInterface {
 public:
  int Call(int fd, unsigned long request, intptr_t argi) override {
    if (request == I2C_SLAVE) {
      return Slave(fd, argi);
    } else {
      FatalLog() << "unsupported ioctl() call";
    }
  }

  int Call(int fd, unsigned long request, void *argp) override {
    if (request == I2C_SMBUS) {
      auto *arg = static_cast<struct i2c_smbus_ioctl_data *>(argp);
      return Smbus(fd, arg->read_write, arg->command, arg->size, arg->data);
    } else if (request == I2C_RDWR) {
      auto *arg = static_cast<struct i2c_rdwr_ioctl_data *>(argp);
      return I2cReadWrite(fd, arg->msgs, arg->nmsgs);
    } else if (request == I2C_FUNCS) {
      auto *funcs = static_cast<uint32_t *>(argp);
      int ret = I2cFuncs(fd, funcs);
      // We need to fake the funcs, let it support
      // I2C_FUNC_SMBUS_READ_I2C_BLOCK.
      *funcs |= I2C_FUNC_SMBUS_READ_I2C_BLOCK;
      return ret;
    } else {
      FatalLog() << "unsupported ioctl() call";
    }
  }

  // ioctl(fd, I2C_SMBUS, struct i2c_smbus_ioctl_data *);
  MOCK_METHOD(int, Smbus,
              (int fd, char read_write, uint8_t command, int size,
               union i2c_smbus_data *data));

  // ioctl(fd, I2C_SLAVE, int);
  MOCK_METHOD(int, Slave, (int fd, int));

  // ioctl(fd, I2C_RDWR, i2c_rdwr_ioctl_data)
  MOCK_METHOD(int, I2cReadWrite, (int fd, struct i2c_msg *msgs, int nmsgs));

  // ioctl(fd, I2C_FUNCS, uint32_t *);
  MOCK_METHOD(int, I2cFuncs, (int fd, uint32_t *));
};

// I2C_SMBUS: Respond to I2C_SMBUS_READ with a fixed value.  Return 0.
ACTION_P(ReadSmbusData, value) {
  int size = arg3;
  union i2c_smbus_data *sm_data = arg4;
  if (sm_data) {
    if (size == I2C_SMBUS_BYTE_DATA || size == I2C_SMBUS_BYTE) {
      sm_data->byte = value;
    } else if (size == I2C_SMBUS_WORD_DATA) {
      sm_data->word = value;
    }
  }

  return 0;
}

// I2C_SMBUS: Verify an I2C_SMBUS_WRITE against a fixed value.  Return 0.
ACTION_P(WriteSmbusData, value) {
  int size = arg3;
  union i2c_smbus_data *sm_data = arg4;
  if (sm_data) {
    if (size == I2C_SMBUS_BYTE_DATA || size == I2C_SMBUS_BYTE ||
        size == I2C_SMBUS_QUICK) {
      EXPECT_EQ(static_cast<int>(value), static_cast<int>(sm_data->byte));
    } else if (size == I2C_SMBUS_WORD_DATA) {
      EXPECT_EQ(static_cast<int>(value), static_cast<int>(sm_data->word));
    }
  }

  return 0;
}

// Respond to a block read request with the given buffer and size.
ACTION_P2(ReadBlockI2CData, value, size) {
  union i2c_smbus_data *sm_data = arg4;
  if (sm_data) {
    EXPECT_LE(size, 32);
    sm_data->block[0] = size;
    for (int i = 1; i < size + 1; ++i) sm_data->block[i] = value[i - 1];
  }

  return 0;
}

// Verify the data of a block write request.
ACTION_P(WriteBlockI2CData, value) {
  union i2c_smbus_data *sm_data = arg4;
  if (sm_data) {
    EXPECT_LE(sm_data->block[0], 32);
    for (int i = 1; i < sm_data->block[0]; ++i)
      EXPECT_EQ(sm_data->block[i], value[i - 1]);
  }

  return 0;
}

class KernelSmbusAccessTest : public testing::Test {
 public:
  KernelSmbusAccessTest() : fs_(GetTestTempdirPath()) {}

 protected:
  TestFilesystem fs_;
  MockIoctl mock_ioctl_;
};

TEST_F(KernelSmbusAccessTest, TestDevFilesNotPresent) {
  std::string test_data("test_data");
  fs_.CreateDir("/dev");

  auto loc = SmbusLocation::Make<1, 0>();
  KernelSmbusAccess access(GetTestTempdirPath("dev"), &mock_ioctl_);

  EXPECT_EQ(access.SendByte(loc, 0).code(), absl::StatusCode::kInternal);
}

class KernelSmbusAccessTest1 : public testing::Test {
 protected:
  KernelSmbusAccessTest1() : fs_(GetTestTempdirPath()) {
    fs_.CreateDir("/dev");

    dev_dir_ = GetTestTempdirPath("dev");

    std::string test_data("dont_care");
    fs_.CreateFile("/dev/i2c-1", test_data);
  }

  TestFilesystem fs_;
  MockIoctl mock_ioctl_;
  std::string dev_dir_;
};

// Try to read from bus 2 (nonexistent).
TEST_F(KernelSmbusAccessTest1, NonExistentBusNumber) {
  auto loc = SmbusLocation::Make<2, 0>();

  KernelSmbusAccess access(dev_dir_, &mock_ioctl_);

  uint8_t data;

  EXPECT_EQ(access.ReceiveByte(loc, &data).code(), absl::StatusCode::kInternal);
}

TEST_F(KernelSmbusAccessTest1, SlaveFailure) {
  auto loc = SmbusLocation::Make<1, 0>();

  KernelSmbusAccess access(dev_dir_, &mock_ioctl_);

  EXPECT_CALL(mock_ioctl_, Slave(_, loc.address().value()))
      .WillOnce(Return(-1));

  EXPECT_EQ(access.SendByte(loc, 0).code(), absl::StatusCode::kInternal);
}

TEST_F(KernelSmbusAccessTest1, ProbeDevice) {
  auto loc = SmbusLocation::Make<1, 0>();
  KernelSmbusAccess access(dev_dir_, &mock_ioctl_);

  // Device found via probe.
  EXPECT_CALL(mock_ioctl_, Slave(_, loc.address().value())).WillOnce(Return(0));
  // Note: smbus quick transaction data bit is read/write parameter

  EXPECT_CALL(mock_ioctl_, Smbus(_, 0, _, I2C_SMBUS_QUICK, _))
      .WillOnce(Return(0));

  EXPECT_TRUE(access.ProbeDevice(loc).ok());

  // Device not found (error during SMBUS_QUICK).
  EXPECT_CALL(mock_ioctl_, Slave(_, loc.address().value())).WillOnce(Return(0));

  EXPECT_CALL(mock_ioctl_, Smbus(_, 0, _, I2C_SMBUS_QUICK, _))
      .WillOnce(Return(-1));
  EXPECT_EQ(access.ProbeDevice(loc).code(), absl::StatusCode::kNotFound);
}

TEST_F(KernelSmbusAccessTest1, WriteQuickSuccess) {
  auto loc = SmbusLocation::Make<1, 0>();
  KernelSmbusAccess access(dev_dir_, &mock_ioctl_);

  EXPECT_CALL(mock_ioctl_, Slave(_, loc.address().value())).WillOnce(Return(0));

  uint8_t expected_data = 1;

  // Note: smbus quick transaction data bit is read/write parameter
  EXPECT_CALL(mock_ioctl_, Smbus(_, expected_data, _, I2C_SMBUS_QUICK, _))
      .WillOnce(Return(0));
  EXPECT_TRUE(access.WriteQuick(loc, expected_data).ok());
}

TEST_F(KernelSmbusAccessTest1, WriteQuickFailUnsupportedSlave) {
  auto loc = SmbusLocation::Make<1, 0>();
  KernelSmbusAccess access(dev_dir_, &mock_ioctl_);

  EXPECT_CALL(mock_ioctl_, Slave(_, loc.address().value()))
      .WillOnce(Return(-1));

  uint8_t expected_data = 1;
  EXPECT_EQ(access.WriteQuick(loc, expected_data).code(),
            absl::StatusCode::kInternal);
}

TEST_F(KernelSmbusAccessTest1, SendByteSuccess) {
  auto loc = SmbusLocation::Make<1, 0>();
  KernelSmbusAccess access(dev_dir_, &mock_ioctl_);

  EXPECT_CALL(mock_ioctl_, Slave(_, loc.address().value())).WillOnce(Return(0));

  uint8_t expected_data = 0xea;

  EXPECT_CALL(mock_ioctl_, Smbus(_, I2C_SMBUS_WRITE, _, I2C_SMBUS_BYTE, _))
      .WillOnce(WriteSmbusData(expected_data));
  EXPECT_TRUE(access.SendByte(loc, expected_data).ok());
}

TEST_F(KernelSmbusAccessTest1, SendByteFailWriteData) {
  auto loc = SmbusLocation::Make<1, 0>();
  KernelSmbusAccess access(dev_dir_, &mock_ioctl_);

  EXPECT_CALL(mock_ioctl_, Slave(_, loc.address().value())).WillOnce(Return(0));

  uint8_t expected_data = 0xea;

  EXPECT_CALL(mock_ioctl_, Smbus(_, I2C_SMBUS_WRITE, _, I2C_SMBUS_BYTE, _))
      .WillOnce(Return(-1));
  EXPECT_EQ(access.SendByte(loc, expected_data).code(),
            absl::StatusCode::kInternal);
}

TEST_F(KernelSmbusAccessTest1, ReceiveByteSuccess) {
  auto loc = SmbusLocation::Make<1, 0>();
  KernelSmbusAccess access(dev_dir_, &mock_ioctl_);

  EXPECT_CALL(mock_ioctl_, Slave(_, loc.address().value())).WillOnce(Return(0));

  uint8_t expected_data = 0xea;

  EXPECT_CALL(mock_ioctl_, Smbus(_, I2C_SMBUS_READ, _, I2C_SMBUS_BYTE, _))
      .WillOnce(ReadSmbusData(expected_data));

  uint8_t data = 0;

  EXPECT_TRUE(access.ReceiveByte(loc, &data).ok());

  EXPECT_EQ(expected_data, data);
}

TEST_F(KernelSmbusAccessTest1, ReceiveByteFailReadData) {
  auto loc = SmbusLocation::Make<1, 0>();
  KernelSmbusAccess access(dev_dir_, &mock_ioctl_);

  EXPECT_CALL(mock_ioctl_, Slave(_, loc.address().value())).WillOnce(Return(0));

  EXPECT_CALL(mock_ioctl_, Smbus(_, I2C_SMBUS_READ, _, I2C_SMBUS_BYTE, _))
      .WillOnce(Return(-1));

  uint8_t data = 0;

  EXPECT_EQ(access.ReceiveByte(loc, &data).code(), absl::StatusCode::kInternal);
}

TEST_F(KernelSmbusAccessTest1, Write8Success) {
  auto loc = SmbusLocation::Make<1, 0>();

  int reg = 0xab;

  KernelSmbusAccess access(dev_dir_, &mock_ioctl_);

  EXPECT_CALL(mock_ioctl_, Slave(_, loc.address().value())).WillOnce(Return(0));

  uint8_t expected_data = 0xea;

  EXPECT_CALL(mock_ioctl_,
              Smbus(_, I2C_SMBUS_WRITE, reg, I2C_SMBUS_BYTE_DATA, _))
      .WillOnce(WriteSmbusData(expected_data));

  EXPECT_TRUE(access.Write8(loc, reg, expected_data).ok());
}

TEST_F(KernelSmbusAccessTest1, Write8FailWriteData) {
  auto loc = SmbusLocation::Make<1, 0>();

  int reg = 0xab;

  KernelSmbusAccess access(dev_dir_, &mock_ioctl_);

  EXPECT_CALL(mock_ioctl_, Slave(_, loc.address().value())).WillOnce(Return(0));

  uint8_t expected_data = 0xea;

  EXPECT_CALL(mock_ioctl_,
              Smbus(_, I2C_SMBUS_WRITE, reg, I2C_SMBUS_BYTE_DATA, _))
      .WillOnce(Return(-1));

  EXPECT_FALSE(access.Write8(loc, reg, expected_data).ok());
}

TEST_F(KernelSmbusAccessTest1, Write8FailUnsupportedSlave) {
  auto loc = SmbusLocation::Make<1, 0>();

  int reg = 0xab;

  KernelSmbusAccess access(dev_dir_, &mock_ioctl_);

  EXPECT_CALL(mock_ioctl_, Slave(_, loc.address().value()))
      .WillOnce(Return(-1));

  uint8_t expected_data = 0xea;

  EXPECT_FALSE(access.Write8(loc, reg, expected_data).ok());
}

TEST_F(KernelSmbusAccessTest1, Read8Success) {
  auto loc = SmbusLocation::Make<1, 0>();
  int reg = 0xab;
  KernelSmbusAccess access(dev_dir_, &mock_ioctl_);

  EXPECT_CALL(mock_ioctl_, Slave(_, loc.address().value())).WillOnce(Return(0));

  uint8_t expected_data = 0xea;

  EXPECT_CALL(mock_ioctl_,
              Smbus(_, I2C_SMBUS_READ, reg, I2C_SMBUS_BYTE_DATA, _))
      .WillOnce(ReadSmbusData(expected_data));

  uint8_t data = 0;

  EXPECT_TRUE(access.Read8(loc, reg, &data).ok());

  EXPECT_EQ(expected_data, data);
}

TEST_F(KernelSmbusAccessTest1, Read8UnsupportedSlave) {
  auto loc = SmbusLocation::Make<1, 0>();
  int reg = 0xab;
  KernelSmbusAccess access(dev_dir_, &mock_ioctl_);

  EXPECT_CALL(mock_ioctl_, Slave(_, loc.address().value()))
      .WillOnce(Return(-1));

  uint8_t data = 0;

  EXPECT_EQ(access.Read8(loc, reg, &data).code(), absl::StatusCode::kInternal);
}

TEST_F(KernelSmbusAccessTest1, Read8FailReadData) {
  auto loc = SmbusLocation::Make<1, 0>();
  int reg = 0xab;
  KernelSmbusAccess access(dev_dir_, &mock_ioctl_);

  EXPECT_CALL(mock_ioctl_, Slave(_, loc.address().value())).WillOnce(Return(0));

  EXPECT_CALL(mock_ioctl_,
              Smbus(_, I2C_SMBUS_READ, reg, I2C_SMBUS_BYTE_DATA, _))
      .WillOnce(Return(-1));

  uint8_t data = 0;

  EXPECT_EQ(access.Read8(loc, reg, &data).code(), absl::StatusCode::kInternal);
}

TEST_F(KernelSmbusAccessTest1, Write16Success) {
  auto loc = SmbusLocation::Make<1, 0>();
  int reg = 0xab;
  KernelSmbusAccess access(dev_dir_, &mock_ioctl_);

  EXPECT_CALL(mock_ioctl_, Slave(_, loc.address().value())).WillOnce(Return(0));

  uint16_t expected_data = 0xea;

  EXPECT_CALL(mock_ioctl_,
              Smbus(_, I2C_SMBUS_WRITE, reg, I2C_SMBUS_WORD_DATA, _))
      .WillOnce(WriteSmbusData(expected_data));

  EXPECT_TRUE(access.Write16(loc, reg, expected_data).ok());
}

TEST_F(KernelSmbusAccessTest1, Write16UnsupportedSlave) {
  auto loc = SmbusLocation::Make<1, 0>();
  int reg = 0xab;
  KernelSmbusAccess access(dev_dir_, &mock_ioctl_);

  EXPECT_CALL(mock_ioctl_, Slave(_, loc.address().value()))
      .WillOnce(Return(-1));

  uint16_t expected_data = 0xea;

  EXPECT_EQ(access.Write16(loc, reg, expected_data).code(),
            absl::StatusCode::kInternal);
}

TEST_F(KernelSmbusAccessTest1, Write16FailWriteData) {
  auto loc = SmbusLocation::Make<1, 0>();
  int reg = 0xab;
  KernelSmbusAccess access(dev_dir_, &mock_ioctl_);

  EXPECT_CALL(mock_ioctl_, Slave(_, loc.address().value())).WillOnce(Return(0));

  uint16_t expected_data = 0xea;

  EXPECT_CALL(mock_ioctl_,
              Smbus(_, I2C_SMBUS_WRITE, reg, I2C_SMBUS_WORD_DATA, _))
      .WillOnce(Return(-1));

  EXPECT_EQ(access.Write16(loc, reg, expected_data).code(),
            absl::StatusCode::kInternal);
}

TEST_F(KernelSmbusAccessTest1, Read16Success) {
  auto loc = SmbusLocation::Make<1, 0>();
  int reg = 0xab;
  KernelSmbusAccess access(dev_dir_, &mock_ioctl_);

  EXPECT_CALL(mock_ioctl_, Slave(_, loc.address().value())).WillOnce(Return(0));

  uint16_t expected_data = 0xabcd;

  EXPECT_CALL(mock_ioctl_,
              Smbus(_, I2C_SMBUS_READ, reg, I2C_SMBUS_WORD_DATA, _))
      .WillOnce(ReadSmbusData(expected_data));

  uint16_t data = 0;

  EXPECT_TRUE(access.Read16(loc, reg, &data).ok());

  EXPECT_EQ(expected_data, data);
}

TEST_F(KernelSmbusAccessTest1, Read16UnsupportedSlave) {
  auto loc = SmbusLocation::Make<1, 0>();
  int reg = 0xab;
  KernelSmbusAccess access(dev_dir_, &mock_ioctl_);

  EXPECT_CALL(mock_ioctl_, Slave(_, loc.address().value()))
      .WillOnce(Return(-1));

  uint16_t data = 0;

  EXPECT_EQ(access.Read16(loc, reg, &data).code(), absl::StatusCode::kInternal);
}

TEST_F(KernelSmbusAccessTest1, Read16FailReadData) {
  auto loc = SmbusLocation::Make<1, 0>();
  int reg = 0xab;
  KernelSmbusAccess access(dev_dir_, &mock_ioctl_);

  EXPECT_CALL(mock_ioctl_, Slave(_, loc.address().value())).WillOnce(Return(0));

  EXPECT_CALL(mock_ioctl_,
              Smbus(_, I2C_SMBUS_READ, reg, I2C_SMBUS_WORD_DATA, _))
      .WillOnce(Return(-1));

  uint16_t data = 0;

  EXPECT_EQ(access.Read16(loc, reg, &data).code(), absl::StatusCode::kInternal);
}

TEST_F(KernelSmbusAccessTest1, WriteBlockSuccess) {
  auto loc = SmbusLocation::Make<1, 0>();
  int reg = 0xab;

  KernelSmbusAccess access(dev_dir_, &mock_ioctl_);

  EXPECT_CALL(mock_ioctl_, Slave(_, loc.address().value())).WillOnce(Return(0));

  unsigned char expected_data[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};

  EXPECT_CALL(mock_ioctl_,
              Smbus(_, I2C_SMBUS_WRITE, reg, I2C_SMBUS_I2C_BLOCK_DATA, _))
      .WillOnce(WriteBlockI2CData(expected_data));

  EXPECT_TRUE(access
                  .WriteBlockI2C(
                      loc, reg,
                      absl::MakeConstSpan(expected_data, sizeof(expected_data)))
                  .ok());
}

TEST_F(KernelSmbusAccessTest1, WriteBlockFailDataSizeError) {
  auto loc = SmbusLocation::Make<1, 0>();
  int reg = 0xab;

  KernelSmbusAccess access(dev_dir_, &mock_ioctl_);

  std::vector<unsigned char> expected_data(128, 0);

  EXPECT_EQ(access
                .WriteBlockI2C(loc, reg,
                               absl::MakeConstSpan(expected_data.data(),
                                                   expected_data.size()))
                .code(),
            absl::StatusCode::kInternal);
}

TEST_F(KernelSmbusAccessTest1, WriteBlockFailWriteData) {
  auto loc = SmbusLocation::Make<1, 0>();
  int reg = 0xab;

  KernelSmbusAccess access(dev_dir_, &mock_ioctl_);

  EXPECT_CALL(mock_ioctl_, Slave(_, loc.address().value())).WillOnce(Return(0));

  unsigned char expected_data[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};

  EXPECT_CALL(mock_ioctl_,
              Smbus(_, I2C_SMBUS_WRITE, reg, I2C_SMBUS_I2C_BLOCK_DATA, _))
      .WillOnce(Return(-1));

  EXPECT_EQ(access
                .WriteBlockI2C(
                    loc, reg,
                    absl::MakeConstSpan(expected_data, sizeof(expected_data)))
                .code(),
            absl::StatusCode::kInternal);
}

TEST_F(KernelSmbusAccessTest1, WriteBlockFailUnsupportedSlave) {
  auto loc = SmbusLocation::Make<1, 0>();
  int reg = 0xab;

  KernelSmbusAccess access(dev_dir_, &mock_ioctl_);

  EXPECT_CALL(mock_ioctl_, Slave(_, loc.address().value()))
      .WillOnce(Return(-1));

  unsigned char expected_data[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};

  EXPECT_EQ(access
                .WriteBlockI2C(
                    loc, reg,
                    absl::MakeConstSpan(expected_data, sizeof(expected_data)))
                .code(),
            absl::StatusCode::kInternal);
}

TEST_F(KernelSmbusAccessTest1, ReadBlockSuccess) {
  auto loc = SmbusLocation::Make<1, 0>();
  int reg = 0xab;
  KernelSmbusAccess access(dev_dir_, &mock_ioctl_);

  EXPECT_CALL(mock_ioctl_, I2cFuncs(_, _)).WillOnce(Return(0));

  EXPECT_CALL(mock_ioctl_, Slave(_, loc.address().value())).WillOnce(Return(0));

  uint8_t expected_data[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};

  EXPECT_CALL(mock_ioctl_,
              Smbus(_, I2C_SMBUS_READ, reg, I2C_SMBUS_I2C_BLOCK_DATA, _))
      .WillOnce(ReadBlockI2CData(expected_data, sizeof(expected_data)));

  unsigned char data[sizeof(expected_data)] = {0};

  size_t len;

  EXPECT_TRUE(
      access.ReadBlockI2C(loc, reg, absl::MakeSpan(data, sizeof(data)), &len)
          .ok());

  EXPECT_EQ(sizeof(data), len);

  EXPECT_THAT(data, ElementsAre(0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06));
}

TEST_F(KernelSmbusAccessTest1, ReadBlockFailDataSizeError) {
  auto loc = SmbusLocation::Make<1, 0>();
  int reg = 0xab;
  KernelSmbusAccess access(dev_dir_, &mock_ioctl_);

  std::vector<unsigned char> data(128, 0);

  size_t len;

  EXPECT_EQ(access.ReadBlockI2C(loc, reg, absl::MakeSpan(data), &len).code(),
            absl::StatusCode::kInternal);
}

TEST_F(KernelSmbusAccessTest1, ReadBlockFailUnsupportedSlave) {
  auto loc = SmbusLocation::Make<1, 0>();
  int reg = 0xab;
  KernelSmbusAccess access(dev_dir_, &mock_ioctl_);

  EXPECT_CALL(mock_ioctl_, Slave(_, loc.address().value()))
      .WillOnce(Return(-1));

  uint8_t expected_data[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};

  unsigned char data[sizeof(expected_data)] = {0};

  size_t len;

  EXPECT_EQ(
      access.ReadBlockI2C(loc, reg, absl::MakeSpan(data, sizeof(data)), &len)
          .code(),
      absl::StatusCode::kInternal);
}

TEST_F(KernelSmbusAccessTest1, ReadBlockFailUnsupportedI2CBlock) {
  auto loc = SmbusLocation::Make<1, 0>();
  int reg = 0xab;
  KernelSmbusAccess access(dev_dir_, &mock_ioctl_);

  EXPECT_CALL(mock_ioctl_, I2cFuncs(_, _)).WillOnce(Return(-1));

  EXPECT_CALL(mock_ioctl_, Slave(_, loc.address().value())).WillOnce(Return(0));

  uint8_t expected_data[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};

  unsigned char data[sizeof(expected_data)] = {0};

  size_t len;

  EXPECT_EQ(
      access.ReadBlockI2C(loc, reg, absl::MakeSpan(data, sizeof(data)), &len)
          .code(),
      absl::StatusCode::kUnimplemented);
}

TEST_F(KernelSmbusAccessTest1, ReadBlockFailReadData) {
  auto loc = SmbusLocation::Make<1, 0>();
  int reg = 0xab;
  KernelSmbusAccess access(dev_dir_, &mock_ioctl_);

  EXPECT_CALL(mock_ioctl_, I2cFuncs(_, _)).WillOnce(Return(0));

  EXPECT_CALL(mock_ioctl_, Slave(_, loc.address().value())).WillOnce(Return(0));

  uint8_t expected_data[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};

  EXPECT_CALL(mock_ioctl_,
              Smbus(_, I2C_SMBUS_READ, reg, I2C_SMBUS_I2C_BLOCK_DATA, _))
      .WillOnce(Return(-1));

  unsigned char data[sizeof(expected_data)] = {0};

  size_t len;

  EXPECT_EQ(
      access.ReadBlockI2C(loc, reg, absl::MakeSpan(data, sizeof(data)), &len)
          .code(),
      absl::StatusCode::kInternal);
}

}  // namespace
}  // namespace ecclesia
