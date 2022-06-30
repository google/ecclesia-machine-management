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

TEST(GetRedPath, GetAllSystem) {
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
TEST(GetRedPath, GetAllMemory) {
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

TEST(GetRedPath, WrongWordRedPath) {
  FakeRedfishServer server("indus_hmb_cn/mockup.shar");
  std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
  std::vector<std::string> ids;
  constexpr absl::string_view kRedpath = "/System[*]/Memory[*]";
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

}  // namespace
}  // namespace ecclesia