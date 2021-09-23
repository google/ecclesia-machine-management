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

#include "ecclesia/magent/redfish/core/assembly_modifiers.h"

#include <functional>
#include <string>
#include <utility>

#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/meta/type_traits.h"
#include "absl/status/status.h"
#include "absl/strings/str_replace.h"
#include "ecclesia/lib/io/pci/location.h"
#include "ecclesia/magent/redfish/core/assembly.h"
#include "ecclesia/magent/redfish/core/redfish_keywords.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {
namespace {

constexpr PciDbdfLocation kLocation = PciDbdfLocation::Make<1, 2, 3, 4>();
constexpr char kTestAssemblyUri[] = "/redfish/v1/Systems/system/Test/0";
constexpr char kTestAssemblyName[] = "test_assembly";
constexpr char kTestComponentName[] = "test_component";

constexpr char kExpectedPcieFunctionOdataId[] =
    "/redfish/v1/Systems/system/PCIeDevices/0001:02:03/PCIeFunctions/4";

Assembly::AssemblyModifier CreateStandardPcieFunctionModifier() {
  return CreateModifierToAssociatePcieFunction(
      kLocation, kTestAssemblyUri, kTestAssemblyName, kTestComponentName);
}

Assembly::AssemblyModifier CreateStandardComponentModifier() {
  return CreateModifierToCreateComponent(kTestAssemblyUri, kTestAssemblyName,
                                         kTestComponentName);
}

std::string FlattenStylizedJsonString(const std::string &json) {
  return absl::StrReplaceAll(json, {{"\n", ""}, {"\t", ""}, {" ", ""}});
}

TEST(PcieFunctionAssemblyModifier, AssemblyMissing) {
  const std::string kStaticAssembly = R"json({})json";
  const std::string kBadUri = "/redfish/v1/not/the/right/uri";

  nlohmann::json value = nlohmann::json::parse(kStaticAssembly, nullptr, false);
  ASSERT_FALSE(value.is_discarded());

  absl::flat_hash_map<std::string, nlohmann::json> assemblies;
  assemblies.insert(std::make_pair(kBadUri, std::move(value)));
  EXPECT_FALSE(CreateStandardPcieFunctionModifier()(assemblies).ok());

  // Check that assembly is not modified
  EXPECT_EQ(FlattenStylizedJsonString(
      assemblies.find(kBadUri)->second.dump()),
            FlattenStylizedJsonString(kStaticAssembly));
}

TEST(PcieFunctionAssemblyModifier, AssemblyNameMissing) {
  const std::string kStaticAssembly = R"json(
    {
      "Assemblies":[
        {"Name":"not_the_test_assembly"}
      ]
    }
  )json";

  nlohmann::json value = nlohmann::json::parse(kStaticAssembly, nullptr, false);
  ASSERT_FALSE(value.is_discarded());

  absl::flat_hash_map<std::string, nlohmann::json> assemblies;
  assemblies.insert(std::make_pair(kTestAssemblyUri, std::move(value)));
  EXPECT_FALSE(CreateStandardPcieFunctionModifier()(assemblies).ok());

  // Check that assembly is not modified
  EXPECT_EQ(FlattenStylizedJsonString(
                (assemblies.find(kTestAssemblyUri)->second.dump())),
            FlattenStylizedJsonString(kStaticAssembly));
}

TEST(PcieFunctionAssemblyModifier, ComponentsMissing) {
  const std::string kStaticAssembly = R"json(
    {
      "Assemblies":[
        {"Name":"test_assembly"}
      ]
    }
  )json";

  nlohmann::json value = nlohmann::json::parse(kStaticAssembly, nullptr, false);
  ASSERT_FALSE(value.is_discarded());

  absl::flat_hash_map<std::string, nlohmann::json> assemblies;
  assemblies.insert(std::make_pair(kTestAssemblyUri, std::move(value)));
  EXPECT_FALSE(CreateStandardPcieFunctionModifier()(assemblies).ok());

  // Check that assembly is not modified
  EXPECT_EQ(FlattenStylizedJsonString(
                (assemblies.find(kTestAssemblyUri)->second.dump())),
            FlattenStylizedJsonString(kStaticAssembly));
}

TEST(PcieFunctionAssemblyModifier, ComponentNameMissing) {
  const std::string kStaticAssembly = R"json(
    {
      "Assemblies":[
        {
          "Name":"test_assembly",
          "Oem": {
            "Google": {
              "Components": [
                {
                  "Name":"not_the_test_component"
                }
              ]
            }
          }
        }
      ]
    }
  )json";

  nlohmann::json value = nlohmann::json::parse(kStaticAssembly, nullptr, false);
  ASSERT_FALSE(value.is_discarded());

  absl::flat_hash_map<std::string, nlohmann::json> assemblies;
  assemblies.insert(std::make_pair(kTestAssemblyUri, std::move(value)));
  EXPECT_FALSE(CreateStandardPcieFunctionModifier()(assemblies).ok());

  // Check that assembly is not modified
  EXPECT_EQ(FlattenStylizedJsonString(
                (assemblies.find(kTestAssemblyUri)->second.dump())),
            FlattenStylizedJsonString(kStaticAssembly));
}

TEST(PcieFunctionAssemblyModifier, ModifyAssembly) {
  const std::string kStaticAssembly = R"json(
    {
      "Assemblies":[
        {
          "Name":"test_assembly",
          "Oem": {
            "Google": {
              "Components": [
                {
                  "Name":"test_component"
                }
              ]
            }
          }
        }
      ]
    }
  )json";

  nlohmann::json value = nlohmann::json::parse(kStaticAssembly, nullptr, false);
  ASSERT_FALSE(value.is_discarded());

  absl::flat_hash_map<std::string, nlohmann::json> assemblies;
  assemblies.insert(std::make_pair(kTestAssemblyUri, std::move(value)));
  EXPECT_TRUE(CreateStandardPcieFunctionModifier()(assemblies).ok());

  // Check that assembly is modified
  const auto &test_associated_with =
      assemblies.find(kTestAssemblyUri)
          ->second[kAssemblies][0][kOem][kGoogle][kComponents][0]
                  [kAssociatedWith];
  ASSERT_GT(test_associated_with.size(), 0);
  ASSERT_EQ(test_associated_with[0][kOdataId].get<std::string>(),
            kExpectedPcieFunctionOdataId);
}

TEST(AddComponentModifier, AssemblyMissing) {
  const std::string kStaticAssembly = R"json({})json";
  const std::string kBadUri = "/redfish/v1/not/the/right/uri";

  nlohmann::json value = nlohmann::json::parse(kStaticAssembly, nullptr, false);
  ASSERT_FALSE(value.is_discarded());

  absl::flat_hash_map<std::string, nlohmann::json> assemblies;
  assemblies.insert(std::make_pair(kBadUri, std::move(value)));
  EXPECT_FALSE(CreateStandardComponentModifier()(assemblies).ok());

  // Check that assembly is not modified
  EXPECT_EQ(FlattenStylizedJsonString(
                assemblies.find(kBadUri)->second.dump()),
            FlattenStylizedJsonString(kStaticAssembly));
}

TEST(AddComponentModifier, AssemblyNameMissing) {
  const std::string kStaticAssembly = R"json(
    {
      "Assemblies":[
        {"Name":"not_the_test_assembly"}
      ]
    }
  )json";

  nlohmann::json value = nlohmann::json::parse(kStaticAssembly, nullptr, false);
  ASSERT_FALSE(value.is_discarded());

  absl::flat_hash_map<std::string, nlohmann::json> assemblies;
  assemblies.insert(std::make_pair(kTestAssemblyUri, std::move(value)));
  EXPECT_FALSE(CreateStandardComponentModifier()(assemblies).ok());

  // Check that assembly is not modified
  EXPECT_EQ(FlattenStylizedJsonString(
                (assemblies.find(kTestAssemblyUri)->second.dump())),
            FlattenStylizedJsonString(kStaticAssembly));
}

TEST(AddComponentModifier, ComponentsMissing) {
  const std::string kStaticAssembly = R"json(
    {
      "Assemblies":[
        {"Name":"test_assembly"},
        {"@odata.id": "/redfish/v1/Systems/system/Test/0#/Assemblies/1"}
      ]
    }
  )json";

  nlohmann::json value = nlohmann::json::parse(kStaticAssembly, nullptr, false);
  ASSERT_FALSE(value.is_discarded());

  absl::flat_hash_map<std::string, nlohmann::json> assemblies;
  assemblies.insert(std::make_pair(kTestAssemblyUri, std::move(value)));
  EXPECT_FALSE(CreateStandardComponentModifier()(assemblies).ok());

  // Check that assembly is not modified
  EXPECT_EQ(FlattenStylizedJsonString(
                (assemblies.find(kTestAssemblyUri)->second.dump())),
            FlattenStylizedJsonString(kStaticAssembly));
}

TEST(AddComponentModifier, ComponentNameMissing) {
  const std::string kStaticAssembly = R"json(
    {
      "Assemblies":[
        {
          "@odata.id": "/redfish/v1/Systems/system/Test/0#/Assemblies/0",
          "Name":"test_assembly",
          "MemberId": 0,
          "Oem": {
            "Google": {
              "Components": [
                {
                  "@odata.id": "/redfish/v1/Systems/system/Test/0#/Assemblies/0/Components/0",
                  "Name":"not_the_test_component"
                }
              ]
            }
          }
        }
      ]
    }
  )json";

  nlohmann::json value = nlohmann::json::parse(kStaticAssembly, nullptr, false);
  ASSERT_FALSE(value.is_discarded());

  absl::flat_hash_map<std::string, nlohmann::json> assemblies;
  assemblies.insert(std::make_pair(kTestAssemblyUri, std::move(value)));
  EXPECT_TRUE(CreateStandardComponentModifier()(assemblies).ok());

  // Check that assembly is not modified
  const auto &components =
      assemblies.find(kTestAssemblyUri)
          ->second[kAssemblies][0][kOem][kGoogle][kComponents];
  EXPECT_EQ(components.size(), 2);
  EXPECT_EQ(components[1][kName].get<std::string>(), kTestComponentName);
}

TEST(AddComponentModifier, ComponentNameExists) {
  const std::string kStaticAssembly = R"json(
    {
      "Assemblies":[
        {
          "@odata.id": "/redfish/v1/Systems/system/Test/0#/Assemblies/0",
          "Name":"test_assembly",
          "MemberId": 0,
          "Oem": {
            "Google": {
              "Components": [
                {
                  "@odata.id": "/redfish/v1/Systems/system/Test/0#/Assemblies/0/Components/0",
                  "Name":"test_component"
                }
              ]
            }
          }
        }
      ]
    }
  )json";

  nlohmann::json value = nlohmann::json::parse(kStaticAssembly, nullptr, false);
  ASSERT_FALSE(value.is_discarded());

  absl::flat_hash_map<std::string, nlohmann::json> assemblies;
  assemblies.insert(std::make_pair(kTestAssemblyUri, std::move(value)));
  EXPECT_TRUE(CreateStandardComponentModifier()(assemblies).ok());

  // Check that assembly is not modified
  const auto &components =
      assemblies.find(kTestAssemblyUri)
          ->second[kAssemblies][0][kOem][kGoogle][kComponents];
  EXPECT_EQ(components.size(), 1);
}

}  // namespace
}  // namespace ecclesia
