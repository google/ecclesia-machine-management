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

#include "ecclesia/lib/devpath/devpath.h"

#include <optional>
#include <ostream>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"

namespace ecclesia {
namespace {

using ::testing::Eq;

TEST(IsValidDevpathTest, ValidPaths) {
  EXPECT_TRUE(IsValidDevpath("/phys"));
  EXPECT_TRUE(IsValidDevpath("/phys/CPU0"));
  EXPECT_TRUE(IsValidDevpath("/phys/PE0"));
  EXPECT_TRUE(IsValidDevpath("/phys/USB"));
  EXPECT_TRUE(IsValidDevpath("/phys/USB/DOWNLINK"));
  EXPECT_TRUE(IsValidDevpath("/phys/PE1"));
  EXPECT_TRUE(IsValidDevpath("/phys/PE1/NET0"));
}

TEST(IsValidDevpathTest, ValidPathsWithConnector) {
  EXPECT_TRUE(IsValidDevpath("/phys:connector:CPU0"));
  EXPECT_TRUE(IsValidDevpath("/phys:connector:PE0"));
  EXPECT_TRUE(IsValidDevpath("/phys:connector:USB"));
  EXPECT_TRUE(IsValidDevpath("/phys:connector:PE1"));
  EXPECT_TRUE(IsValidDevpath("/phys:connector:M.2"));
  EXPECT_TRUE(IsValidDevpath("/phys/PE1:connector:NET0"));
}

TEST(IsValidDevpathTest, ValidPathsWithDevice) {
  EXPECT_TRUE(IsValidDevpath("/phys:device:PCH"));
  EXPECT_TRUE(IsValidDevpath("/phys/CPU0:device:die0:core0"));
  EXPECT_TRUE(IsValidDevpath("/phys:device:gpu"));
  EXPECT_TRUE(IsValidDevpath("/phys/USB/DOWNLINK:device:controller"));
  EXPECT_TRUE(IsValidDevpath("/phys:device:pci"));
  EXPECT_TRUE(IsValidDevpath("/phys:device:die0:port@3"));
  EXPECT_TRUE(IsValidDevpath("/phys:device:die@0.4:port@3"));
  EXPECT_TRUE(IsValidDevpath("/phys:device:iio:pcie_root@bridge3:port3.0"));
}

TEST(IsValidDevpathTest, InvalidPaths) {
  EXPECT_FALSE(IsValidDevpath(""));
  EXPECT_FALSE(IsValidDevpath("/phys/"));
  EXPECT_FALSE(IsValidDevpath("/virt"));
  EXPECT_FALSE(IsValidDevpath("/phys/PE 0"));
  EXPECT_FALSE(IsValidDevpath("/phys:"));
  EXPECT_FALSE(IsValidDevpath("/phys:namespace"));
  EXPECT_FALSE(IsValidDevpath("/phys::text"));
  EXPECT_FALSE(IsValidDevpath("/phys/PE0+1"));
  EXPECT_FALSE(IsValidDevpath("/phys:connector:"));
  EXPECT_FALSE(IsValidDevpath("/phys:connector:PE0+1"));
  EXPECT_FALSE(IsValidDevpath("/phys:device:"));
  EXPECT_FALSE(IsValidDevpath("/phys:device:die0:"));
  EXPECT_FALSE(IsValidDevpath("/phys:device:die*"));
}

// A simple matcher implementation for comparing a DevpathComponents struct
// against a (path, suffix namespace, suffix text) triple. This doesn't do
// anything you couldn't do matching the three values manually, but it lets you
// express this more compactly and produces nicer failure messages.
class DevpathComponentsMatcher
    : public ::testing::MatcherInterface<const DevpathComponents &> {
 public:
  DevpathComponentsMatcher(absl::string_view path, absl::string_view ns,
                           absl::string_view text)
      : path_(path), ns_(ns), text_(text) {}

  bool MatchAndExplain(const DevpathComponents &value,
                       ::testing::MatchResultListener *listener) const final {
    bool matches = true;
    if (value.path != path_) {
      *listener << "\npath=" << value.path << ", expected=" << path_;
      matches = false;
    }
    if (value.suffix_namespace != ns_) {
      *listener << "\nsuffix namespace=" << value.suffix_namespace
                << ", expected=" << ns_;
      matches = false;
    }
    if (value.suffix_text != text_) {
      *listener << "\nsuffix text=" << value.suffix_text
                << ", expected=" << text_;
      matches = false;
    }
    return matches;
  }

  void DescribeTo(std::ostream *os) const override {
    *os << "has path=" << path_ << ", namespace=" << ns_ << ", text=" << text_;
  }

  void DescribeNegationTo(std::ostream *os) const override {
    *os << "does not have path=" << path_ << ", namespace=" << ns_
        << ", text=" << text_;
  }

 private:
  absl::string_view path_;
  absl::string_view ns_;
  absl::string_view text_;
};
::testing::Matcher<const DevpathComponents &> HasDevpathComponents(
    absl::string_view path, absl::string_view ns, absl::string_view text) {
  return ::testing::MakeMatcher(new DevpathComponentsMatcher(path, ns, text));
}

TEST(GetDevpathComponents, PathOnly) {
  EXPECT_THAT(GetDevpathComponents("/phys"),
              HasDevpathComponents("/phys", "", ""));
  EXPECT_THAT(GetDevpathComponents("/phys/PE0"),
              HasDevpathComponents("/phys/PE0", "", ""));
  EXPECT_THAT(GetDevpathComponents("/phys/PE1/NET0"),
              HasDevpathComponents("/phys/PE1/NET0", "", ""));
}

TEST(GetDevpathComponents, PathsWithFullSuffix) {
  EXPECT_THAT(GetDevpathComponents("/phys:connector:CPU0"),
              HasDevpathComponents("/phys", "connector", "CPU0"));
  EXPECT_THAT(GetDevpathComponents("/phys:connector:PE0"),
              HasDevpathComponents("/phys", "connector", "PE0"));
  EXPECT_THAT(GetDevpathComponents("/phys:connector:USB"),
              HasDevpathComponents("/phys", "connector", "USB"));
  EXPECT_THAT(GetDevpathComponents("/phys:connector:PE1"),
              HasDevpathComponents("/phys", "connector", "PE1"));
  EXPECT_THAT(GetDevpathComponents("/phys/PE1:connector:NET0"),
              HasDevpathComponents("/phys/PE1", "connector", "NET0"));
  EXPECT_THAT(GetDevpathComponents("/phys:device:PCH"),
              HasDevpathComponents("/phys", "device", "PCH"));
  EXPECT_THAT(GetDevpathComponents("/phys/CPU0:device:die0:core0"),
              HasDevpathComponents("/phys/CPU0", "device", "die0:core0"));
  EXPECT_THAT(GetDevpathComponents("/phys:device:gpu"),
              HasDevpathComponents("/phys", "device", "gpu"));
  EXPECT_THAT(
      GetDevpathComponents("/phys/USB/DOWNLINK:device:controller"),
      HasDevpathComponents("/phys/USB/DOWNLINK", "device", "controller"));
  EXPECT_THAT(GetDevpathComponents("/phys:device:pci"),
              HasDevpathComponents("/phys", "device", "pci"));
}

TEST(GetDevpathPlugin, Plugins) {
  EXPECT_THAT(GetDevpathPlugin("/phys"), Eq("/phys"));
  EXPECT_THAT(GetDevpathPlugin("/phys/CPU0"), Eq("/phys/CPU0"));
  EXPECT_THAT(GetDevpathPlugin("/phys/PE0"), Eq("/phys/PE0"));
  EXPECT_THAT(GetDevpathPlugin("/phys/USB"), Eq("/phys/USB"));
  EXPECT_THAT(GetDevpathPlugin("/phys/USB/DOWNLINK"), Eq("/phys/USB/DOWNLINK"));
  EXPECT_THAT(GetDevpathPlugin("/phys/PE1"), Eq("/phys/PE1"));
  EXPECT_THAT(GetDevpathPlugin("/phys/PE1/NET0"), Eq("/phys/PE1/NET0"));
}

TEST(GetDevpathPlugin, ConnectorsAndDevices) {
  EXPECT_THAT(GetDevpathPlugin("/phys:connector:CPU0"), Eq("/phys"));
  EXPECT_THAT(GetDevpathPlugin("/phys:connector:PE0"), Eq("/phys"));
  EXPECT_THAT(GetDevpathPlugin("/phys:connector:USB"), Eq("/phys"));
  EXPECT_THAT(GetDevpathPlugin("/phys:connector:PE1"), Eq("/phys"));
  EXPECT_THAT(GetDevpathPlugin("/phys/PE1:connector:NET0"), Eq("/phys/PE1"));
  EXPECT_THAT(GetDevpathPlugin("/phys:device:PCH"), Eq("/phys"));
  EXPECT_THAT(GetDevpathPlugin("/phys/CPU0:device:die0:core0"),
              Eq("/phys/CPU0"));
  EXPECT_THAT(GetDevpathPlugin("/phys:device:gpu"), Eq("/phys"));
  EXPECT_THAT(GetDevpathPlugin("/phys/USB/DOWNLINK:device:controller"),
              Eq("/phys/USB/DOWNLINK"));
  EXPECT_THAT(GetDevpathPlugin("/phys:device:pci"), Eq("/phys"));
}

TEST(GetUpstreamDevpath, Root) {
  EXPECT_THAT(GetUpstreamDevpath("/phys"), Eq(std::nullopt));
}

TEST(GetUpstreamDevpath, Connector) {
  EXPECT_THAT(GetUpstreamDevpath("/phys:connector:PE0"), Eq("/phys"));
  EXPECT_THAT(GetUpstreamDevpath("/phys/PE0:connector:DOWNLINK"),
              Eq("/phys/PE0"));
  EXPECT_THAT(GetUpstreamDevpath("/phys/PE0/DOWNLINK:connector:SATA"),
              Eq("/phys/PE0/DOWNLINK"));
}

TEST(GetUpstreamDevpath, Device) {
  EXPECT_THAT(GetUpstreamDevpath("/phys:device:open_bmc"), Eq("/phys"));
  EXPECT_THAT(GetUpstreamDevpath("/phys/PE0:device:open_bmc"), Eq("/phys/PE0"));
  EXPECT_THAT(GetUpstreamDevpath("/phys/PE0/DOWNLINK:device:open_bmc"),
              Eq("/phys/PE0/DOWNLINK"));
}

TEST(GetUpstreamDevpath, Plugin) {
  EXPECT_THAT(GetUpstreamDevpath("/phys/PE0"), Eq("/phys:connector:PE0"));
  EXPECT_THAT(GetUpstreamDevpath("/phys/PE0/DOWNLINK"),
              Eq("/phys/PE0:connector:DOWNLINK"));
  EXPECT_THAT(GetUpstreamDevpath("/phys/PE0/DOWNLINK/XPE5"),
              Eq("/phys/PE0/DOWNLINK:connector:XPE5"));
}

TEST(MakeDevpathFromComponents, RoundTrip) {
  std::vector<absl::string_view> test_paths = {
      "/phys",
      "/phys/CPU0",
      "/phys/PE0",
      "/phys/USB",
      "/phys/USB/DOWNLINK",
      "/phys/PE1",
      "/phys/PE1/NET0",
      "/phys:connector:CPU0",
      "/phys:connector:PE0",
      "/phys:connector:USB",
      "/phys:connector:PE1",
      "/phys/PE1:connector:NET0",
      "/phys:device:PCH",
      "/phys/CPU0:device:die0:core0",
      "/phys:device:gpu",
      "/phys/USB/DOWNLINK:device:controller",
      "/phys:device:pci",
  };
  // Verify that every one of these test paths can survive a round trip through
  // MakeDevpathFromComponents.
  for (absl::string_view path : test_paths) {
    EXPECT_THAT(MakeDevpathFromComponents(GetDevpathComponents(path)),
                Eq(path));
  }
}

TEST(MakeDevpathFromComponents, TransformPath) {
  // Take a device devpath and transform the plugin path. We should be able to
  // construct a path on a different device.
  DevpathComponents base = GetDevpathComponents("/phys/PE1:device:pci:net");
  DevpathComponents transformed = base;
  transformed.path = "/phys/SUBTRAY/PE1";
  EXPECT_THAT(MakeDevpathFromComponents(transformed),
              Eq("/phys/SUBTRAY/PE1:device:pci:net"));
}

TEST(IsPluginDevpath, CorrectPluginCatogorization) {
  EXPECT_TRUE(IsPluginDevpath("/phys"));
  EXPECT_TRUE(IsPluginDevpath("/phys/PE0"));
  EXPECT_TRUE(IsPluginDevpath("/phys/DIMM20"));
  EXPECT_TRUE(IsPluginDevpath("/phys/CPU1"));
  EXPECT_TRUE(IsPluginDevpath("/phys/PE0/J14/DOWNLINK/stele_conn"));
  EXPECT_TRUE(IsPluginDevpath("/phys/PE0/J14/DOWNLINK/stele_conn/DOWNLINK"));

  EXPECT_FALSE(IsPluginDevpath("/phys/PE0/J14/DOWNLINK/CPU0:device:die0"));
  EXPECT_FALSE(IsPluginDevpath("/phys:device:gpu"));
  EXPECT_FALSE(IsPluginDevpath("/phys:connector:PE0"));
  EXPECT_FALSE(IsPluginDevpath("/phys/PE2/IO0:device:cpld"));
}

}  // namespace
}  // namespace ecclesia
