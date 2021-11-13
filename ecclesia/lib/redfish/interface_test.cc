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

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/http/codes.h"
#include "ecclesia/lib/redfish/property_definitions.h"
#include "ecclesia/lib/redfish/testing/fake_redfish_server.h"
#include "ecclesia/lib/testing/status.h"
#include "tensorflow_serving/util/net_http/server/public/response_code_enum.h"

namespace libredfish {
namespace {

using ::testing::Eq;
using ::testing::Ne;
using ::ecclesia::IsOk;
using ::ecclesia::IsStatusFailedPrecondition;

using ::tensorflow::serving::net_http::HTTPStatusCode;
using ::tensorflow::serving::net_http::ServerRequestInterface;

TEST(RedfishVariant, IndexingOk) {
  ecclesia::FakeRedfishServer mockup(
      "barebones_session_auth/mockup.shar",
      absl::StrCat(ecclesia::GetTestTempUdsDirectory(), "/mockup.socket"));
  auto raw_intf = mockup.RedfishClientInterface();

  auto chassis = raw_intf->GetRoot()[kRfPropertyChassis][0];
  EXPECT_THAT(chassis.status(), ecclesia::IsOk());
  EXPECT_THAT(chassis.httpcode(),
              ecclesia::HttpResponseCode::HTTP_CODE_REQUEST_OK);
  auto obj = chassis.AsObject();
  ASSERT_THAT(obj, Ne(nullptr));
  EXPECT_THAT(obj->GetNodeValue<libredfish::PropertyName>(), Eq("chassis"));
  std::string state;
  EXPECT_TRUE(chassis[kRfPropertyStatus][kRfPropertyState].GetValue(&state));
  EXPECT_THAT(state, Eq("StandbyOffline"));
}

TEST(RedfishVariant, IndexingError) {
  ecclesia::FakeRedfishServer mockup(
      "barebones_session_auth/mockup.shar",
      absl::StrCat(ecclesia::GetTestTempUdsDirectory(), "/mockup.socket"));
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

  // Any downsteam element from the failing element should also have the same
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
  ecclesia::FakeRedfishServer mockup(
      "barebones_session_auth/mockup.shar",
      absl::StrCat(ecclesia::GetTestTempUdsDirectory(), "/mockup.socket"));
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

}  // namespace
}  // namespace libredfish