/*
 * Copyright 2022 Google LLC
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

#include "ecclesia/lib/redfish/types.h"

#include "gtest/gtest.h"

namespace ecclesia {

TEST(TypeAndVersionExtraction, Works) {
  std::string type_and_version = "Resource.v1_0.Resource";
  std::optional<ResourceTypeAndVersion> resource_type_and_version =
      GetResourceTypeAndVersionFromOdataType(type_and_version);
  ASSERT_TRUE(resource_type_and_version.has_value());
  EXPECT_EQ(resource_type_and_version->resource_type, "Resource");
  EXPECT_EQ(resource_type_and_version->version, "1_0");
}

TEST(TypeAndVersionExtraction, NoValueForPartialOdataType) {
  std::string type_and_version = "Resource.v1_0";
  std::optional<ResourceTypeAndVersion> resource_type_and_version =
      GetResourceTypeAndVersionFromOdataType(type_and_version);
  EXPECT_FALSE(resource_type_and_version.has_value());
}

TEST(TypeAndVersionExtraction, NoValueForOvercomplicatedOdataType) {
  std::string type_and_version = "Resource.v1_0.Resource.Extra.Info.123_456";
  std::optional<ResourceTypeAndVersion> resource_type_and_version =
      GetResourceTypeAndVersionFromOdataType(type_and_version);
  EXPECT_FALSE(resource_type_and_version.has_value());
}

TEST(TypeAndVersionExtraction, NoValueForEmptyInput) {
  std::string type_and_version;
  std::optional<ResourceTypeAndVersion> resource_type_and_version =
      GetResourceTypeAndVersionFromOdataType(type_and_version);
  EXPECT_FALSE(resource_type_and_version.has_value());
}

}  // namespace ecclesia
