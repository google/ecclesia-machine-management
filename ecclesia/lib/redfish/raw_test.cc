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

#include "ecclesia/lib/redfish/raw.h"

#include <memory>
#include <optional>
#include <string>
#include <variant>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/test_mockup.h"

namespace libredfish {
namespace {

using ::testing::Eq;
using ::testing::IsNull;

constexpr absl::string_view kMockupServerCertFile =
    "lib/redfish/testing/cert/server_cert.crt";
constexpr absl::string_view kMockupServerKeyFile =
    "lib/redfish/testing/cert/server_cert.key";
constexpr absl::string_view kMockupServerCAFile =
    "lib/redfish/testing/cert/ca_client.crt";
constexpr absl::string_view kClientCertFile =
    "lib/redfish/testing/cert/client_cert.crt";
constexpr absl::string_view kClientKeyFile =
    "lib/redfish/testing/cert/client_cert.key";
constexpr absl::string_view kClientCAFile =
    "lib/redfish/testing/cert/ca_server.crt";

enum class BackendType { kDefault, kEcclesiaCurl };

absl::string_view BackendTypeToString(BackendType backend_type) {
  switch (backend_type) {
    case BackendType::kDefault:
      return "Default";
    case BackendType::kEcclesiaCurl:
      return "EcclesiaCurl";
  }
}

enum class InterfaceType { kNoAuth, kSessionAuth };

std::optional<absl::string_view> InterfaceTypeToString(
    InterfaceType interface_type) {
  switch (interface_type) {
    case InterfaceType::kSessionAuth:
      return "SessionAuth";
    case InterfaceType::kNoAuth:
      return "NoAuth";
    default:
      // The interface type is not supported yet!
      return std::nullopt;
  }
}

struct RawTestConfig {
  BackendType backend_type;
  InterfaceType interface_type;
};

const RawTestConfig kRawTestCases[] = {
    {BackendType::kDefault, InterfaceType::kNoAuth},
    {BackendType::kDefault, InterfaceType::kSessionAuth},
    {BackendType::kEcclesiaCurl, InterfaceType::kNoAuth},
};

// The class converts default integer based test names to meaningful auth type
// based names in value-parameterized tests.
struct PrintToStringParamName {
  template <class ParamType>
  std::string operator()(
      const ::testing::TestParamInfo<ParamType> &info) const {
    auto interface = InterfaceTypeToString(info.param.interface_type);
    ecclesia::Check(interface.has_value(), "the interface is supported");
    return std::string(BackendTypeToString(info.param.backend_type)) + "_" +
           std::string(*interface);
  }
};

// The class handles non value-parameterized tests.
class RawInterfaceTest : public ::testing::Test {};

// Todo(nanzhou) add this test case into RawInterfaceWithParamTest once
// indus_hmb_cn_mockup has SessionService
TEST_F(RawInterfaceTest, GetFragmentUriMatches) {
  auto mockup_server = TestingMockupServer("indus_hmb_cn/mockup.shar");
  auto raw_intf = mockup_server.RedfishClientInterface();
  auto root = raw_intf->GetRoot().AsObject();
  ASSERT_TRUE(root);
  auto chassis = (*root)["Chassis"].AsIterable();
  ASSERT_TRUE(chassis);
  auto indus = (*chassis)[0].AsObject();
  ASSERT_TRUE(indus);
  auto indus_assembly = (*indus)["Assembly"].AsObject();
  ASSERT_TRUE(indus_assembly);
  auto assemblies = (*indus_assembly)["Assemblies"].AsIterable();
  ASSERT_TRUE(assemblies);
  auto assembly = (*assemblies)[0].AsObject();
  ASSERT_TRUE(assembly);

  auto assembly_via_uri =
      raw_intf->GetUri("/redfish/v1/Chassis/chassis/Assembly#/Assemblies/0")
          .AsObject();
  ASSERT_TRUE(assembly_via_uri);

  EXPECT_THAT(assembly->GetUri(), Eq(assembly_via_uri->GetUri()));
}

// The class handles value-parameterized tests.
class RawInterfaceWithParamTest
    : public ::testing::Test,
      public testing::WithParamInterface<RawTestConfig> {
 protected:
  RawInterfaceWithParamTest() {}
  // Sets up raw_intf_ based on auth types
  void SetUp() {
    switch (GetParam().interface_type) {
      case InterfaceType::kSessionAuth:
        mockup_server_ = std::make_unique<TestingMockupServer>(
            "barebones_session_auth/mockup.shar");
        raw_intf_ = mockup_server_->RedfishClientSessionAuthInterface();
        break;
      default:
        mockup_server_ =
            std::make_unique<TestingMockupServer>("indus_hmb_cn/mockup.shar");
        raw_intf_ = mockup_server_->RedfishClientInterface();
    }
  }

  std::unique_ptr<TestingMockupServer> mockup_server_;
  std::unique_ptr<RedfishInterface> raw_intf_;
};

TEST_P(RawInterfaceWithParamTest, GetUriMatchesGetRoot) {
  auto root = raw_intf_->GetRoot().AsObject();
  auto root_via_uri = raw_intf_->GetUri("/redfish/v1").AsObject();
  auto root_via_uri2 = raw_intf_->GetUri("/redfish/v1/").AsObject();
  ASSERT_TRUE(root);
  ASSERT_TRUE(root_via_uri);
  ASSERT_TRUE(root_via_uri2);
  EXPECT_THAT(root->GetUri(), Eq(root_via_uri->GetUri()));
  EXPECT_THAT(root->GetUri(), Eq(root_via_uri2->GetUri()));
}
/*
TEST_P(RawInterfaceWithParamTest, DebugStringIsValid) {
  auto root = raw_intf_->GetRoot().AsObject();
  ASSERT_TRUE(root);
  std::string root_debug_str = root->DebugString();
  ASSERT_FALSE(root_debug_str.empty());
  auto root_uri = root->GetUri();
  ASSERT_TRUE(root_uri.has_value());

  EXPECT_NE(root_debug_str.find(root_uri.value()), std::string::npos);
}

TEST_P(RawInterfaceWithParamTest, GetChildObjectByUriMatches) {
  auto root = raw_intf_->GetRoot().AsObject();
  ASSERT_TRUE(root);
  auto chassis = (*root)["Chassis"].AsObject();
  ASSERT_TRUE(chassis);

  auto chassis_via_uri = raw_intf_->GetUri("/redfish/v1/Chassis").AsObject();
  ASSERT_TRUE(chassis_via_uri);

  EXPECT_THAT(chassis->GetUri(), Eq(chassis_via_uri->GetUri()));
}

TEST_P(RawInterfaceWithParamTest, GetIndusObjectByUriMatches) {
  auto root = raw_intf_->GetRoot().AsObject();
  ASSERT_TRUE(root);
  auto chassis = (*root)["Chassis"].AsIterable();
  ASSERT_TRUE(chassis);
  auto indus = (*chassis)[0].AsObject();
  ASSERT_TRUE(indus);

  auto indus_via_uri =
      raw_intf_->GetUri("/redfish/v1/Chassis/chassis").AsObject();
  ASSERT_TRUE(indus_via_uri);

  EXPECT_THAT(indus->GetUri(), Eq(indus_via_uri->GetUri()));
}

TEST_P(RawInterfaceWithParamTest, PostUri) {
  auto origin_collection =
      raw_intf_->GetUri("/redfish/v1/Chassis").AsIterable();
  ASSERT_TRUE(origin_collection);
  auto origin_size = origin_collection->Size();

  auto res = raw_intf_->PostUri("/redfish/v1/Chassis",
                                {
                                    {"key1", 1},
                                    {"key2", 1.3},
                                    {"key3", "test"},
                                    {"key4", true},
                                    {"key5", std::string("value5")},
                                });
  EXPECT_TRUE(res.status().ok()) << res.status().message();

  auto new_collection = raw_intf_->GetUri("/redfish/v1/Chassis").AsIterable();
  ASSERT_TRUE(origin_collection);
  auto new_size = new_collection->Size();
  EXPECT_EQ(new_size - origin_size, 1);
  auto new_chassis =
      (*new_collection)[static_cast<int>(new_size - 1)].AsObject();

  EXPECT_EQ(new_chassis->GetNodeValue<int32_t>("key1").value_or(0), 1);
  EXPECT_EQ(new_chassis->GetNodeValue<double>("key2").value_or(0.0), 1.3);
  EXPECT_EQ(new_chassis->GetNodeValue<std::string>("key3").value_or(""),
            "test");
  EXPECT_EQ(new_chassis->GetNodeValue<bool>("key4").value_or(false), true);
  EXPECT_EQ(new_chassis->GetNodeValue<std::string>("key5").value_or(""),
            "value5");
}

TEST_P(RawInterfaceWithParamTest, PostUriWithStringPayload) {
  auto origin_collection =
      raw_intf_->GetUri("/redfish/v1/Chassis").AsIterable();
  ASSERT_TRUE(origin_collection);
  auto origin_size = origin_collection->Size();

  auto res = raw_intf_->PostUri("/redfish/v1/Chassis",
                                "{"
                                "\"key1\": 1,"
                                "\"key2\": 1.3,"
                                "\"key3\": \"test\","
                                "\"key4\": true"
                                "}");
  EXPECT_TRUE(res.status().ok()) << res.status().message();

  auto new_collection = raw_intf_->GetUri("/redfish/v1/Chassis").AsIterable();
  ASSERT_TRUE(origin_collection);
  auto new_size = new_collection->Size();
  EXPECT_EQ(new_size - origin_size, 1);
  auto new_chassis =
      (*new_collection)[static_cast<int>(new_size - 1)].AsObject();

  EXPECT_EQ(new_chassis->GetNodeValue<int32_t>("key1").value_or(0), 1);
  EXPECT_EQ(new_chassis->GetNodeValue<double>("key2").value_or(0.0), 1.3);
  EXPECT_EQ(new_chassis->GetNodeValue<std::string>("key3").value_or(""),
            "test");
  EXPECT_EQ(new_chassis->GetNodeValue<bool>("key4").value_or(false), true);
}

TEST_P(RawInterfaceWithParamTest, PatchUri) {
  auto root_chassis =
      raw_intf_->GetUri("/redfish/v1/Chassis/chassis").AsObject();
  ASSERT_TRUE(root_chassis);

  auto res = raw_intf_->PatchUri("/redfish/v1/Chassis/chassis",
                                 {{"Name", "testname"}});
  EXPECT_TRUE(res.status().ok()) << res.status().message();

  auto new_root_chassis =
      raw_intf_->GetUri("/redfish/v1/Chassis/chassis").AsObject();
  ASSERT_TRUE(new_root_chassis);

  EXPECT_THAT(new_root_chassis->GetNodeValue<libredfish::PropertyName>(),
              Eq("testname"));
}

TEST_P(RawInterfaceWithParamTest, PatchUriMultipleFields) {
  auto root_chassis =
      raw_intf_->GetUri("/redfish/v1/Chassis/chassis").AsObject();
  ASSERT_TRUE(root_chassis);

  auto res = raw_intf_->PatchUri("/redfish/v1/Chassis/chassis",
                                 {{"Name", "testname"}, {"Id", "testid"}});
  EXPECT_TRUE(res.status().ok()) << res.status().message();

  auto new_root_chassis =
      raw_intf_->GetUri("/redfish/v1/Chassis/chassis").AsObject();
  ASSERT_TRUE(new_root_chassis);

  EXPECT_THAT(new_root_chassis->GetNodeValue<libredfish::PropertyName>(),
              Eq("testname"));
  EXPECT_THAT(new_root_chassis->GetNodeValue<libredfish::PropertyId>(),
              Eq("testid"));
}

TEST_P(RawInterfaceWithParamTest, PatchBadUri) {
  auto res = raw_intf_->PatchUri("/redfish/v1/Not/A/Uri",
                                 {{"Name", "testname"}, {"Id", "testid"}});
  EXPECT_THAT(res.status().code(), Eq(absl::StatusCode::kNotFound));
}
*/
TEST_P(RawInterfaceWithParamTest, ParseDateTime) {
  auto res = raw_intf_->PostUri("/redfish/v1/Chassis",
                                "{"
                                "\"DateTime\": \"2020-12-21T12:34:56+00:00\""
                                "}");
  EXPECT_TRUE(res.status().ok()) << res.status().message();

  auto chassis_collection =
      raw_intf_->GetUri("/redfish/v1/Chassis").AsIterable();
  auto chassis =
      (*chassis_collection)[static_cast<int>(chassis_collection->Size() - 1)];

  auto datetime_variant = chassis["DateTime"];
  ASSERT_TRUE(datetime_variant.status().ok());

  absl::Time datetime;
  ASSERT_TRUE(datetime_variant.GetValue(&datetime));

  absl::TimeZone utc;
  ASSERT_TRUE(absl::LoadTimeZone("UTC", &utc));

  absl::Time datetime_gold = absl::FromDateTime(2020, 12, 21, 12, 34, 56, utc);
  ASSERT_EQ(datetime, datetime_gold);
}

INSTANTIATE_TEST_SUITE_P(RawTests, RawInterfaceWithParamTest,
                         testing::ValuesIn(kRawTestCases),
                         PrintToStringParamName());

}  // namespace
}  // namespace libredfish
