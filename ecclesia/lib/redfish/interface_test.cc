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

#include "ecclesia/lib/redfish/interface.h"

#include <memory>
#include <optional>
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "ecclesia/lib/http/codes.h"
#include "ecclesia/lib/redfish/property_definitions.h"
#include "ecclesia/lib/redfish/testing/fake_redfish_server.h"
#include "ecclesia/lib/testing/status.h"
#include "tensorflow_serving/util/net_http/public/response_code_enum.h"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"

namespace ecclesia {
namespace {

using ::ecclesia::IsStatusFailedPrecondition;
using ::tensorflow::serving::net_http::HTTPStatusCode;
using ::tensorflow::serving::net_http::ServerRequestInterface;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::Ne;
using ::testing::Optional;
using ::testing::Pair;

TEST(RedfishVariant, DebugMsgShouldHaveIndent) {
  ecclesia::FakeRedfishServer mockup("barebones_session_auth/mockup.shar");
  auto raw_intf = mockup.RedfishClientInterface();
  RedfishVariant chassis = raw_intf->GetRoot();

  EXPECT_EQ(R"({
 "@odata.context": "/redfish/v1/$metadata#ServiceRoot.ServiceRoot",
 "@odata.id": "/redfish/v1",
 "@odata.type": "#ServiceRoot.v1_5_0.ServiceRoot",
 "Chassis": {
  "@odata.id": "/redfish/v1/Chassis"
 },
 "EventService": {
  "@odata.id": "/redfish/v1/EventService"
 },
 "Id": "RootService",
 "Links": {
  "Sessions": {
   "@odata.id": "/redfish/v1/SessionService/Sessions"
  }
 },
 "Name": "Root Service",
 "RedfishVersion": "1.6.1"
})",
            chassis.DebugString());
}

TEST(RedfishVariant, IndexingOk) {
  ecclesia::FakeRedfishServer mockup("barebones_session_auth/mockup.shar");
  auto raw_intf = mockup.RedfishClientInterface();

  auto chassis = raw_intf->GetRoot()[kRfPropertyChassis][0];
  EXPECT_THAT(chassis.status(), ecclesia::IsOk());
  EXPECT_THAT(chassis.httpcode(),
              ecclesia::HttpResponseCode::HTTP_CODE_REQUEST_OK);

  EXPECT_THAT(chassis.httpheaders(),
              Optional(Contains(Pair("OData-Version", "4.0"))));
  auto obj = chassis.AsObject();
  ASSERT_THAT(obj, Ne(nullptr));
  EXPECT_THAT(obj->GetNodeValue<PropertyName>(), Eq("chassis"));
  std::string state;
  EXPECT_TRUE(chassis[kRfPropertyStatus][kRfPropertyState].GetValue(&state));
  EXPECT_THAT(state, Eq("StandbyOffline"));
}

TEST(RedfishVariant, IndexingError) {
  ecclesia::FakeRedfishServer mockup("barebones_session_auth/mockup.shar");
  auto raw_intf = mockup.RedfishClientInterface();

  mockup.AddHttpGetHandler("/redfish/v1/Chassis/chassis",
                           [](ServerRequestInterface *req) {
                             req->ReplyWithStatus(HTTPStatusCode::IM_A_TEAPOT);
                           });

  // Querying the actual element [0] should produce the mocked failure.
  auto chassis = raw_intf->GetRoot()[kRfPropertyChassis][0];
  EXPECT_THAT(chassis.status(), IsStatusFailedPrecondition());
  EXPECT_THAT(chassis.status().message(), Eq("I'm a Teapot"));
  EXPECT_THAT(chassis.httpcode(),
              ecclesia::HttpResponseCode::HTTP_CODE_IM_A_TEAPOT);
  EXPECT_THAT(chassis.AsObject(), Eq(nullptr));
  EXPECT_THAT(chassis.AsIterable(), Eq(nullptr));

  // Any downstream element from the failing element should also have the same
  // error.
  auto failed_prop = raw_intf->GetRoot()[kRfPropertyChassis][0]["Property"];
  EXPECT_THAT(failed_prop.status(), IsStatusFailedPrecondition());
  EXPECT_THAT(failed_prop.status().message(), Eq("I'm a Teapot"));
  EXPECT_THAT(failed_prop.httpcode(),
              ecclesia::HttpResponseCode::HTTP_CODE_IM_A_TEAPOT);
  EXPECT_THAT(failed_prop.AsObject(), Eq(nullptr));
  EXPECT_THAT(failed_prop.AsIterable(), Eq(nullptr));

  auto failed_index = raw_intf->GetRoot()[kRfPropertyChassis][0][0];
  EXPECT_THAT(failed_index.status(), IsStatusFailedPrecondition());
  EXPECT_THAT(failed_index.status().message(), Eq("I'm a Teapot"));
  EXPECT_THAT(failed_index.httpcode(),
              ecclesia::HttpResponseCode::HTTP_CODE_IM_A_TEAPOT);
  EXPECT_THAT(failed_index.AsObject(), Eq(nullptr));
  EXPECT_THAT(failed_index.AsIterable(), Eq(nullptr));
}

TEST(RedfishVariant, PropertyError) {
  ecclesia::FakeRedfishServer mockup("barebones_session_auth/mockup.shar");
  auto raw_intf = mockup.RedfishClientInterface();

  mockup.AddHttpGetHandler("/redfish/v1/Chassis",
                           [](ServerRequestInterface *req) {
                             req->ReplyWithStatus(HTTPStatusCode::IM_A_TEAPOT);
                           });

  // Querying the actual element should produce the mocked failure.
  auto chassis_collection = raw_intf->GetRoot()[kRfPropertyChassis];
  EXPECT_THAT(chassis_collection.status(), IsStatusFailedPrecondition());
  EXPECT_THAT(chassis_collection.status().message(), Eq("I'm a Teapot"));
  EXPECT_THAT(chassis_collection.httpcode(),
              ecclesia::HttpResponseCode::HTTP_CODE_IM_A_TEAPOT);
  EXPECT_THAT(chassis_collection.AsObject(), Eq(nullptr));
  EXPECT_THAT(chassis_collection.AsIterable(), Eq(nullptr));

  // Any downsteam element from the failing element should also have the same
  // error.
  auto failed_prop = raw_intf->GetRoot()[kRfPropertyChassis]["Property"];
  EXPECT_THAT(failed_prop.status(), IsStatusFailedPrecondition());
  EXPECT_THAT(failed_prop.status().message(), Eq("I'm a Teapot"));
  EXPECT_THAT(failed_prop.httpcode(),
              ecclesia::HttpResponseCode::HTTP_CODE_IM_A_TEAPOT);
  EXPECT_THAT(failed_prop.AsObject(), Eq(nullptr));
  EXPECT_THAT(failed_prop.AsIterable(), Eq(nullptr));

  auto failed_index = raw_intf->GetRoot()[kRfPropertyChassis][0];
  EXPECT_THAT(failed_index.status(), IsStatusFailedPrecondition());
  EXPECT_THAT(failed_index.status().message(), Eq("I'm a Teapot"));
  EXPECT_THAT(failed_index.httpcode(),
              ecclesia::HttpResponseCode::HTTP_CODE_IM_A_TEAPOT);
  EXPECT_THAT(failed_index.AsObject(), Eq(nullptr));
  EXPECT_THAT(failed_index.AsIterable(), Eq(nullptr));
}

TEST(RedfishVariant, RedfishQueryParamExpand) {
  EXPECT_EQ(
      RedfishQueryParamExpand(
          {.type = RedfishQueryParamExpand::ExpandType::kBoth, .levels = 1})
          .ToString(),
      "$expand=*($levels=1)");

  EXPECT_EQ(
      RedfishQueryParamExpand(
          {.type = RedfishQueryParamExpand::ExpandType::kLinks, .levels = 2})
          .ToString(),
      "$expand=~($levels=2)");

  EXPECT_EQ(
      RedfishQueryParamExpand(
          {.type = RedfishQueryParamExpand::ExpandType::kNotLinks, .levels = 3})
          .ToString(),
      "$expand=.($levels=3)");
}

TEST(RedfishVariant, RedfishQueryParamFilter) {
  std::string filter_string1 = "Prop1%20eq%2042";
  auto filter = RedfishQueryParamFilter(filter_string1);
  EXPECT_EQ(filter.ToString(), absl::StrCat("$filter=", filter_string1));
  std::string filter_string2 = "Prop1%20eq%2084";
  filter.SetFilterString(filter_string2);
  EXPECT_EQ(filter.ToString(), absl::StrCat("$filter=", filter_string2));
}

TEST(RedfishVariant, ValidateRedfishSupportSuccess) {
  EXPECT_THAT(
      RedfishQueryParamExpand(
          {.type = RedfishQueryParamExpand::ExpandType::kBoth, .levels = 1})
          .ValidateRedfishSupport(std::nullopt),
      ecclesia::IsStatusInternal());
  // Test successful scenarios
  EXPECT_THAT(
      RedfishQueryParamExpand(
          {.type = RedfishQueryParamExpand::ExpandType::kBoth, .levels = 1})
          .ValidateRedfishSupport(RedfishSupportedFeatures{
              .expand = {.expand_all = true, .max_levels = 1}}),
      ecclesia::IsOk());
  EXPECT_THAT(
      RedfishQueryParamExpand(
          {.type = RedfishQueryParamExpand::ExpandType::kLinks, .levels = 2})
          .ValidateRedfishSupport(RedfishSupportedFeatures{
              .expand = {.links = true, .max_levels = 3}}),
      ecclesia::IsOk());
  EXPECT_THAT(
      RedfishQueryParamExpand(
          {.type = RedfishQueryParamExpand::ExpandType::kNotLinks, .levels = 1})
          .ValidateRedfishSupport(RedfishSupportedFeatures{
              .expand = {.no_links = true, .max_levels = 1}}),
      ecclesia::IsOk());
}

TEST(RedfishVariant, ValidateRedfishSupportFailures) {
  EXPECT_THAT(
      RedfishQueryParamExpand(
          {.type = RedfishQueryParamExpand::ExpandType::kBoth, .levels = 1})
          .ValidateRedfishSupport(RedfishSupportedFeatures{
              .expand = {.expand_all = false, .max_levels = 1}}),
      ecclesia::IsStatusInternal());
  EXPECT_THAT(
      RedfishQueryParamExpand(
          {.type = RedfishQueryParamExpand::ExpandType::kLinks, .levels = 2})
          .ValidateRedfishSupport(RedfishSupportedFeatures{
              .expand = {.links = false, .max_levels = 3}}),
      ecclesia::IsStatusInternal());
  EXPECT_THAT(
      RedfishQueryParamExpand(
          {.type = RedfishQueryParamExpand::ExpandType::kNotLinks, .levels = 1})
          .ValidateRedfishSupport(RedfishSupportedFeatures{
              .expand = {.no_links = false, .max_levels = 1}}),
      ecclesia::IsStatusInternal());
  // Validate levels are controlled
  EXPECT_THAT(
      RedfishQueryParamExpand(
          {.type = RedfishQueryParamExpand::ExpandType::kNotLinks, .levels = 2})
          .ValidateRedfishSupport(RedfishSupportedFeatures{
              .expand = {.no_links = true, .max_levels = 1}}),
      ecclesia::IsStatusInternal());
}

// Test the default behavior of RedfishInterface regarding the other non-pure
// virtual interfaces.
TEST(RedfishInterface, DefaultBehavior) {
  std::unique_ptr<RedfishInterface> interface = std::make_unique<NullRedfish>();
  EXPECT_EQ(interface->SupportedFeatures(), std::nullopt);

  absl::AnyInvocable<void(const RedfishVariant &) const> on_event =
      [](const RedfishVariant &) {};
  absl::AnyInvocable<void(const absl::Status &) const> on_stop =
      [](const absl::Status &) {};

  EXPECT_THAT(interface->Subscribe("", on_event, on_stop),
              ecclesia::IsStatusUnimplemented());
}

TEST(RedfishVariant, RedfishQueryParamTop) {
  EXPECT_EQ(RedfishQueryParamTop(/*numMembers*/ 100).ToString(), "$top=100");
}

TEST(RedfishVariant, ValidateRedfishTopSupportSuccess) {
  // Test successful scenarios
  EXPECT_THAT(
      RedfishQueryParamTop(/*num_members*/ 1)
          .ValidateRedfishSupport(RedfishSupportedFeatures{
            .top_skip = {.enable = true}}),
      ecclesia::IsOk());
}

TEST(RedfishVariant, ValidateRedfishTopSupportFail) {
  // Test failure scenarios
  EXPECT_THAT(
      RedfishQueryParamTop(/*num_members*/ 100)
          .ValidateRedfishSupport(std::nullopt),
      ecclesia::IsStatusInternal());
}

}  // namespace
}  // namespace ecclesia
