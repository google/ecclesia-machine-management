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

#include "ecclesia/lib/io/smbus/smbus.h"

#include <optional>
#include <string>
#include <utility>

#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/meta/type_traits.h"
#include "absl/strings/str_format.h"

namespace ecclesia {
namespace {

TEST(SmbusBusTest, TestRangeCheck) {
  EXPECT_EQ(SmbusBus::TryMake(-1), std::nullopt);
  EXPECT_EQ(SmbusBus::TryMake(0)->value(), 0);
  EXPECT_EQ(SmbusBus::TryMake(1)->value(), 1);
  EXPECT_EQ(SmbusBus::TryMake(255)->value(), 255);
  EXPECT_EQ(SmbusBus::TryMake(256), std::nullopt);
}

TEST(SmbusAddressTest, TestRangeCheck) {
  EXPECT_EQ(SmbusAddress::TryMake(-1), std::nullopt);
  EXPECT_EQ(SmbusAddress::TryMake(0x00)->value(), 0x00);
  EXPECT_EQ(SmbusAddress::TryMake(0x01)->value(), 0x01);
  EXPECT_EQ(SmbusAddress::TryMake(0x7f)->value(), 0x7f);
  EXPECT_EQ(SmbusAddress::TryMake(0x80), std::nullopt);
}

TEST(SmbusLocationTest, TestComparator) {
  EXPECT_GE((SmbusLocation::Make<0, 0>()), (SmbusLocation::Make<0, 0>()));

  EXPECT_GE((SmbusLocation::Make<0, 1>()), (SmbusLocation::Make<0, 0>()));

  EXPECT_GE((SmbusLocation::Make<1, 0>()), (SmbusLocation::Make<0, 0>()));

  EXPECT_LT((SmbusLocation::Make<0, 0>()), (SmbusLocation::Make<1, 0>()));

  EXPECT_LT((SmbusLocation::Make<0, 0>()), (SmbusLocation::Make<0, 1>()));

  EXPECT_EQ((SmbusLocation::Make<0, 0>()), (SmbusLocation::Make<0, 0>()));
}

TEST(AccessInterfaceTest, IsHashable) {
  absl::flat_hash_map<SmbusLocation, std::string> smbus_map;

  // Push a selection of different values into the map.
  auto loc0 = SmbusLocation::Make<2, 0x4f>();
  auto loc1 = SmbusLocation::Make<0, 0x4f>();
  auto loc2 = SmbusLocation::Make<0, 0x00>();

  smbus_map[loc0] = absl::StrFormat("%s", absl::FormatStreamed(loc0));
  smbus_map[loc1] = absl::StrFormat("%s", absl::FormatStreamed(loc1));
  smbus_map[loc2] = absl::StrFormat("%s", absl::FormatStreamed(loc2));

  // All of those locations should have mapped onto different buckets.
  EXPECT_EQ(smbus_map.size(), 3);

  // Looking up a location with a different DeviceLocation object should work.
  auto iter = smbus_map.find(SmbusLocation::Make<0, 0x4f>());
  ASSERT_NE(iter, smbus_map.end());
  EXPECT_EQ(iter->first, loc1);
  EXPECT_EQ(iter->second, "0-004f");
}

TEST(SmbusLocationTest, TryMake) {
  EXPECT_EQ(SmbusLocation::TryMake(12, 0x8f), std::nullopt);
  EXPECT_EQ(SmbusLocation::TryMake(256, 0x7f), std::nullopt);
  EXPECT_EQ(SmbusLocation::TryMake(10, 0x1f),
            (SmbusLocation::Make<10, 0x1f>()));
  EXPECT_EQ(SmbusLocation::TryMake(12, 0x23),
            (SmbusLocation::Make<12, 0x23>()));
}

TEST(SmbusLocationTest, FromString) {
  EXPECT_EQ(SmbusLocation::FromString(""), std::nullopt);
  EXPECT_EQ(SmbusLocation::FromString("12"), std::nullopt);
  EXPECT_EQ(SmbusLocation::FromString("12-23-34"), std::nullopt);
  EXPECT_EQ(SmbusLocation::FromString("12-2g"), std::nullopt);
  EXPECT_EQ(SmbusLocation::FromString("a-23"), std::nullopt);
  EXPECT_EQ(SmbusLocation::FromString("12-8f"), std::nullopt);
  EXPECT_EQ(SmbusLocation::FromString("256-7f"), std::nullopt);
  EXPECT_EQ(SmbusLocation::FromString("10-1f"),
            (SmbusLocation::Make<10, 0x1f>()));
  EXPECT_EQ(SmbusLocation::FromString("12-23"),
            (SmbusLocation::Make<12, 0x23>()));
}

}  // namespace
}  // namespace ecclesia
