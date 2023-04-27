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

#include "ecclesia/lib/strings/natural_sort.h"

#include <algorithm>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "absl/strings/str_cat.h"

namespace ecclesia {
namespace {

TEST(StrComarisonTest, IndividualComparisons) {
  EXPECT_LT(NaturalSortCmp("DIMM", "SSD"), 0);
  EXPECT_GT(NaturalSortCmp("SSD", "DIMM"), 0);
  EXPECT_EQ(NaturalSortCmp("DIMM", "DIMM"), 0);
  EXPECT_LT(NaturalSortCmp("DIMM0", "SSD10"), 0);
  EXPECT_EQ(NaturalSortCmp("DIMM0", "DIMM0"), 0);
  EXPECT_LT(NaturalSortCmp("DIMM", "DIMM0"), 0);
  EXPECT_GT(NaturalSortCmp("DIMM0", "DIMM"), 0);
  EXPECT_LT(NaturalSortCmp("DIMM0", "DIMM1"), 0);
  EXPECT_LT(NaturalSortCmp("DIMM000", "DIMM001"), 0);
  EXPECT_LT(NaturalSortCmp("DIMM0", "DIMM001"), 0);
  EXPECT_GT(NaturalSortCmp("DIMM1", "DIMM001"), 0);
  EXPECT_LT(NaturalSortCmp("DIMM001", "DIMM1"), 0);
  EXPECT_LT(NaturalSortCmp("DIMM2", "DIMM10"), 0);
  // Test for silk screen name with letters added to number
  EXPECT_LT(NaturalSortCmp("DIMM2A", "DIMM2B"), 0);
}

TEST(StrComparisonTest, SortingItems) {
  std::vector<std::string> list_of_dimms;
  for (int i = 31; i >= 0; i--) {
    list_of_dimms.push_back(absl::StrCat("DIMM", i));
  }
  std::sort(list_of_dimms.begin(), list_of_dimms.end(), NaturalSortLessThan);

  for (int i = 0; i < 32; i++) {
    EXPECT_EQ(list_of_dimms[i], absl::StrCat("DIMM", i));
  }
}

}  // namespace
}  // namespace ecclesia
