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

#include "ecclesia/lib/io/pci/location.h"

#include <string>

#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/meta/type_traits.h"
#include "absl/strings/str_format.h"
#include "absl/types/optional.h"

namespace ecclesia {
namespace {

TEST(PciLocationTest, VerifyIdentityOps) {
  // domain has range [0-65535], we only test [0-8] here.
  for (int domain = 0; domain <= 0x8; ++domain) {
    for (int bus = 0; bus <= 0xff; ++bus) {
      for (int device = 0; device <= 0x1f; ++device) {
        for (int function = 0; function <= 7; ++function) {
          auto maybe_loc =
              PciDbdfLocation::TryMake(domain, bus, device, function);
          ASSERT_TRUE(maybe_loc.has_value());
          std::string loc_str =
              absl::StrFormat("%s", absl::FormatStreamed(maybe_loc.value()));
          ASSERT_EQ(12, loc_str.length());

          auto loc_from_str = PciDbdfLocation::FromString(loc_str);
          ASSERT_TRUE(loc_from_str.has_value());

          EXPECT_EQ(maybe_loc.value(), loc_from_str.value());
        }
      }
    }
  }
}

TEST(PciLocationTest, PciLocationFailTests) {
  // from empty string
  auto loc = PciDbdfLocation::FromString("");
  EXPECT_EQ(loc, absl::nullopt);

  // short form not parsable.
  loc = PciDbdfLocation::FromString("02:01.0");
  EXPECT_EQ(loc, absl::nullopt);

  // Strict format: 0001:02:03.4, 0 padding is enforced.
  loc = PciDbdfLocation::FromString("0001:2:3.4");
  EXPECT_EQ(loc, absl::nullopt);

  // Invalid character.
  loc = PciDbdfLocation::FromString("0001:02:0g.7");
  EXPECT_EQ(loc, absl::nullopt);

  // Function number out of range.
  loc = PciDbdfLocation::FromString("0001:02:03.9");
  EXPECT_EQ(loc, absl::nullopt);

  // TryMake: Function number out of range.
  loc = PciDbdfLocation::TryMake(1, 2, 3, 9);
  EXPECT_EQ(loc, absl::nullopt);
}

TEST(PciLocationTest, TestComparator) {
  EXPECT_GE((PciDbdfLocation::Make<0, 0, 0, 0>()),
            (PciDbdfLocation::Make<0, 0, 0, 0>()));

  EXPECT_GE((PciDbdfLocation::Make<0, 1, 2, 3>()),
            (PciDbdfLocation::Make<0, 0, 0, 0>()));

  EXPECT_GE((PciDbdfLocation::Make<1, 0, 0, 0>()),
            (PciDbdfLocation::Make<0, 0xff, 0x1f, 0x7>()));

  EXPECT_LT((PciDbdfLocation::Make<0, 0xff, 0x1f, 0x7>()),
            (PciDbdfLocation::Make<1, 0, 0, 0>()));

  EXPECT_LT((PciDbdfLocation::Make<0, 0, 0, 0>()),
            (PciDbdfLocation::Make<0, 1, 2, 3>()));

  EXPECT_EQ((PciDbdfLocation::Make<2, 4, 5, 6>()),
            (PciDbdfLocation::Make<2, 4, 5, 6>()));
}

TEST(PciLocationTest, IsHashable) {
  absl::flat_hash_map<PciDbdfLocation, std::string> pci_map;

  // Push different values into the map.
  auto loc0 = PciDbdfLocation::Make<1, 2, 3, 4>();
  auto loc1 = PciDbdfLocation::Make<0, 2, 3, 4>();
  auto loc2 = PciDbdfLocation::Make<1, 0, 3, 4>();
  auto loc3 = PciDbdfLocation::Make<1, 2, 0, 4>();
  auto loc4 = PciDbdfLocation::Make<1, 2, 3, 0>();

  pci_map[loc0] = absl::StrFormat("%s", absl::FormatStreamed(loc0));
  pci_map[loc1] = absl::StrFormat("%s", absl::FormatStreamed(loc1));
  pci_map[loc2] = absl::StrFormat("%s", absl::FormatStreamed(loc2));
  pci_map[loc3] = absl::StrFormat("%s", absl::FormatStreamed(loc3));
  pci_map[loc4] = absl::StrFormat("%s", absl::FormatStreamed(loc4));

  EXPECT_EQ(pci_map.size(), 5);

  auto iter = pci_map.find(PciDbdfLocation::Make<1, 0, 3, 4>());
  ASSERT_NE(iter, pci_map.end());
  EXPECT_EQ(iter->first, loc2);
  EXPECT_EQ(iter->second, "0001:00:03.4");
}

TEST(PciLocationTest, ToCorrectString) {
  auto loc0 = PciDbdfLocation::Make<1, 2, 3, 4>();
  auto loc1 = PciDbdfLocation::Make<0, 0xa, 0xb, 7>();
  // The max possible numbers for domain:bus:device.function value
  auto loc2 = PciDbdfLocation::Make<0xffff, 0xff, 0x1f, 7>();
  EXPECT_EQ(loc0.ToString(), "0001:02:03.4");
  EXPECT_EQ(loc1.ToString(), "0000:0a:0b.7");
  EXPECT_EQ(loc2.ToString(), "ffff:ff:1f.7");
}

TEST(PciDeviceLocationTest, VerifyIdentityOps) {
  // domain has range [0-65535], we only test [0-8] here.
  for (int domain = 0; domain <= 0x8; ++domain) {
    for (int bus = 0; bus <= 0xff; ++bus) {
      for (int device = 0; device <= 0x1f; ++device) {
        auto maybe_loc = PciDbdLocation::TryMake(domain, bus, device);
        ASSERT_TRUE(maybe_loc.has_value());
        std::string loc_str =
            absl::StrFormat("%s", absl::FormatStreamed(maybe_loc.value()));
        ASSERT_EQ(10, loc_str.length());

        auto loc_from_str = PciDbdLocation::FromString(loc_str);
        ASSERT_TRUE(loc_from_str.has_value());

        EXPECT_EQ(maybe_loc.value(), loc_from_str.value());
      }
    }
  }
}

TEST(PciDeviceLocationTest, TestComparator) {
  auto pci1_location = PciDbdfLocation::Make<1, 2, 3, 4>();
  auto pci2_location = PciDbdfLocation::Make<1, 2, 3, 5>();
  auto pci3_location = PciDbdfLocation::Make<1, 2, 4, 4>();

  PciDbdLocation pci_dbd1(pci1_location);
  PciDbdLocation pci_dbd2(pci2_location);
  PciDbdLocation pci_dbd3(pci3_location);

  EXPECT_EQ(pci_dbd1, (PciDbdLocation::Make<1, 2, 3>()));
  EXPECT_EQ(pci_dbd2, (PciDbdLocation::Make<1, 2, 3>()));
  EXPECT_EQ(pci_dbd3, (PciDbdLocation::Make<1, 2, 4>()));

  EXPECT_EQ(pci_dbd1, pci_dbd2);
  EXPECT_NE(pci_dbd1, pci_dbd3);

  EXPECT_LE(pci_dbd1, pci_dbd1);
  EXPECT_LE(pci_dbd1, pci_dbd2);
  EXPECT_LE(pci_dbd1, pci_dbd3);
  EXPECT_LT(pci_dbd1, pci_dbd3);
  EXPECT_LE(pci_dbd2, pci_dbd1);
  EXPECT_LE(pci_dbd2, pci_dbd2);
  EXPECT_LE(pci_dbd2, pci_dbd3);
  EXPECT_LE(pci_dbd3, pci_dbd3);

  EXPECT_GE(pci_dbd1, pci_dbd1);
  EXPECT_GE(pci_dbd1, pci_dbd2);
  EXPECT_GE(pci_dbd2, pci_dbd1);
  EXPECT_GE(pci_dbd2, pci_dbd2);
  EXPECT_GE(pci_dbd3, pci_dbd1);
  EXPECT_GT(pci_dbd3, pci_dbd1);
  EXPECT_GE(pci_dbd3, pci_dbd2);
  EXPECT_GT(pci_dbd3, pci_dbd2);
  EXPECT_GE(pci_dbd3, pci_dbd3);
}

TEST(PciDeviceLocationTest, IsHashable) {
  absl::flat_hash_map<PciDbdLocation, std::string> pci_map;
  auto pci1_location = PciDbdfLocation::Make<1, 2, 3, 4>();
  auto pci2_location = PciDbdfLocation::Make<1, 2, 3, 5>();
  auto pci3_location = PciDbdfLocation::Make<1, 2, 4, 4>();
  auto pci4_location = PciDbdfLocation::Make<1, 2, 5, 6>();

  PciDbdLocation pci_dbd1(pci1_location);
  PciDbdLocation pci_dbd2(pci2_location);
  PciDbdLocation pci_dbd3(pci3_location);
  PciDbdLocation pci_dbd4(pci4_location);

  // 0001:02:03:4 and 0001:02:03:5 have the same key. The later one will
  // override the previous one.
  pci_map[pci_dbd1] = pci1_location.ToString();
  pci_map[pci_dbd2] = pci2_location.ToString();
  pci_map[pci_dbd3] = pci3_location.ToString();
  pci_map[pci_dbd4] = pci4_location.ToString();

  // 0001:02:03:4 and 0001:02:03:5 collides into a single entry.
  EXPECT_EQ(pci_map.size(), 3);

  auto iter = pci_map.find(pci_dbd1);
  ASSERT_NE(iter, pci_map.end());
  EXPECT_EQ(iter->first, pci_dbd1);
  EXPECT_EQ(iter->first.ToString(), "0001:02:03");
  EXPECT_EQ(iter->second, "0001:02:03.5");
}

TEST(PciDeviceLocationTest, ToCorrectString) {
  auto dbd0 = PciDbdLocation::Make<1, 2, 3>();
  auto dbd1 = PciDbdLocation::Make<0, 0xa, 0xb>();
  // The max possible numbers for domain:bus:device value
  auto dbd2 = PciDbdLocation::Make<0xffff, 0xff, 0x1f>();
  EXPECT_EQ(dbd0.ToString(), "0001:02:03");
  EXPECT_EQ(dbd1.ToString(), "0000:0a:0b");
  EXPECT_EQ(dbd2.ToString(), "ffff:ff:1f");
}

}  // namespace
}  // namespace ecclesia
