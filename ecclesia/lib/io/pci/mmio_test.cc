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

#include "ecclesia/lib/io/pci/mmio.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>

#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ecclesia/lib/codec/endian.h"
#include "ecclesia/lib/file/path.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/io/pci/location.h"

namespace ecclesia {

namespace {

constexpr PciDbdfLocation kDefaultLocation =
    PciDbdfLocation::Make<0, 0, 0, 0>();

constexpr absl::StatusCode kOkStatus = absl::StatusCode::kOk;
constexpr absl::StatusCode kInternalError = absl::StatusCode::kInternal;

}  // namespace

TEST(MmioNoDevice, ReadFailure) {
  PciMmioRegion pci("/nonexistent", 0, kDefaultLocation);

  EXPECT_EQ(pci.Read8(0).status().code(), kInternalError);
  EXPECT_EQ(pci.Read16(0).status().code(), kInternalError);
  EXPECT_EQ(pci.Read32(0).status().code(), kInternalError);
}

TEST(MmioNoDevice, WriteFailure) {
  PciMmioRegion pci("/nonexistent", 0, kDefaultLocation);

  const uint8_t val8 = 0xef;
  const uint16_t val16 = 0xbeef;
  const uint32_t val32 = 0xdeadbeef;
  EXPECT_EQ(pci.Write8(0, val8).code(), kInternalError);
  EXPECT_EQ(pci.Write16(0, val16).code(), kInternalError);
  EXPECT_EQ(pci.Write32(0, val32).code(), kInternalError);
}

class PciMmioRegionTest : public testing::Test {
 protected:
  void SetUp() override {}

  void SetupMmconfigForBuses(int num_buses) {
    // Create a sparse file to represent PCI config space and mmap it.
    mmconfig_filename_ = JoinFilePaths(GetTestTempdirPath(), "pci_mmconfig");
    size_ = num_buses * 4096 * 32 * 8;  // 256MB of config space
    int fd = open(mmconfig_filename_.c_str(), O_CREAT | O_RDWR | O_TRUNC,
                  S_IRWXU | S_IRWXG);
    ASSERT_GE(fd, 0);
    lseek(fd, size_, SEEK_SET);
    write(fd, "\x00", 1);

    mmconfig_ = reinterpret_cast<uint8_t *>(
        mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));

    close(fd);

    EXPECT_TRUE(mmconfig_);
  }

  void TearDown() override {
    munmap(mmconfig_, size_);
    unlink(mmconfig_filename_.c_str());
  }

  static constexpr int kBaseAddr = 0x80000;

  std::string mmconfig_filename_;
  uint8_t *mmconfig_{};
  int size_{};
};

TEST_F(PciMmioRegionTest, ReadConfigDev0Success) {
  SetupMmconfigForBuses(256);
  PciMmioRegion pci(mmconfig_filename_, kBaseAddr, kDefaultLocation);

  const uint32_t expected_val = 0xdeadbeef;
  LittleEndian::Store32(expected_val, mmconfig_ + kBaseAddr);

  {
    auto maybe_val = pci.Read8(0);
    ASSERT_TRUE(maybe_val.ok()) << maybe_val.status();
    EXPECT_EQ(maybe_val.value(), 0xef);
  }
  {
    auto maybe_val = pci.Read16(0);
    ASSERT_TRUE(maybe_val.ok()) << maybe_val.status();
    EXPECT_EQ(maybe_val.value(), 0xbeef);
  }
  {
    auto maybe_val = pci.Read32(0);
    ASSERT_TRUE(maybe_val.ok()) << maybe_val.status();
    EXPECT_EQ(maybe_val.value(), 0xdeadbeef);
  }
}

TEST_F(PciMmioRegionTest, ReadConfigSuccess) {
  SetupMmconfigForBuses(256);
  const auto loc = PciDbdfLocation::Make<0, 1, 2, 3>();
  PciMmioRegion pci(mmconfig_filename_, kBaseAddr, loc);

  const uint32_t expected_val = 0xdeadbeef;
  const uint64_t pci_offset = PciMmioRegion::LocationToOffset(loc) + 0x38;
  LittleEndian::Store32(expected_val, mmconfig_ + kBaseAddr + pci_offset);

  {
    auto maybe_val = pci.Read8(0x38);
    ASSERT_TRUE(maybe_val.ok()) << maybe_val.status();
    EXPECT_EQ(maybe_val.value(), 0xef);
  }
  {
    auto maybe_val = pci.Read16(0x38);
    ASSERT_TRUE(maybe_val.ok()) << maybe_val.status();
    EXPECT_EQ(maybe_val.value(), 0xbeef);
  }
  {
    auto maybe_val = pci.Read32(0x38);
    ASSERT_TRUE(maybe_val.ok()) << maybe_val.status();
    EXPECT_EQ(maybe_val.value(), 0xdeadbeef);
  }
}

TEST_F(PciMmioRegionTest, WriteConfigSuccess) {
  SetupMmconfigForBuses(256);
  const auto loc = PciDbdfLocation::Make<0, 3, 2, 1>();
  PciMmioRegion pci(mmconfig_filename_, kBaseAddr, loc);

  // Fill memory with alternating 0's and 1's for overwrite checking
  std::fill(mmconfig_, mmconfig_ + size_, 0xaa);

  const uint64_t pci_offset = PciMmioRegion::LocationToOffset(loc) + 0x38;
  {
    EXPECT_EQ(pci.Write8(0x38, 0xef).code(), kOkStatus);

    // Read back 32 bytes even though only 8 bytes were written, to verify that
    // nothing overwrote the space beyond 8 bytes.
    uint32_t val;
    memcpy(&val, mmconfig_ + kBaseAddr + pci_offset, sizeof(val));
    EXPECT_EQ(0xaaaaaaef, val);
  }
  {
    EXPECT_EQ(pci.Write16(0x38, 0xbeef).code(), kOkStatus);

    // Read back 32 bytes even though only 8 bytes were
    uint32_t val;
    memcpy(&val, mmconfig_ + kBaseAddr + pci_offset, sizeof(val));
    EXPECT_EQ(0xaaaabeef, val);
  }
  {
    EXPECT_EQ(pci.Write32(0x38, 0xdeadbeef).code(), kOkStatus);

    uint32_t val;
    memcpy(&val, mmconfig_ + kBaseAddr + pci_offset, sizeof(val));
    EXPECT_EQ(0xdeadbeef, val);
  }
}

}  // namespace ecclesia
