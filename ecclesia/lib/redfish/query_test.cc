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

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/property_definitions.h"
#include "ecclesia/lib/redfish/testing/fake_redfish_server.h"

namespace ecclesia {
namespace {
using ::testing::UnorderedElementsAre;

template <const absl::string_view& mockup>
class RedPathQueryTest : public testing::Test {
 protected:
  RedPathQueryTest()
      : server_(mockup), intf_(server_.RedfishClientInterface()) {}

  void PopulateIdsFromRedpath(absl::string_view redpath) {
    GetRedPath(intf_.get(), redpath,
               [&](std::unique_ptr<RedfishObject> object) {
                 auto value = object->GetNodeValue<PropertyOdataId>();
                 if (value.has_value()) {
                   ids.push_back(std::move(*value));
                 }
                 return RedfishIterReturnValue::kContinue;
               });
  }

  std::vector<std::string> ids;
  FakeRedfishServer server_;
  std::unique_ptr<RedfishInterface> intf_;
};

inline constexpr absl::string_view kMockupPathIndus =
    "indus_hmb_cn/mockup.shar";
inline constexpr absl::string_view kMockupPathIndusPlayground =
    "indus_hmb_cn_playground/mockup.shar";
inline constexpr absl::string_view kMockupPathIndusShim =
    "indus_hmb_shim/mockup.shar";

using GetRedPathTestIndus = RedPathQueryTest<kMockupPathIndus>;
using GetRedPathTestIndusPlayground =
    RedPathQueryTest<kMockupPathIndusPlayground>;
using GetRedPathTestIndusShim = RedPathQueryTest<kMockupPathIndusShim>;

// Root
TEST_F(GetRedPathTestIndus, RootRedPath) {
  PopulateIdsFromRedpath("/");
  EXPECT_THAT(ids, UnorderedElementsAre("/redfish/v1"));
}

// ********************************[*]******************************************
TEST_F(GetRedPathTestIndus, GetObjectAllSystem) {
  PopulateIdsFromRedpath("/Systems[*]");
  EXPECT_THAT(ids, UnorderedElementsAre("/redfish/v1/Systems/system"));
}
TEST_F(GetRedPathTestIndus, GetObjectAllMemory) {
  PopulateIdsFromRedpath("/Systems[*]/Memory[*]");
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
TEST_F(GetRedPathTestIndus, GetNoDelimiter) {
  PopulateIdsFromRedpath("/Systems[*]/Memory[*]/Assembly");
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
TEST_F(GetRedPathTestIndus, GetArrayAllMembers) {
  PopulateIdsFromRedpath("/Chassis[*]/Thermal/Temperatures[*]");
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
TEST_F(GetRedPathTestIndus, WrongWordRedPath) {
  PopulateIdsFromRedpath("/Syste[*]/Memory[*]");
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST_F(GetRedPathTestIndus, SpaceAsRedPath) {
  PopulateIdsFromRedpath("  ");
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST_F(GetRedPathTestIndus, WrongCharacterRedPath) {
  PopulateIdsFromRedpath("/System[*]/z~023=");
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST_F(GetRedPathTestIndus, EmptyRedPath) {
  PopulateIdsFromRedpath("");
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST_F(GetRedPathTestIndus, RedPathWithEmptyFilter) {
  PopulateIdsFromRedpath("/Syste[*]/Memory[]");
  EXPECT_THAT(ids, UnorderedElementsAre());
}

// *****************************[Index]*****************************************
TEST_F(GetRedPathTestIndus, GetObjectByIndex) {
  PopulateIdsFromRedpath("/Systems[*]/Memory[3]");
  EXPECT_THAT(ids, UnorderedElementsAre("/redfish/v1/Systems/system/Memory/3"));
}
TEST_F(GetRedPathTestIndus, GetArrayByIndex) {
  PopulateIdsFromRedpath("/Chassis[*]/Thermal/Temperatures[5]");
  EXPECT_THAT(ids, UnorderedElementsAre(
                       "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/5"));
}
TEST_F(GetRedPathTestIndus, GetObjectByNotExistIndex) {
  PopulateIdsFromRedpath("/Systems[*]/Memory[100]");
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST_F(GetRedPathTestIndus, GetObjectByNegativeIndex) {
  PopulateIdsFromRedpath("/Systems[*]/Memory[-1]");
  EXPECT_THAT(ids, UnorderedElementsAre());
}

// *****************************[last()]****************************************
TEST_F(GetRedPathTestIndus, GetLastObject) {
  PopulateIdsFromRedpath("/Systems[*]/Memory[last()]");
  EXPECT_THAT(ids,
              UnorderedElementsAre("/redfish/v1/Systems/system/Memory/23"));
}
TEST_F(GetRedPathTestIndus, GetLastArray) {
  PopulateIdsFromRedpath("/Chassis[*]/Thermal/Temperatures[last()]");
  EXPECT_THAT(ids,
              UnorderedElementsAre(
                  "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/23"));
}
TEST_F(GetRedPathTestIndus, PropertyDoNotHaveMembersOrArrayToGetLast) {
  PopulateIdsFromRedpath("/Chassis[*]/Thermal[last()]");
  EXPECT_THAT(ids, UnorderedElementsAre());
}

// ***************************[nodename]****************************************
TEST_F(GetRedPathTestIndus, GetObjectByNodename) {
  PopulateIdsFromRedpath("/Systems[*]/Memory[SerialNumber]/Assembly");
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
TEST_F(GetRedPathTestIndus, GetArrayByNodename) {
  PopulateIdsFromRedpath("/Systems[*]/Processors[*]/Assembly/Assemblies[Name]");
  EXPECT_THAT(
      ids,
      UnorderedElementsAre(
          "/redfish/v1/Systems/system/Processors/0/Assembly#/Assemblies/0",
          "/redfish/v1/Systems/system/Processors/1/Assembly#/Assemblies/0"));
}
TEST_F(GetRedPathTestIndus, GetArrayByNodenameWithSpecialCharacter) {
  PopulateIdsFromRedpath(
      "/Systems[*]/Processors[*]/Assembly/Assemblies[@odata.id]");
  EXPECT_THAT(
      ids,
      UnorderedElementsAre(
          "/redfish/v1/Systems/system/Processors/0/Assembly#/Assemblies/0",
          "/redfish/v1/Systems/system/Processors/1/Assembly#/Assemblies/0"));
}
TEST_F(GetRedPathTestIndus, GetObjectByNodenameWithNumValue) {
  PopulateIdsFromRedpath("/Systems[*]/Processors[MaxSpeedMHz]");
  EXPECT_THAT(ids,
              UnorderedElementsAre("/redfish/v1/Systems/system/Processors/0",
                                   "/redfish/v1/Systems/system/Processors/1"));
}
TEST_F(GetRedPathTestIndus, GetUnexistNodename) {
  PopulateIdsFromRedpath("/Systems[*]/Processors[*]/Assembly/Assemblies[Test]");
  EXPECT_THAT(ids, UnorderedElementsAre());
}

// ***************************[node=value]**************************************
TEST_F(GetRedPathTestIndus, GetObjectByNodenameAndValue) {
  PopulateIdsFromRedpath("/Systems[*]/Memory[Name=DIMM0]/Assembly");
  EXPECT_THAT(ids, UnorderedElementsAre(
                       "/redfish/v1/Systems/system/Memory/0/Assembly"));
}
TEST_F(GetRedPathTestIndus, GetObjectMemberByNodename) {
  PopulateIdsFromRedpath("/Systems[Id=system]");
  EXPECT_THAT(ids, UnorderedElementsAre("/redfish/v1/Systems/system"));
}
TEST_F(GetRedPathTestIndus, GetObjectByNodenameAndNumValue) {
  PopulateIdsFromRedpath("/Systems[*]/Processors[MaxSpeedMHz=4000]");
  EXPECT_THAT(ids,
              UnorderedElementsAre("/redfish/v1/Systems/system/Processors/0",
                                   "/redfish/v1/Systems/system/Processors/1"));
}
TEST_F(GetRedPathTestIndus, GetObjectByNodenameAndNumValueDoNotExist) {
  PopulateIdsFromRedpath("/Systems[*]/Processors[MaxSpeedMHz=4001]");
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST_F(GetRedPathTestIndus, GetArrayByNodenameAndValue) {
  PopulateIdsFromRedpath("/Chassis[*]/Thermal/Temperatures[Name=dimm0]");
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  EXPECT_THAT(ids, UnorderedElementsAre(
                       "/redfish/v1/Chassis/chassis/Thermal/#/Temperatures/0"));
}
TEST_F(GetRedPathTestIndusPlayground, GetObjectNodewithBoolValue) {
  PopulateIdsFromRedpath("/Chassis[*]/Thermal/Fans[HotPluggable=false]");
  EXPECT_THAT(
      ids, UnorderedElementsAre("/redfish/v1/Chassis/Indus/Thermal/#/Fans/0"));
}
TEST_F(GetRedPathTestIndusPlayground, GetObjectNodewithNullValue) {
  PopulateIdsFromRedpath("/Chassis[*]/Thermal/Fans[LowerThresholdFatal=null]");
  EXPECT_THAT(
      ids, UnorderedElementsAre("/redfish/v1/Chassis/Indus/Thermal/#/Fans/0"));
}
TEST_F(GetRedPathTestIndus, GetObjectByNodenameAndWrongValue) {
  PopulateIdsFromRedpath("/Systems[*]/Memory[Name=DIM]/Assembly");
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST_F(GetRedPathTestIndus, GetObjectByWrongNodenameAndValue) {
  PopulateIdsFromRedpath("/Systems[*]/Memory[id=DIMM0]/Assembly");
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST_F(GetRedPathTestIndus, GetObjectByEmptyNodenameAndValue) {
  PopulateIdsFromRedpath("/Systems[*]/Memory[=DIMM0]/Assembly");
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST_F(GetRedPathTestIndus, GetObjectByNodenameAndEmptyValue) {
  PopulateIdsFromRedpath("/Systems[*]/Memory[Id=]/Assembly");
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST_F(GetRedPathTestIndus, GetObjectByEmptyNodenameAndEmptyValue) {
  PopulateIdsFromRedpath("/Systems[*]/Memory[=]/Assembly");
  EXPECT_THAT(ids, UnorderedElementsAre());
}

// ****************************[node!=value]************************************
TEST_F(GetRedPathTestIndus, GetObjectNameNotEqualValue) {
  PopulateIdsFromRedpath("/Systems[*]/Memory[Id!=DIMM0]");
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
TEST_F(GetRedPathTestIndus, GetArrayNameNotEqualValue) {
  PopulateIdsFromRedpath("/Chassis[*]/Thermal/Temperatures[Name!=dimm0]");
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
TEST_F(GetRedPathTestIndusPlayground, GetArrayNameNotEqualNumberValue) {
  PopulateIdsFromRedpath(
      "/Chassis[*]/Thermal/Temperatures[ReadingCelsius!=45]");
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
TEST_F(GetRedPathTestIndus, GetObjectByNodenameDoNotExisttNotEqualValue) {
  PopulateIdsFromRedpath(
      "/Systems[*]/Processors[Reading!=#Processor.v1_7_1.Processor]");
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST_F(GetRedPathTestIndusPlayground, GetObjectNodeNotEqualBoolValue) {
  PopulateIdsFromRedpath("/Chassis[*]/Thermal/Fans[HotPluggable!=true]");
  EXPECT_THAT(
      ids, UnorderedElementsAre("/redfish/v1/Chassis/Indus/Thermal/#/Fans/0"));
}
TEST_F(GetRedPathTestIndusPlayground, GetObjectNodeNotEqualNullValue) {
  PopulateIdsFromRedpath(
      "/Chassis[*]/Thermal/Fans[LowerThresholdNonCritical!=null]");
  EXPECT_THAT(
      ids, UnorderedElementsAre("/redfish/v1/Chassis/Indus/Thermal/#/Fans/0"));
}
TEST_F(GetRedPathTestIndus, GetObjectWithWrongNameNotEqualValue) {
  PopulateIdsFromRedpath("/Systems[*]/Memory[id!=DIMM0]");
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST_F(GetRedPathTestIndus, GetObjectWithEmptyNameNotEqualValue) {
  PopulateIdsFromRedpath("/Systems[*]/Memory[!=DIMM0]");
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST_F(GetRedPathTestIndus, GetObjectWithNameNotEqualEmptyValue) {
  PopulateIdsFromRedpath("/Systems[*]/Memory[Id!=]");
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
TEST_F(GetRedPathTestIndus, GetObjectWithEmptyNameEqualEmptyValue) {
  PopulateIdsFromRedpath("/Systems[*]/Memory[!=]");
  EXPECT_THAT(ids, UnorderedElementsAre());
}

// **************************[node>=value]**************************************
TEST_F(GetRedPathTestIndusShim, GetObjectNodeValueGreaterEqualThanDoubleValue) {
  PopulateIdsFromRedpath("/Chassis[*]/Sensors[Reading>=16115.0]");
  EXPECT_THAT(ids, UnorderedElementsAre(
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan0_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan1_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan2_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan3_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan4_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan5_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan6_rpm"));
}
TEST_F(GetRedPathTestIndusShim, GetObjectNodeValueGreaterEqualThanIntValue) {
  PopulateIdsFromRedpath("/Chassis[*]/Sensors[Reading>=16115]");
  EXPECT_THAT(ids, UnorderedElementsAre(
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan0_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan1_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan2_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan3_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan4_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan5_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan6_rpm"));
}
TEST_F(GetRedPathTestIndusPlayground,
       GetArrayNodeValueGreaterEqualThanDoubleValue) {
  PopulateIdsFromRedpath(
      "/Chassis[*]/Thermal/Temperatures[ReadingCelsius>=50.0]");
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
TEST_F(GetRedPathTestIndusPlayground,
       GetArrayNodeValueGreaterEqualThanIntValue) {
  PopulateIdsFromRedpath(
      "/Chassis[*]/Thermal/Temperatures[ReadingCelsius>=50]");
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
TEST_F(GetRedPathTestIndusShim, GetObjectStringNodeValueGreaterEqual) {
  PopulateIdsFromRedpath("/Chassis[*]/Sensors[ReadingType>=16115.0]");
  EXPECT_THAT(ids, UnorderedElementsAre());
}

TEST_F(GetRedPathTestIndusPlayground,
       GetArrayWrongNodeGreaterEqualThanIntValue) {
  PopulateIdsFromRedpath(
      "/Chassis[*]/Thermal/Temperatures[readingCelsius>=50]");
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST_F(GetRedPathTestIndusPlayground,
       GetArrayEmptyNodeGreaterEqualThanIntValue) {
  PopulateIdsFromRedpath("/Chassis[*]/Thermal/Temperatures[>=50]");
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST_F(GetRedPathTestIndusPlayground, GetArrayNodeGreaterEuqalThanEmptyValue) {
  PopulateIdsFromRedpath("/Chassis[*]/Thermal/Temperatures[ReadingCelsius>=]");
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST_F(GetRedPathTestIndusPlayground,
       GetArrayEmptyNodeGreaterEqualThanEmptyValue) {
  PopulateIdsFromRedpath("/Chassis[*]/Thermal/Temperatures[>=]");
  EXPECT_THAT(ids, UnorderedElementsAre());
}

// ************************* [node<=value]**************************************
TEST_F(GetRedPathTestIndusShim, GetObjectNodeValueLessEqualThanDoubleValue) {
  PopulateIdsFromRedpath("/Chassis[*]/Sensors[Reading<=60.0]");
  EXPECT_THAT(ids, UnorderedElementsAre(
                       "/redfish/v1/Chassis/chassis/Sensors/indus_eat_temp",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_latm_temp",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_cpu0_pwmon",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_cpu1_pwmon",
                       "/redfish/v1/Chassis/chassis/Sensors/i_cpu0_t",
                       "/redfish/v1/Chassis/chassis/Sensors/i_cpu1_t"));
}
TEST_F(GetRedPathTestIndusShim, GetObjectNodeValueLessEqualThanIntValue) {
  PopulateIdsFromRedpath("/Chassis[*]/Sensors[Reading<=60]");
  EXPECT_THAT(ids, UnorderedElementsAre(
                       "/redfish/v1/Chassis/chassis/Sensors/indus_eat_temp",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_latm_temp",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_cpu0_pwmon",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_cpu1_pwmon",
                       "/redfish/v1/Chassis/chassis/Sensors/i_cpu0_t",
                       "/redfish/v1/Chassis/chassis/Sensors/i_cpu1_t"));
}
TEST_F(GetRedPathTestIndusPlayground,
       GetArrayNodeValueLessEqualThanDoubleValue) {
  PopulateIdsFromRedpath(
      "/Chassis[*]/Thermal/Temperatures[ReadingCelsius<=45.0]");
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
TEST_F(GetRedPathTestIndusPlayground, GetArrayNodeValueLessEqualThanIntValue) {
  PopulateIdsFromRedpath(
      "/Chassis[*]/Thermal/Temperatures[ReadingCelsius<=45]");
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
TEST_F(GetRedPathTestIndusPlayground,
       GetArrayWrongNodeValueLessEqualThanIntValue) {
  PopulateIdsFromRedpath(
      "/Chassis[*]/Thermal/Temperatures[readingCelsius<=45]");
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST_F(GetRedPathTestIndusPlayground, GetArrayEmptyNodeLessEqualThanIntValue) {
  PopulateIdsFromRedpath("/Chassis[*]/Thermal/Temperatures[<=45]");
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST_F(GetRedPathTestIndusPlayground,
       GetArrayNodeValueLessEqualThanEmptyValue) {
  PopulateIdsFromRedpath("/Chassis[*]/Thermal/Temperatures[ReadingCelsius<=]");
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST_F(GetRedPathTestIndusPlayground,
       GetArrayEmptyNodeValueLessEqualThanEmptyValue) {
  PopulateIdsFromRedpath("/Chassis[*]/Thermal/Temperatures[<=]");
  EXPECT_THAT(ids, UnorderedElementsAre());
}

// ****************************[node>value]*************************************
TEST_F(GetRedPathTestIndusShim, GetObjectNodeValueGreaterThanDoubleValue) {
  PopulateIdsFromRedpath("/Chassis[*]/Sensors[Reading>16114.9]");
  EXPECT_THAT(ids, UnorderedElementsAre(
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan0_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan1_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan2_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan3_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan4_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan5_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan6_rpm"));
}
TEST_F(GetRedPathTestIndusShim, GetObjectNodeValueGreaterThanIntValue) {
  PopulateIdsFromRedpath("/Chassis[*]/Sensors[Reading>16114]");
  EXPECT_THAT(ids, UnorderedElementsAre(
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan0_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan1_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan2_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan3_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan4_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan5_rpm",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_fan6_rpm"));
}
TEST_F(GetRedPathTestIndusPlayground, GetArrayNodeValueGreaterThanDoubleValue) {
  PopulateIdsFromRedpath(
      "/Chassis[*]/Thermal/Temperatures[ReadingCelsius>45.0]");
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
TEST_F(GetRedPathTestIndusPlayground, GetArrayNodeValueGreaterThanIntValue) {
  PopulateIdsFromRedpath("/Chassis[*]/Thermal/Temperatures[ReadingCelsius>45]");
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
TEST_F(GetRedPathTestIndusShim, GetObjectWrongNodeGreaterThanDoubleValue) {
  PopulateIdsFromRedpath("/Chassis[*]/Sensors[reading>16114.9]");
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST_F(GetRedPathTestIndusShim, GetObjectEmptyNodeGreaterThanDoubleValue) {
  PopulateIdsFromRedpath("/Chassis[*]/Sensors[>16114.9]");
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST_F(GetRedPathTestIndusShim, GetObjectNodeGreaterThanEmptyValue) {
  PopulateIdsFromRedpath("/Chassis[*]/Sensors[Reading>]");
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST_F(GetRedPathTestIndusShim, GetObjectEmptyNodeGreaterThanEmptyValue) {
  PopulateIdsFromRedpath("/Chassis[*]/Sensors[>]");
  EXPECT_THAT(ids, UnorderedElementsAre());
}

// ***************************[node<value]**************************************
TEST_F(GetRedPathTestIndusShim, GetObjectNodeValueLessThanDoubleValue) {
  PopulateIdsFromRedpath("/Chassis[*]/Sensors[Reading<100.0]");
  EXPECT_THAT(ids, UnorderedElementsAre(
                       "/redfish/v1/Chassis/chassis/Sensors/indus_eat_temp",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_latm_temp",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_cpu0_pwmon",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_cpu1_pwmon",
                       "/redfish/v1/Chassis/chassis/Sensors/i_cpu0_t",
                       "/redfish/v1/Chassis/chassis/Sensors/i_cpu1_t"));
}
TEST_F(GetRedPathTestIndusShim, GetObjectNodeValueLessThanIntValue) {
  PopulateIdsFromRedpath("/Chassis[*]/Sensors[Reading<100]");
  EXPECT_THAT(ids, UnorderedElementsAre(
                       "/redfish/v1/Chassis/chassis/Sensors/indus_eat_temp",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_latm_temp",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_cpu0_pwmon",
                       "/redfish/v1/Chassis/chassis/Sensors/indus_cpu1_pwmon",
                       "/redfish/v1/Chassis/chassis/Sensors/i_cpu0_t",
                       "/redfish/v1/Chassis/chassis/Sensors/i_cpu1_t"));
}
TEST_F(GetRedPathTestIndusPlayground, GetArrayNodeValueLessThanDoubleValue) {
  PopulateIdsFromRedpath(
      "/Chassis[*]/Thermal/Temperatures[ReadingCelsius<50.0]");
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
TEST_F(GetRedPathTestIndusPlayground, GetArrayNodeValueLessThanIntValue) {
  PopulateIdsFromRedpath("/Chassis[*]/Thermal/Temperatures[ReadingCelsius<50]");
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
TEST_F(GetRedPathTestIndusShim, GetObjectWrongNodeValueLessThanDoubleValue) {
  PopulateIdsFromRedpath("/Chassis[*]/Sensors[reading<100.0]");
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST_F(GetRedPathTestIndusShim, GetObjectEmptyNodeValueLessThanDoubleValue) {
  PopulateIdsFromRedpath("/Chassis[*]/Sensors[<100.0]");
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST_F(GetRedPathTestIndusShim, GetObjectNodeValueLessThanEmptyValue) {
  PopulateIdsFromRedpath("/Chassis[*]/Sensors[Reading<]");
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST_F(GetRedPathTestIndusShim, GetObjectEmptyNodeValueLessThanEmptyValue) {
  PopulateIdsFromRedpath("/Chassis[*]/Sensors[<]");
  EXPECT_THAT(ids, UnorderedElementsAre());
}

// ***************************[node.child]**************************************
TEST_F(GetRedPathTestIndus, GetObjectChild) {
  PopulateIdsFromRedpath("/Systems[*]/Memory[Status.State]");
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
TEST_F(GetRedPathTestIndus, GetArrayChild) {
  PopulateIdsFromRedpath("/Chassis[*]/Assembly/Assemblies[Oem.Google]");
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
TEST_F(GetRedPathTestIndus, GetArrayMultiChild) {
  PopulateIdsFromRedpath(
      "/Chassis[*]/Assembly/Assemblies[Oem.Google.AttachedTo]");
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
TEST_F(GetRedPathTestIndus, GetArrayMultiChildAndValue) {
  PopulateIdsFromRedpath(
      "/Systems[*]/Processors[*]/"
      "Metrics[Oem.Google.ProcessorErrorCounts.Correctable=0]");
  EXPECT_THAT(ids,
              UnorderedElementsAre(
                  "/redfish/v1/Systems/system/Processors/0/ProcessorMetrics",
                  "/redfish/v1/Systems/system/Processors/1/ProcessorMetrics"));
}
TEST_F(GetRedPathTestIndus, GetObjectWithWrongNodeAndChild) {
  PopulateIdsFromRedpath("/Systems[*]/Memory[status.State]");
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST_F(GetRedPathTestIndus, GetObjectWithNodeAndWrongChild) {
  PopulateIdsFromRedpath("/Systems[*]/Memory[Status.state]");
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST_F(GetRedPathTestIndus, GetObjectWithEmptyNodeAndChild) {
  PopulateIdsFromRedpath("/Systems[*]/Memory[.state]");
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST_F(GetRedPathTestIndus, GetObjectWithNodeAndEmptyChild) {
  PopulateIdsFromRedpath("/Systems[*]/Memory[Status.]");
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST_F(GetRedPathTestIndus, GetObjectWithEmptyNodeAndEmptyChild) {
  PopulateIdsFromRedpath("/Systems[*]/Memory[.]");
  EXPECT_THAT(ids, UnorderedElementsAre());
}

// *************************[node.child=value]**********************************
TEST_F(GetRedPathTestIndus, GetObjectChildwithValue) {
  PopulateIdsFromRedpath("/Systems[*]/Memory[Status.State=Enabled]");
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
TEST_F(GetRedPathTestIndus, GetArrayChildwithValue) {
  PopulateIdsFromRedpath(
      "/Chassis[*]/Thermal/Temperatures[Status.State=Enabled]");
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
TEST_F(GetRedPathTestIndusPlayground, GetObjectNodeAndChildwithNullValue) {
  PopulateIdsFromRedpath(
      "/Systems[*]/Memory[*]/"
      "Metrics[Oem.Google.MemoryErrorCounts.Count=null]");
  EXPECT_THAT(ids, UnorderedElementsAre(
                       "/redfish/v1/Systems/system/Memory/23/MemoryMetrics"));
}
TEST_F(GetRedPathTestIndusPlayground, GetObjectNodeAndChildwithBoolValue) {
  PopulateIdsFromRedpath(
      "/Systems[*]/Memory[*]/"
      "Metrics[Oem.Google.MemoryErrorCounts.Countable=false]");
  EXPECT_THAT(ids, UnorderedElementsAre(
                       "/redfish/v1/Systems/system/Memory/23/MemoryMetrics"));
}
TEST_F(GetRedPathTestIndus, GetObjectNodeAndChildwithWrongValue) {
  PopulateIdsFromRedpath("/Systems[*]/Memory[status.State=Enable]");
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST_F(GetRedPathTestIndus, GetObjectNodeAndChildwithEmptyValue) {
  PopulateIdsFromRedpath("/Systems[*]/Memory[Status.State=]");
  EXPECT_THAT(ids, UnorderedElementsAre());
}
TEST_F(GetRedPathTestIndus, GetObjectEmptyNodeAndEmptyChildwithEmptyValue) {
  PopulateIdsFromRedpath("/Systems[*]/Memory[.=]");
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