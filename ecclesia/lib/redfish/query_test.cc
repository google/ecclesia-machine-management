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

#include "ecclesia/lib/redfish/query.h"

#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "ecclesia/lib/redfish/property_definitions.h"
#include "ecclesia/lib/redfish/testing/fake_redfish_server.h"

namespace ecclesia {
namespace {
using ::testing::UnorderedElementsAre;

// Root
TEST(GetRedPath, RootRedPath) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetNodeValue<PropertyOdataId>();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre("/redfish/v1"));
}

// ********************************[*]******************************************
TEST(GetRedPath, GetObjectAllSystem) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Systems[*]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre("/redfish/v1/Systems/system"));
}
TEST(GetRedPath, GetObjectAllMemory) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Systems[*]/Memory[*]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids,
              UnorderedElementsAre("/redfish/v1/Systems/system/Memory/0",
                                   "/redfish/v1/Systems/system/Memory/1",
                                   "/redfish/v1/Systems/system/Memory/2",
                                   "/redfish/v1/Systems/system/Memory/3",
                                   "/redfish/v1/Systems/system/Memory/4",
                                   "/redfish/v1/Systems/system/Memory/5",
                                   "/redfish/v1/Systems/system/Memory/6",
                                   "/redfish/v1/Systems/system/Memory/7",
                                   "/redfish/v1/Systems/system/Memory/8",
                                   "/redfish/v1/Systems/system/Memory/9",
                                   "/redfish/v1/Systems/system/Memory/10",
                                   "/redfish/v1/Systems/system/Memory/11",
                                   "/redfish/v1/Systems/system/Memory/12",
                                   "/redfish/v1/Systems/system/Memory/13",
                                   "/redfish/v1/Systems/system/Memory/14",
                                   "/redfish/v1/Systems/system/Memory/15",
                                   "/redfish/v1/Systems/system/Memory/16",
                                   "/redfish/v1/Systems/system/Memory/17",
                                   "/redfish/v1/Systems/system/Memory/18",
                                   "/redfish/v1/Systems/system/Memory/19",
                                   "/redfish/v1/Systems/system/Memory/20",
                                   "/redfish/v1/Systems/system/Memory/21",
                                   "/redfish/v1/Systems/system/Memory/22",
                                   "/redfish/v1/Systems/system/Memory/23"));
}
TEST(GetRedPath, GetNoDelimiter) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Systems[*]/Memory[*]/Assembly";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre(
                       "/redfish/v1/Systems/system/Memory/0/Assembly",
                       "/redfish/v1/Systems/system/Memory/1/Assembly",
                       "/redfish/v1/Systems/system/Memory/2/Assembly",
                       "/redfish/v1/Systems/system/Memory/3/Assembly",
                       "/redfish/v1/Systems/system/Memory/4/Assembly",
                       "/redfish/v1/Systems/system/Memory/5/Assembly",
                       "/redfish/v1/Systems/system/Memory/6/Assembly",
                       "/redfish/v1/Systems/system/Memory/7/Assembly",
                       "/redfish/v1/Systems/system/Memory/8/Assembly",
                       "/redfish/v1/Systems/system/Memory/9/Assembly",
                       "/redfish/v1/Systems/system/Memory/10/Assembly",
                       "/redfish/v1/Systems/system/Memory/11/Assembly",
                       "/redfish/v1/Systems/system/Memory/12/Assembly",
                       "/redfish/v1/Systems/system/Memory/13/Assembly",
                       "/redfish/v1/Systems/system/Memory/14/Assembly",
                       "/redfish/v1/Systems/system/Memory/15/Assembly",
                       "/redfish/v1/Systems/system/Memory/16/Assembly",
                       "/redfish/v1/Systems/system/Memory/17/Assembly",
                       "/redfish/v1/Systems/system/Memory/18/Assembly",
                       "/redfish/v1/Systems/system/Memory/19/Assembly",
                       "/redfish/v1/Systems/system/Memory/20/Assembly",
                       "/redfish/v1/Systems/system/Memory/21/Assembly",
                       "/redfish/v1/Systems/system/Memory/22/Assembly",
                       "/redfish/v1/Systems/system/Memory/23/Assembly"));
}
TEST(GetRedPath, GetArrayAllMembers) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Chassis[*]/Thermal/Temperatures[*]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids,
              UnorderedElementsAre(
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/0",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/1",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/2",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/3",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/4",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/5",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/6",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/7",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/8",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/9",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/10",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/11",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/12",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/13",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/14",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/15",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/16",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/17",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/18",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/19",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/20",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/21",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/22",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/23"));
}
TEST(GetRedPath, WrongWordRedPath) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Syste[*]/Memory[*]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST(GetRedPath, SpaceAsRedPath) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "  ";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST(GetRedPath, WrongCharacterRedPath) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/System[*]/z~023=;";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST(GetRedPath, EmptyRedPath) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath;
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetNodeValue<PropertyOdataId>();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST(GetRedPath, RedPathWithEmptyFilter) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Syste[*]/Memory[]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre());
}

// *****************************[Index]*****************************************
TEST(GetRedPath, GetObjectByIndex) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Systems[*]/Memory[3]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(*std::move(value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre("/redfish/v1/Systems/system/Memory/3"));
}
TEST(GetRedPath, GetArrayByIndex) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Chassis[*]/Thermal/Temperatures[5]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre(
                       "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/5"));
}
TEST(GetRedPath, GetObjectByNotExistIndex) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Systems[*]/Memory[100]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(*std::move(value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST(GetRedPath, GetObjectByNegativeIndex) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Systems[*]/Memory[-1]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(*std::move(value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre());
}

// *****************************[last()]****************************************
TEST(GetRedPath, GetLastObject) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Systems[*]/Memory[last()]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids,
              UnorderedElementsAre("/redfish/v1/Systems/system/Memory/23"));
}
TEST(GetRedPath, GetLastArray) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath =
      "/Chassis[*]/Thermal/Temperatures[last()]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids,
              UnorderedElementsAre(
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/23"));
}
TEST(GetRedPath, PropertyDoNotHaveMembersOrArrayToGetLast) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Chassis[*]/Thermal[last()]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre());
}

// ***************************[nodename]****************************************
TEST(GetRedPath, GetObjectByNodename) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath =
      "/Systems[*]/Memory[SerialNumber]/Assembly";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre(
                       "/redfish/v1/Systems/system/Memory/0/Assembly",
                       "/redfish/v1/Systems/system/Memory/1/Assembly",
                       "/redfish/v1/Systems/system/Memory/2/Assembly",
                       "/redfish/v1/Systems/system/Memory/3/Assembly",
                       "/redfish/v1/Systems/system/Memory/4/Assembly",
                       "/redfish/v1/Systems/system/Memory/5/Assembly",
                       "/redfish/v1/Systems/system/Memory/6/Assembly",
                       "/redfish/v1/Systems/system/Memory/7/Assembly",
                       "/redfish/v1/Systems/system/Memory/8/Assembly",
                       "/redfish/v1/Systems/system/Memory/9/Assembly",
                       "/redfish/v1/Systems/system/Memory/10/Assembly",
                       "/redfish/v1/Systems/system/Memory/11/Assembly",
                       "/redfish/v1/Systems/system/Memory/12/Assembly",
                       "/redfish/v1/Systems/system/Memory/13/Assembly",
                       "/redfish/v1/Systems/system/Memory/14/Assembly",
                       "/redfish/v1/Systems/system/Memory/15/Assembly",
                       "/redfish/v1/Systems/system/Memory/16/Assembly",
                       "/redfish/v1/Systems/system/Memory/17/Assembly",
                       "/redfish/v1/Systems/system/Memory/18/Assembly",
                       "/redfish/v1/Systems/system/Memory/19/Assembly",
                       "/redfish/v1/Systems/system/Memory/20/Assembly",
                       "/redfish/v1/Systems/system/Memory/21/Assembly",
                       "/redfish/v1/Systems/system/Memory/22/Assembly",
                       "/redfish/v1/Systems/system/Memory/23/Assembly"));
}
TEST(GetRedPath, GetArrayByNodename) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath =
      "/Systems[*]/Processors[*]/Assembly/Assemblies[Name]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(
      ids,
      UnorderedElementsAre(
          "/redfish/v1/Systems/system/Processors/0/Assembly#/Assemblies/0",
          "/redfish/v1/Systems/system/Processors/1/Assembly#/Assemblies/0"));
}
TEST(GetRedPath, GetArrayByNodenameWithSpecialCharacter) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath =
      "/Systems[*]/Processors[*]/Assembly/Assemblies[@odata.id]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(
      ids,
      UnorderedElementsAre(
          "/redfish/v1/Systems/system/Processors/0/Assembly#/Assemblies/0",
          "/redfish/v1/Systems/system/Processors/1/Assembly#/Assemblies/0"));
}
TEST(GetRedPath, GetObjectByNodenameWithNumValue) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Systems[*]/Processors[MaxSpeedMHz]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids,
              UnorderedElementsAre("/redfish/v1/Systems/system/Processors/0",
                                   "/redfish/v1/Systems/system/Processors/1"));
}
TEST(GetRedPath, GetUnexistNodename) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath =
      "/Systems[*]/Processors[*]/Assembly/Assemblies[Test]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre());
}

// ***************************[node=value]**************************************
TEST(GetRedPath, GetObjectByNodenameAndValue) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath =
      "/Systems[*]/Memory[Name=DIMM0]/Assembly";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre(
                       "/redfish/v1/Systems/system/Memory/0/Assembly"));
}
TEST(GetRedPath, GetObjectMemberByNodename) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Systems[Id=system]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre("/redfish/v1/Systems/system"));
}
TEST(GetRedPath, GetObjectByNodenameAndNumValue) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath =
      "/Systems[*]/Processors[MaxSpeedMHz=4000]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids,
              UnorderedElementsAre("/redfish/v1/Systems/system/Processors/0",
                                   "/redfish/v1/Systems/system/Processors/1"));
}
TEST(GetRedPath, GetObjectByNodenameAndNumValueDoNotExist) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath =
      "/Systems[*]/Processors[MaxSpeedMHz=4001]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids,
              UnorderedElementsAre());
}
TEST(GetRedPath, GetArrayByNodenameAndValue) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath =
      "/Chassis[*]/Thermal/Temperatures[Name=dimm0]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre(
                       "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/0"));
}
TEST(GetRedPath, GetObjectByNodenameAndWrongValue) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath =
      "/Systems[*]/Memory[Name=DIM]/Assembly";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST(GetRedPath, GetObjectByWrongNodenameAndValue) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath =
      "/Systems[*]/Memory[id=DIMM0]/Assembly";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST(GetRedPath, GetObjectByEmptyNodenameAndValue) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Systems[*]/Memory[=DIMM0]/Assembly";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST(GetRedPath, GetObjectByNodenameAndEmptyValue) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Systems[*]/Memory[Id=]/Assembly";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST(GetRedPath, GetObjectByEmptyNodenameAndEmptyValue) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Systems[*]/Memory[=]/Assembly";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre());
}

// ****************************[node!=value]************************************
TEST(GetRedPath, GetObjectNameNotEqualValue) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Systems[*]/Memory[Id!=DIMM0]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids,
              UnorderedElementsAre("/redfish/v1/Systems/system/Memory/1",
                                   "/redfish/v1/Systems/system/Memory/2",
                                   "/redfish/v1/Systems/system/Memory/3",
                                   "/redfish/v1/Systems/system/Memory/4",
                                   "/redfish/v1/Systems/system/Memory/5",
                                   "/redfish/v1/Systems/system/Memory/6",
                                   "/redfish/v1/Systems/system/Memory/7",
                                   "/redfish/v1/Systems/system/Memory/8",
                                   "/redfish/v1/Systems/system/Memory/9",
                                   "/redfish/v1/Systems/system/Memory/10",
                                   "/redfish/v1/Systems/system/Memory/11",
                                   "/redfish/v1/Systems/system/Memory/12",
                                   "/redfish/v1/Systems/system/Memory/13",
                                   "/redfish/v1/Systems/system/Memory/14",
                                   "/redfish/v1/Systems/system/Memory/15",
                                   "/redfish/v1/Systems/system/Memory/16",
                                   "/redfish/v1/Systems/system/Memory/17",
                                   "/redfish/v1/Systems/system/Memory/18",
                                   "/redfish/v1/Systems/system/Memory/19",
                                   "/redfish/v1/Systems/system/Memory/20",
                                   "/redfish/v1/Systems/system/Memory/21",
                                   "/redfish/v1/Systems/system/Memory/22",
                                   "/redfish/v1/Systems/system/Memory/23"));
}
TEST(GetRedPath, GetArrayNameNotEqualValue) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath =
      "/Chassis[*]/Thermal/Temperatures[Name!=dimm0]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids,
              UnorderedElementsAre(
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/1",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/2",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/3",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/4",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/5",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/6",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/7",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/8",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/9",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/10",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/11",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/12",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/13",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/14",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/15",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/16",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/17",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/18",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/19",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/20",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/21",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/22",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/23"));
}
TEST(GetRedPath, GetArrayNameNotEqualNumberValue) {
  FakeRedfishServer server("indus_hmb_cn_playground/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath =
      "/Chassis[*]/Thermal/Temperatures[ReadingCelsius!=45]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre(
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/12",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/13",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/14",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/15",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/16",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/17",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/18",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/19",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/20",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/21",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/22",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/23"));
}
TEST(GetRedPath, GetObjectByNodenameDoNotExisttNotEqualValue) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath =
      "/Systems[*]/Processors[Reading!=#Processor.v1_7_1.Processor]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST(GetRedPath, GetObjectWithWrongNameNotEqualValue) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Systems[*]/Memory[id!=DIMM0]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST(GetRedPath, GetObjectWithEmptyNameNotEqualValue) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Systems[*]/Memory[!=DIMM0]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST(GetRedPath, GetObjectWithNameNotEqualEmptyValue) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Systems[*]/Memory[Id!=]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids,
              UnorderedElementsAre("/redfish/v1/Systems/system/Memory/0",
                                   "/redfish/v1/Systems/system/Memory/1",
                                   "/redfish/v1/Systems/system/Memory/2",
                                   "/redfish/v1/Systems/system/Memory/3",
                                   "/redfish/v1/Systems/system/Memory/4",
                                   "/redfish/v1/Systems/system/Memory/5",
                                   "/redfish/v1/Systems/system/Memory/6",
                                   "/redfish/v1/Systems/system/Memory/7",
                                   "/redfish/v1/Systems/system/Memory/8",
                                   "/redfish/v1/Systems/system/Memory/9",
                                   "/redfish/v1/Systems/system/Memory/10",
                                   "/redfish/v1/Systems/system/Memory/11",
                                   "/redfish/v1/Systems/system/Memory/12",
                                   "/redfish/v1/Systems/system/Memory/13",
                                   "/redfish/v1/Systems/system/Memory/14",
                                   "/redfish/v1/Systems/system/Memory/15",
                                   "/redfish/v1/Systems/system/Memory/16",
                                   "/redfish/v1/Systems/system/Memory/17",
                                   "/redfish/v1/Systems/system/Memory/18",
                                   "/redfish/v1/Systems/system/Memory/19",
                                   "/redfish/v1/Systems/system/Memory/20",
                                   "/redfish/v1/Systems/system/Memory/21",
                                   "/redfish/v1/Systems/system/Memory/22",
                                   "/redfish/v1/Systems/system/Memory/23"));
}
TEST(GetRedPath, GetObjectWithEmptyNameEqualEmptyValue) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Systems[*]/Memory[!=]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre());
}

// **************************[node>=value]**************************************
TEST(GetRedPath, GetObjectNodeValueGreaterEqualThanDoubleValue) {
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath =
      "/Chassis[*]/Sensors[Reading>=16115.0]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre(
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan0_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan1_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan2_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan3_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan4_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan5_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan6_rpm"));
}
TEST(GetRedPath, GetObjectNodeValueGreaterEqualThanIntValue) {
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Chassis[*]/Sensors[Reading>=16115]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre(
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan0_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan1_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan2_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan3_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan4_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan5_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan6_rpm"));
}
TEST(GetRedPath, GetArrayNodeValueGreaterEqualThanDoubleValue) {
  FakeRedfishServer server("indus_hmb_cn_playground/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath =
      "/Chassis[*]/Thermal/Temperatures[ReadingCelsius>=50.0]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre(
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/12",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/13",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/14",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/15",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/16",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/17",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/18",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/19",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/20",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/21",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/22",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/23"));
}
TEST(GetRedPath, GetArrayNodeValueGreaterEqualThanIntValue) {
  FakeRedfishServer server("indus_hmb_cn_playground/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath =
      "/Chassis[*]/Thermal/Temperatures[ReadingCelsius>=50]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre(
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/12",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/13",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/14",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/15",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/16",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/17",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/18",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/19",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/20",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/21",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/22",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/23"));
}
TEST(GetRedPath, GetObjectStringNodeValueGreaterEqual) {
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath =
      "/Chassis[*]/Sensors[ReadingType>=16115.0]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre());
}

TEST(GetRedPath, GetArrayWrongNodeGreaterEqualThanIntValue) {
  FakeRedfishServer server("indus_hmb_cn_playground/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath =
      "/Chassis[*]/Thermal/Temperatures[readingCelsius>=50]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST(GetRedPath, GetArrayEmptyNodeGreaterEqualThanIntValue) {
  FakeRedfishServer server("indus_hmb_cn_playground/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath =
      "/Chassis[*]/Thermal/Temperatures[>=50]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST(GetRedPath, GetArrayNodeGreaterEuqalThanEmptyValue) {
  FakeRedfishServer server("indus_hmb_cn_playground/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath =
      "/Chassis[*]/Thermal/Temperatures[ReadingCelsius>=]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST(GetRedPath, GetArrayEmptyNodeGreaterEqualThanEmptyValue) {
  FakeRedfishServer server("indus_hmb_cn_playground/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Chassis[*]/Thermal/Temperatures[>=]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre());
}

// ************************* [node<=value]**************************************
TEST(GetRedPath, GetObjectNodeValueLessEqualThanDoubleValue) {
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Chassis[*]/Sensors[Reading<=60.0]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre(
                       "/redfish/v1/Chassis/chassis/Sensors/indus_eat_temp",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_latm_temp",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_cpu0_pwmon",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_cpu1_pwmon",
                       "/redfish/v1/Chassis/chassis/Sensors/i_cpu0_t",
                       "/redfish/v1/Chassis/chassis/Sensors/i_cpu1_t"));
}
TEST(GetRedPath, GetObjectNodeValueLessEqualThanIntValue) {
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Chassis[*]/Sensors[Reading<=60]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre(
                       "/redfish/v1/Chassis/chassis/Sensors/indus_eat_temp",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_latm_temp",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_cpu0_pwmon",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_cpu1_pwmon",
                       "/redfish/v1/Chassis/chassis/Sensors/i_cpu0_t",
                       "/redfish/v1/Chassis/chassis/Sensors/i_cpu1_t"));
}
TEST(GetRedPath, GetArrayNodeValueLessEqualThanDoubleValue) {
  FakeRedfishServer server("indus_hmb_cn_playground/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath =
      "/Chassis[*]/Thermal/Temperatures[ReadingCelsius<=45.0]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre(
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/0",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/1",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/2",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/3",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/4",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/5",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/6",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/7",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/8",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/9",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/10",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/11"));
}
TEST(GetRedPath, GetArrayNodeValueLessEqualThanIntValue) {
  FakeRedfishServer server("indus_hmb_cn_playground/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath =
      "/Chassis[*]/Thermal/Temperatures[ReadingCelsius<=45]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre(
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/0",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/1",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/2",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/3",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/4",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/5",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/6",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/7",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/8",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/9",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/10",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/11"));
}
TEST(GetRedPath, GetArrayWrongNodeValueLessEqualThanIntValue) {
  FakeRedfishServer server("indus_hmb_cn_playground/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath =
      "/Chassis[*]/Thermal/Temperatures[readingCelsius<=45]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST(GetRedPath, GetArrayEmptyNodeLessEqualThanIntValue) {
  FakeRedfishServer server("indus_hmb_cn_playground/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath =
      "/Chassis[*]/Thermal/Temperatures[<=45]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST(GetRedPath, GetArrayNodeValueLessEqualThanEmptyValue) {
  FakeRedfishServer server("indus_hmb_cn_playground/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath =
      "/Chassis[*]/Thermal/Temperatures[ReadingCelsius<=]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST(GetRedPath, GetArrayEmptyNodeValueLessEqualThanEmptyValue) {
  FakeRedfishServer server("indus_hmb_cn_playground/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Chassis[*]/Thermal/Temperatures[<=]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre());
}

// ****************************[node>value]*************************************
TEST(GetRedPath, GetObjectNodeValueGreaterThanDoubleValue) {
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Chassis[*]/Sensors[Reading>16114.9]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre(
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan0_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan1_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan2_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan3_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan4_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan5_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan6_rpm"));
}
TEST(GetRedPath, GetObjectNodeValueGreaterThanIntValue) {
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Chassis[*]/Sensors[Reading>16114]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre(
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan0_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan1_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan2_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan3_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan4_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan5_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan6_rpm"));
}
TEST(GetRedPath, GetArrayNodeValueGreaterThanDoubleValue) {
  FakeRedfishServer server("indus_hmb_cn_playground/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath =
      "/Chassis[*]/Thermal/Temperatures[ReadingCelsius>45.0]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre(
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/12",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/13",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/14",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/15",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/16",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/17",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/18",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/19",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/20",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/21",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/22",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/23"));
}
TEST(GetRedPath, GetArrayNodeValueGreaterThanIntValue) {
  FakeRedfishServer server("indus_hmb_cn_playground/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath =
      "/Chassis[*]/Thermal/Temperatures[ReadingCelsius>45]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre(
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/12",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/13",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/14",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/15",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/16",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/17",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/18",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/19",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/20",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/21",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/22",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/23"));
}
TEST(GetRedPath, GetObjectWrongNodeGreaterThanDoubleValue) {
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Chassis[*]/Sensors[reading>16114.9]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST(GetRedPath, GetObjectEmptyNodeGreaterThanDoubleValue) {
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Chassis[*]/Sensors[>16114.9]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST(GetRedPath, GetObjectNodeGreaterThanEmptyValue) {
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Chassis[*]/Sensors[Reading>]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST(GetRedPath, GetObjectEmptyNodeGreaterThanEmptyValue) {
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Chassis[*]/Sensors[>]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre());
}

// ***************************[node<value]**************************************
TEST(GetRedPath, GetObjectNodeValueLessThanDoubleValue) {
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Chassis[*]/Sensors[Reading<100.0]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre(
                       "/redfish/v1/Chassis/chassis/Sensors/indus_eat_temp",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_latm_temp",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_cpu0_pwmon",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_cpu1_pwmon",
                       "/redfish/v1/Chassis/chassis/Sensors/i_cpu0_t",
                       "/redfish/v1/Chassis/chassis/Sensors/i_cpu1_t"));
}
TEST(GetRedPath, GetObjectNodeValueLessThanIntValue) {
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Chassis[*]/Sensors[Reading<100]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre(
                       "/redfish/v1/Chassis/chassis/Sensors/indus_eat_temp",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_latm_temp",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_cpu0_pwmon",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_cpu1_pwmon",
                       "/redfish/v1/Chassis/chassis/Sensors/i_cpu0_t",
                       "/redfish/v1/Chassis/chassis/Sensors/i_cpu1_t"));
}
TEST(GetRedPath, GetArrayNodeValueLessThanDoubleValue) {
  FakeRedfishServer server("indus_hmb_cn_playground/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath =
      "/Chassis[*]/Thermal/Temperatures[ReadingCelsius<50.0]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre(
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/0",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/1",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/2",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/3",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/4",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/5",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/6",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/7",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/8",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/9",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/10",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/11"));
}
TEST(GetRedPath, GetArrayNodeValueLessThanIntValue) {
  FakeRedfishServer server("indus_hmb_cn_playground/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath =
      "/Chassis[*]/Thermal/Temperatures[ReadingCelsius<50]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre(
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/0",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/1",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/2",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/3",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/4",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/5",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/6",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/7",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/8",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/9",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/10",
                       "/redfish/v1/Chassis/Indus/Thermal/#/Temperatures/11"));
}
TEST(GetRedPath, GetObjectWrongNodeValueLessThanDoubleValue) {
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Chassis[*]/Sensors[reading<100.0]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST(GetRedPath, GetObjectEmptyNodeValueLessThanDoubleValue) {
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Chassis[*]/Sensors[<100.0]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST(GetRedPath, GetObjectNodeValueLessThanEmptyValue) {
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Chassis[*]/Sensors[Reading<]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST(GetRedPath, GetObjectEmptyNodeValueLessThanEmptyValue) {
  FakeRedfishServer server("indus_hmb_shim/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Chassis[*]/Sensors[<]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre());
}

// ***************************[node.child]**************************************
TEST(GetRedPath, GetObjectChild) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Systems[*]/Memory[Status.State]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids,
              UnorderedElementsAre("/redfish/v1/Systems/system/Memory/0",
                                   "/redfish/v1/Systems/system/Memory/1",
                                   "/redfish/v1/Systems/system/Memory/2",
                                   "/redfish/v1/Systems/system/Memory/3",
                                   "/redfish/v1/Systems/system/Memory/4",
                                   "/redfish/v1/Systems/system/Memory/5",
                                   "/redfish/v1/Systems/system/Memory/6",
                                   "/redfish/v1/Systems/system/Memory/7",
                                   "/redfish/v1/Systems/system/Memory/8",
                                   "/redfish/v1/Systems/system/Memory/9",
                                   "/redfish/v1/Systems/system/Memory/10",
                                   "/redfish/v1/Systems/system/Memory/11",
                                   "/redfish/v1/Systems/system/Memory/12",
                                   "/redfish/v1/Systems/system/Memory/13",
                                   "/redfish/v1/Systems/system/Memory/14",
                                   "/redfish/v1/Systems/system/Memory/15",
                                   "/redfish/v1/Systems/system/Memory/16",
                                   "/redfish/v1/Systems/system/Memory/17",
                                   "/redfish/v1/Systems/system/Memory/18",
                                   "/redfish/v1/Systems/system/Memory/19",
                                   "/redfish/v1/Systems/system/Memory/20",
                                   "/redfish/v1/Systems/system/Memory/21",
                                   "/redfish/v1/Systems/system/Memory/22",
                                   "/redfish/v1/Systems/system/Memory/23"));
}
TEST(GetRedPath, GetArrayChild) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath =
      "/Chassis[*]/Assembly/Assemblies[Oem.Google]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre(
                       "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/0",
                       "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/1",
                       "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/2",
                       "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/3",
                       "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/4",
                       "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/5",
                       "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/6",
                       "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/7",
                       "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/8",
                       "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/9",
                       "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/10",
                       "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/11",
                       "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/12",
                       "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/13",
                       "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/14",
                       "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/15",
                       "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/16",
                       "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/17"));
}
TEST(GetRedPath, GetArrayMultiChild) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath =
      "/Chassis[*]/Assembly/Assemblies[Oem.Google.AttachedTo]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre(
                       "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/1",
                       "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/2",
                       "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/3",
                       "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/4",
                       "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/5",
                       "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/6",
                       "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/7",
                       "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/8",
                       "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/9",
                       "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/10",
                       "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/11",
                       "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/12",
                       "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/13",
                       "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/14",
                       "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/15",
                       "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/16",
                       "/redfish/v1/Chassis/chassis/Assembly#/Assemblies/17"));
}
TEST(GetRedPath, GetArrayMultiChildAndVelue) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath =
      "/Systems[*]/Processors[*]/"
      "Metrics[Oem.Google.ProcessorErrorCounts.Correctable=0]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids,
              UnorderedElementsAre(
                  "/redfish/v1/Systems/system/Processors/0/ProcessorMetrics",
                  "/redfish/v1/Systems/system/Processors/1/ProcessorMetrics"));
}
TEST(GetRedPath, GetObjectWithWrongNodeAndChild) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Systems[*]/Memory[status.State]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST(GetRedPath, GetObjectWithNodeAndWrongChild) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Systems[*]/Memory[Status.state]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST(GetRedPath, GetObjectWithEmptyNodeAndChild) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Systems[*]/Memory[.state]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST(GetRedPath, GetObjectWithNodeAndEmptyChild) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Systems[*]/Memory[Status.]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST(GetRedPath, GetObjectWithEmptyNodeAndEmptyChild) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Systems[*]/Memory[.]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre());
}

// *************************[node.child=value]**********************************
TEST(GetRedPath, GetObjectChildwithValue) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath =
      "/Systems[*]/Memory[Status.State=Enabled]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids,
              UnorderedElementsAre("/redfish/v1/Systems/system/Memory/0",
                                   "/redfish/v1/Systems/system/Memory/1",
                                   "/redfish/v1/Systems/system/Memory/2",
                                   "/redfish/v1/Systems/system/Memory/3",
                                   "/redfish/v1/Systems/system/Memory/4",
                                   "/redfish/v1/Systems/system/Memory/5",
                                   "/redfish/v1/Systems/system/Memory/6",
                                   "/redfish/v1/Systems/system/Memory/7",
                                   "/redfish/v1/Systems/system/Memory/8",
                                   "/redfish/v1/Systems/system/Memory/9",
                                   "/redfish/v1/Systems/system/Memory/10",
                                   "/redfish/v1/Systems/system/Memory/11",
                                   "/redfish/v1/Systems/system/Memory/12",
                                   "/redfish/v1/Systems/system/Memory/13",
                                   "/redfish/v1/Systems/system/Memory/14",
                                   "/redfish/v1/Systems/system/Memory/15",
                                   "/redfish/v1/Systems/system/Memory/16",
                                   "/redfish/v1/Systems/system/Memory/17",
                                   "/redfish/v1/Systems/system/Memory/18",
                                   "/redfish/v1/Systems/system/Memory/19",
                                   "/redfish/v1/Systems/system/Memory/20",
                                   "/redfish/v1/Systems/system/Memory/21",
                                   "/redfish/v1/Systems/system/Memory/22",
                                   "/redfish/v1/Systems/system/Memory/23"));
}
TEST(GetRedPath, GetArrayChildwithValue) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath =
      "/Chassis[*]/Thermal/Temperatures[Status.State=Enabled]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids,
              UnorderedElementsAre(
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/0",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/1",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/2",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/3",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/4",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/5",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/6",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/7",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/8",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/9",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/10",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/11",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/12",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/13",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/14",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/15",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/16",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/17",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/18",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/19",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/20",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/21",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/22",
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/23"));
}
TEST(GetRedPath, GetObjectNodeAndChildwithWrongValue) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath =
      "/Systems[*]/Memory[status.State=Enable]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST(GetRedPath, GetObjectNodeAndChildwithEmptyValue) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Systems[*]/Memory[Status.State=]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST(GetRedPath, GetObjectEmptyNodeAndEmptyChildwithEmptyValue) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Systems[*]/Memory[.=]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kContinue;
  });
  EXPECT_THAT(ids, UnorderedElementsAre());
}

// Other Test
TEST(GetRedPath, RedfishIterReturnValuekStop) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/Chassis[*]/Thermal/Temperatures[*]";
  GetRedPath(intf.get(), kRedpath, [&](std::unique_ptr<RedfishObject> object) {
    auto value = object->GetUriString();
    ids.push_back(std::move(*value));
    return RedfishIterReturnValue::kStop;
  });
  EXPECT_THAT(ids, UnorderedElementsAre(
                       "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/0"));
}

}  // namespace
}  // namespace ecclesia