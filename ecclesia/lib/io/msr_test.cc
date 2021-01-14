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

#include "ecclesia/lib/io/msr.h"

#include <cstdint>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/testing/status.h"

namespace ecclesia {
namespace {

using ::testing::Not;

class MsrTest : public ::testing::Test {
 protected:
  MsrTest()
      : fs_(GetTestTempdirPath()), msr_(GetTestTempdirPath("dev/cpu/0/msr")) {
    fs_.CreateDir("/dev/cpu/0");

    // content: 0a 0a 0a 0a 0a 0a 0a 0a 0a 0a
    fs_.CreateFile("/dev/cpu/0/msr", "\n\n\n\n\n\n\n\n\n\n");
  }

  TestFilesystem fs_;
  Msr msr_;
};

TEST(Msr, TestMsrNotExist) {
  Msr msr_non_exist(GetTestTempdirPath("dev/cpu/400/msr"));
  EXPECT_THAT(msr_non_exist.Read(2), Not(IsOk()));

  EXPECT_THAT(msr_non_exist.Write(1, 0xDEADBEEFDEADBEEF), Not(IsOk()));
}

TEST_F(MsrTest, TestReadWriteOk) {
  EXPECT_THAT(msr_.Read(2), IsOkAndHolds(0x0a0a0a0a0a0a0a0a));

  uint64_t msr_data = 0xDEADBEEFDEADBEEF;
  EXPECT_THAT(msr_.Write(1, msr_data), IsOk());
  EXPECT_THAT(msr_.Read(1), IsOkAndHolds(msr_data));
}

TEST_F(MsrTest, TestSeekFail) { EXPECT_THAT(msr_.Read(20), Not(IsOk())); }

TEST_F(MsrTest, TestReadFail) { EXPECT_THAT(msr_.Read(5), Not(IsOk())); }

}  // namespace
}  // namespace ecclesia
