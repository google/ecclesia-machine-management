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

#include "ecclesia/lib/redfish/manager.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "google/protobuf/duration.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/statusor.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/property.h"
#include "ecclesia/lib/redfish/property_definitions.h"
#include "ecclesia/lib/redfish/test_mockup.h"
#include "ecclesia/lib/testing/proto.h"

namespace ecclesia {
namespace {

TEST(RedfishInterface, GetManagerForRoot) {
  TestingMockupServer mockup("features/managers/mockup.shar");
  std::unique_ptr<RedfishInterface> rf_intf = mockup.RedfishClientInterface();

  absl::StatusOr<std::unique_ptr<RedfishObject>> manager =
      GetManagerForRoot(rf_intf->GetRoot());
  ASSERT_TRUE(manager.ok());
  std::optional<std::string> entry_point_uuid =
      (*manager)->GetNodeValue<PropertyServiceEntryPointUuid>();
  ASSERT_TRUE(entry_point_uuid.has_value());
  std::unique_ptr<RedfishObject> root = rf_intf->GetRoot().AsObject();
  ASSERT_NE(root, nullptr);
  EXPECT_EQ(entry_point_uuid, root->GetNodeValue<PropertyUuid>());
}

TEST(RedfishInterface, GetUptimeForManager) {
  TestingMockupServer mockup("features/managers/mockup.shar");
  std::unique_ptr<RedfishInterface> rf_intf = mockup.RedfishClientInterface();

  absl::StatusOr<std::unique_ptr<RedfishObject>> manager =
      GetManagerForRoot(rf_intf->GetRoot());
  ASSERT_TRUE(manager.ok());
  absl::StatusOr<google::protobuf::Duration> uptime =
      GetUptimeForManager(*std::move(manager));
  ASSERT_TRUE(uptime.ok());
  EXPECT_THAT(*uptime, EqualsProto(R"pb(seconds: 974173)pb"));
}

}  // namespace
}  // namespace ecclesia
