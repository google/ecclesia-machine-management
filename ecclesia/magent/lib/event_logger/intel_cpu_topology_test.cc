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

#include "ecclesia/magent/lib/event_logger/intel_cpu_topology.h"

#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ecclesia/lib/apifs/apifs.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/testing/status.h"

namespace ecclesia {
namespace {

using testing::ElementsAre;

class IntelCpuTopologyTest : public ::testing::Test {
 protected:
  IntelCpuTopologyTest()
      : fs_(GetTestTempdirPath()),
        apifs_(GetTestTempdirPath("sys/devices/system/cpu")) {
    // Create directories.
    fs_.CreateDir("/sys/devices/system/cpu/cpu0/topology");
    fs_.CreateDir("/sys/devices/system/cpu/cpu1/topology");
    fs_.CreateDir("/sys/devices/system/cpu/cpu2/topology");
    fs_.CreateDir("/sys/devices/system/cpu/cpu3/topology");
    fs_.CreateDir("/sys/devices/system/cpu/cpu4/topology");
    fs_.CreateDir("/sys/devices/system/cpu/cpu5/topology");
    fs_.CreateDir("/sys/devices/system/cpu/cpu6/topology");
    fs_.CreateDir("/sys/devices/system/cpu/cpu7/topology");

    // Create sysfs entries for CPU topology.
    fs_.CreateFile("/sys/devices/system/cpu/cpu0/topology/physical_package_id",
                   "0");
    fs_.CreateFile("/sys/devices/system/cpu/cpu1/topology/physical_package_id",
                   "0");
    fs_.CreateFile("/sys/devices/system/cpu/cpu2/topology/physical_package_id",
                   "0");
    fs_.CreateFile("/sys/devices/system/cpu/cpu3/topology/physical_package_id",
                   "0");
    fs_.CreateFile("/sys/devices/system/cpu/cpu4/topology/physical_package_id",
                   "1");
    fs_.CreateFile("/sys/devices/system/cpu/cpu5/topology/physical_package_id",
                   "1");
    fs_.CreateFile("/sys/devices/system/cpu/cpu6/topology/physical_package_id",
                   "1");
    fs_.CreateFile("/sys/devices/system/cpu/cpu7/topology/physical_package_id",
                   "1");
  }

  TestFilesystem fs_;
  ApifsDirectory apifs_;
};

TEST(IntelCpuTopologyConstructionTest, Ctor) {
  IntelCpuTopology top;
  std::vector<int> lpus = top.GetLpusForSocketId(0);
  EXPECT_FALSE(lpus.empty());
}

TEST_F(IntelCpuTopologyTest, TestParse) {
  fs_.CreateFile("/sys/devices/system/cpu/online", "aaa");
  fs_.CreateFile("/sys/devices/system/cpu/int", "1234");
  fs_.CreateFile("/sys/devices/system/cpu/double", "1.2345");
  fs_.CreateFile("/sys/devices/system/cpu/float", "1.23");
  fs_.CreateFile("/sys/devices/system/cpu/bool", "true");
  IntelCpuTopology intel_top(
      {.apifs_path = GetTestTempdirPath("sys/devices/system/cpu")});

  int int_val;
  EXPECT_FALSE(intel_top.ReadApifsFile<int>("file_none_exist", &int_val).ok());

  EXPECT_FALSE(intel_top.ReadApifsFile<int>("online", &int_val).ok());
  EXPECT_TRUE(intel_top.ReadApifsFile<int>("int", &int_val).ok());
  EXPECT_EQ(1234, int_val);

  float float_val;
  ASSERT_TRUE(intel_top.ReadApifsFile<float>("float", &float_val).ok());
  EXPECT_FLOAT_EQ(1.23, float_val);

  double double_val;
  ASSERT_TRUE(intel_top.ReadApifsFile<double>("double", &double_val).ok());
  EXPECT_DOUBLE_EQ(1.2345, double_val);

  bool bool_val;
  EXPECT_TRUE(intel_top.ReadApifsFile<bool>("bool", &bool_val).ok());
}

TEST_F(IntelCpuTopologyTest, TestCPURangeOnlineFail) {
  IntelCpuTopology intel_top(
      {.apifs_path = GetTestTempdirPath("sys/devices/system/cpu")});

  EXPECT_TRUE(absl::IsNotFound(intel_top.GetSocketIdForLpu(0).status()));
}

TEST_F(IntelCpuTopologyTest, TestCPURangeFromFail) {
  fs_.CreateFile("/sys/devices/system/cpu/online", "a-7");
  IntelCpuTopology intel_top(
      {.apifs_path = GetTestTempdirPath("sys/devices/system/cpu")});

  EXPECT_TRUE(absl::IsNotFound(intel_top.GetSocketIdForLpu(0).status()));
}

TEST_F(IntelCpuTopologyTest, TestCPURangeToFail) {
  fs_.CreateFile("/sys/devices/system/cpu/online", "0-a");
  IntelCpuTopology intel_top(
      {.apifs_path = GetTestTempdirPath("sys/devices/system/cpu")});

  EXPECT_TRUE(absl::IsNotFound(intel_top.GetSocketIdForLpu(0).status()));
}

TEST_F(IntelCpuTopologyTest, TestGetSocketIdForLpu) {
  fs_.CreateFile("/sys/devices/system/cpu/online", "0-7");

  IntelCpuTopology intel_top(
      {.apifs_path = GetTestTempdirPath("sys/devices/system/cpu")});

  for (int i = 0; i < 8; i++) {
    EXPECT_THAT(intel_top.GetSocketIdForLpu(i), IsOkAndHolds(i / 4));
  }
}

TEST_F(IntelCpuTopologyTest, TestGetLpusForSocketId) {
  fs_.CreateFile("/sys/devices/system/cpu/online", "0-7");

  IntelCpuTopology intel_top(
      {.apifs_path = GetTestTempdirPath("sys/devices/system/cpu")});

  std::vector<int> lpus = intel_top.GetLpusForSocketId(0);
  EXPECT_THAT(lpus, ElementsAre(0, 1, 2, 3));
  lpus = intel_top.GetLpusForSocketId(1);
  EXPECT_THAT(lpus, ElementsAre(4, 5, 6, 7));
}

}  // namespace

}  // namespace ecclesia
