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

#include "ecclesia/lib/redfish/toolchain/internal/redfish_accessors.h"

#include <memory>
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "ecclesia/lib/redfish/testing/json_mockup.h"

namespace ecclesia {
namespace {

using ::testing::Eq;

TEST(BasicServerAssemblyTest, SimpleAccessorTest) {
  JsonMockupObject obj {R"(
  {
    "Assemblies": [
      {
        "@odata.id": "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/0",
        "MemberId": "0",
        "Name": "Fan 10",
        "PartNumber": "3434-149",
        "SerialNumber": "Google-Fan-10",
        "Oem": {
        }
      },
      {
        "@odata.id": "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/1",
        "MemberId": "1",
        "Name": "Fan 11",
        "PartNumber": "3434-150",
        "SerialNumber": "Google-Fan-11",
        "Oem": {
        }
      }
    ]
  })"_json};

  // Get Accessor for 'Assemblies' within Assembly resource
  auto assembly_accessor = BasicServerAssembly::Assembly {&obj};
  auto assembly_collection = assembly_accessor.Assemblies();
  ASSERT_TRUE(assembly_collection.has_value());

  // Get Accessor for first 'AssemblyData' entry
  auto iterable_assemblies = assembly_collection.value().AsIterable();
  auto assemblies_obj = (*iterable_assemblies)[0].AsObject();
  ASSERT_TRUE(assemblies_obj);

  auto assembly_data_accessor = BasicServerAssembly::AssemblyData {
    assemblies_obj.get()};

  // Get Part Number using accessor API
  std::optional<std::string> part_number = assembly_data_accessor.PartNumber();
  ASSERT_TRUE(part_number.has_value());
  EXPECT_THAT(part_number.value(), Eq("3434-149"));

  // Get Serial Number using accessor API
  std::optional<std::string> serial_number
    = assembly_data_accessor.SerialNumber();
  ASSERT_TRUE(serial_number.has_value());
  EXPECT_THAT(serial_number.value(), Eq("Google-Fan-10"));
}

}  // namespace
}  // namespace ecclesia
