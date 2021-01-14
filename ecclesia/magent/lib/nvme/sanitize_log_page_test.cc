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

#include "ecclesia/magent/lib/nvme/sanitize_log_page.h"

#include <memory>
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ecclesia/magent/lib/nvme/nvme_types.emb.h"

namespace ecclesia {

// Sanitize Block Erase log data collected from NVMe SSD, nvme-cli reports:
// -----------------------------------------------
// Sanitize Progress                      (SPROG) :  65535
// Sanitize Status                        (SSTAT) :  0x101
// Sanitize Command Dword 10 Information (SCDW10) :  0x2
// Estimated Time For Overwrite          :  4294967295 (No time period reported)
// Estimated Time For Block Erase        :  4294967295 (No time period reported)
// Estimated Time For Crypto Erase       :  4294967295 (No time period reported)
// Estimated Time For Overwrite (No-Deallocate)   :  0
// Estimated Time For Block Erase (No-Deallocate) :  0
// Estimated Time For Crypto Erase (No-Deallocate):  0
constexpr unsigned char kBlockSanitizeLogData[SanitizeLogPageFormat::
                                                  IntrinsicSizeInBytes()] = {
    /* 0x0000 */ 0xff, 0xff, 0x01, 0x01, 0x02, 0x00, 0x00, 0x00,
    /* 0x0008 */ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    /* 0x0010 */ 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
    /* 0x0018 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// Sanitize Crypto Erase log data collected from NVMe SSD, nvme-cli reports:
// -----------------------------------------------
// Sanitize Progress                      (SPROG) :  65535
// Sanitize Status                        (SSTAT) :  0x101
// Sanitize Command Dword 10 Information (SCDW10) :  0x20c
// Estimated Time For Overwrite          :  4294967295 (No time period reported)
// Estimated Time For Block Erase        :  4294967295 (No time period reported)
// Estimated Time For Crypto Erase       :  4294967295 (No time period reported)
// Estimated Time For Overwrite (No-Deallocate)   :  0
// Estimated Time For Block Erase (No-Deallocate) :  0
// Estimated Time For Crypto Erase (No-Deallocate):  0
constexpr unsigned char kCryptoSanitizeLogData[SanitizeLogPageFormat::
                                                   IntrinsicSizeInBytes()] = {
    /* 0x0000 */ 0xff, 0xff, 0x01, 0x01, 0x0c, 0x02, 0x00, 0x00,
    /* 0x0008 */ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    /* 0x0010 */ 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
    /* 0x0018 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

TEST(SanitizeLogPageTest, BlockSanitizeLogData) {
  std::unique_ptr<SanitizeLogPageInterface> sanitize(
      SanitizeLogPage::Parse(
          std::string(kBlockSanitizeLogData,
                      kBlockSanitizeLogData +
                          SanitizeLogPageFormat::IntrinsicSizeInBytes()))
          .value());

  ASSERT_TRUE(sanitize.get());

  EXPECT_EQ(0xffff, sanitize->progress());
  EXPECT_EQ(0x1, sanitize->recent_sanitize_status());
  EXPECT_EQ(0x0, sanitize->completed_overwrite_passes());
  EXPECT_EQ(0x0, sanitize->written_after_sanitize());
  EXPECT_EQ(0x2, sanitize->sanitize_action());
  EXPECT_EQ(0x0, sanitize->allow_unrestricted_sanitize_exit());
  EXPECT_EQ(0x0, sanitize->overwrite_pass_count());
  EXPECT_EQ(0x0, sanitize->overwrite_invert_pattern());
  EXPECT_EQ(0x0, sanitize->no_dealloc());

  EXPECT_EQ(0xffffffff, sanitize->estimate_block_erase_time());
  EXPECT_EQ(0xffffffff, sanitize->estimate_crypto_erase_time());
  EXPECT_EQ(0xffffffff, sanitize->estimate_overwrite_time());
}

TEST(SanitizeLogPageTest, CryptoSanitizeLogData) {
  std::unique_ptr<SanitizeLogPageInterface> sanitize(
      SanitizeLogPage::Parse(
          std::string(kCryptoSanitizeLogData,
                      kCryptoSanitizeLogData +
                          SanitizeLogPageFormat::IntrinsicSizeInBytes()))
          .value());
  ASSERT_TRUE(sanitize.get());

  EXPECT_EQ(0xffff, sanitize->progress());
  EXPECT_EQ(0x1, sanitize->recent_sanitize_status());
  EXPECT_EQ(0x0, sanitize->completed_overwrite_passes());
  EXPECT_EQ(0x0, sanitize->written_after_sanitize());
  EXPECT_EQ(0x4, sanitize->sanitize_action());
  EXPECT_EQ(0x1, sanitize->allow_unrestricted_sanitize_exit());
  EXPECT_EQ(0x0, sanitize->overwrite_pass_count());
  EXPECT_EQ(0x0, sanitize->overwrite_invert_pattern());
  EXPECT_EQ(0x1, sanitize->no_dealloc());

  EXPECT_EQ(0xffffffff, sanitize->estimate_block_erase_time());
  EXPECT_EQ(0xffffffff, sanitize->estimate_crypto_erase_time());
  EXPECT_EQ(0xffffffff, sanitize->estimate_overwrite_time());
}

TEST(SanitizeLogPageTest, SanitizeParseBufferFail) {
  auto status = SanitizeLogPage::Parse("Invalid sanitize log buffer").status();
  EXPECT_EQ(absl::StatusCode::kInternal, status.code());
  EXPECT_THAT(
      status.ToString(),
      testing::HasSubstr("The sanitize log page size must be equal to 512"));
}

}  // namespace ecclesia
