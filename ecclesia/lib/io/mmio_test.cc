/*
 * Copyright 2021 Google LLC
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

#include "ecclesia/lib/io/mmio.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>

#include "gtest/gtest.h"
#include "absl/cleanup/cleanup.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ecclesia/lib/codec/bits.h"
#include "ecclesia/lib/codec/endian.h"
#include "ecclesia/lib/file/path.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/testing/status.h"

namespace ecclesia {

namespace {

constexpr absl::StatusCode kOkStatus = absl::StatusCode::kOk;
constexpr absl::StatusCode kInternalError = absl::StatusCode::kInternal;

}  // namespace

TEST(MmioSanity, EmptyRange) {
  auto mmio = MmioRangeFromFile::Create(AddressRange(1, 0), "/nonexistent");
  EXPECT_THAT(mmio, IsStatusInvalidArgument());
}

TEST(MmioNoDevice, FileFailure) {
  auto mmio = MmioRangeFromFile::Create(AddressRange(0, 0), "/nonexistent");
  EXPECT_THAT(mmio, IsStatusInternal());
}

class MmioAccessTest : public testing::Test {
 protected:
  ~MmioAccessTest() override {
    munmap(mmconfig_, size_);
    unlink(mmconfig_filename_.c_str());
  }

  void SetupMmconfig(int size) {
    size_ = size;
    // Create a sparse file to represent PCI config space and mmap it.
    mmconfig_filename_ = JoinFilePaths(GetTestTempdirPath(), "mmconfig");
    int fd = open(mmconfig_filename_.c_str(), O_CREAT | O_RDWR | O_TRUNC,
                  S_IRWXU | S_IRWXG);
    auto fd_closer = absl::MakeCleanup([fd]() { close(fd); });

    ASSERT_GE(fd, 0);
    lseek(fd, size_, SEEK_SET);
    write(fd, "\x00", 1);

    mmconfig_ = reinterpret_cast<uint8_t *>(
        mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
    EXPECT_TRUE(mmconfig_);
  }

  int size_;
  std::string mmconfig_filename_;
  uint8_t *mmconfig_;
};

TEST_F(MmioAccessTest, ReadSuccess) {
  SetupMmconfig(256);
  auto mmio =
      MmioRangeFromFile::Create(AddressRange(0, 256), mmconfig_filename_);
  ASSERT_THAT(mmio, IsOk());

  const uint32_t expected_val = 0xdeadbeef;
  LittleEndian::Store32(expected_val, mmconfig_);

  {
    auto maybe_val = mmio->Read8(0);
    ASSERT_TRUE(maybe_val.ok()) << maybe_val.status();
    EXPECT_EQ(maybe_val.value(), 0xef);
  }
  {
    auto maybe_val = mmio->Read16(0);
    ASSERT_TRUE(maybe_val.ok()) << maybe_val.status();
    EXPECT_EQ(maybe_val.value(), 0xbeef);
  }
  {
    auto maybe_val = mmio->Read32(0);
    ASSERT_TRUE(maybe_val.ok()) << maybe_val.status();
    EXPECT_EQ(maybe_val.value(), 0xdeadbeef);
  }
}

TEST_F(MmioAccessTest, WriteSuccess) {
  SetupMmconfig(256);
  uint64_t first_address = 16;
  auto mmio = MmioRangeFromFile::Create(AddressRange(first_address, 256),
                                        mmconfig_filename_);
  ASSERT_THAT(mmio, IsOk());

  // Fill memory with alternating 0's and 1's for overwrite checking
  std::fill(mmconfig_, mmconfig_ + size_, 0xaa);

  uint64_t offset = 0x38;
  {
    EXPECT_EQ(mmio->Write8(offset, 0xef).code(), kOkStatus);

    // Read back 32 bytes even though only 8 bytes were written, to verify that
    // nothing overwrote the space beyond 8 bytes.
    uint32_t val;
    memcpy(&val, mmconfig_ + first_address + offset, sizeof(val));
    EXPECT_EQ(0xaaaaaaef, val);
  }
  {
    EXPECT_EQ(mmio->Write16(offset, 0xbeef).code(), kOkStatus);

    // Read back 32 bytes even though only 8 bytes were
    uint32_t val;
    memcpy(&val, mmconfig_ + first_address + offset, sizeof(val));
    EXPECT_EQ(0xaaaabeef, val);
  }
  {
    EXPECT_EQ(mmio->Write32(offset, 0xdeadbeef).code(), kOkStatus);

    uint32_t val;
    memcpy(&val, mmconfig_ + first_address + offset, sizeof(val));
    EXPECT_EQ(0xdeadbeef, val);
  }
}

}  // namespace ecclesia
