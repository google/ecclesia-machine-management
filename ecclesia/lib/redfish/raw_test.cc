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

#include <cstdint>
#include <memory>
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
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

enum class InterfaceType { kNoAuth, kBasicAuth, kSessionAuth, kTlsAuth };

absl::optional<absl::string_view> InterfaceTypeToString(
    InterfaceType interface_type) {
  switch (interface_type) {
    case InterfaceType::kBasicAuth:
      return "BasicAuth";
    case InterfaceType::kSessionAuth:
      return "SessionAuth";
    case InterfaceType::kNoAuth:
      return "NoAuth";
    case InterfaceType::kTlsAuth:
      return "TlsAuth";
    default:
      // The interface type is not supported yet!
      return absl::nullopt;
  }
}

std::unique_ptr<TestingMockupServer> GetTlsServer() {
  return absl::make_unique<TestingMockupServer>(
      "indus_hmb_cn/mockup.shar",
      TestingMockupServer::ServerTlsConfig{
          .cert_file =
              ecclesia::GetTestDataDependencyPath(kMockupServerCertFile),
          .key_file = ecclesia::GetTestDataDependencyPath(kMockupServerKeyFile),
          .ca_cert_file =
              ecclesia::GetTestDataDependencyPath(kMockupServerCAFile)},
      TestingMockupServer::ClientTlsConfig{
          .verify_peer = true,
          .verify_hostname = false,
          .cert_file = ecclesia::GetTestDataDependencyPath(kClientCertFile),
          .key_file = ecclesia::GetTestDataDependencyPath(kClientKeyFile),
          .ca_cert_file = ecclesia::GetTestDataDependencyPath(kClientCAFile)});
}

// The class converts default integer based test names to meaningful auth type
// based names in value-parameterized tests.
struct PrintToStringParamName {
  template <class ParamType>
  std::string operator()(
      const ::testing::TestParamInfo<ParamType>& info) const {
    auto res = InterfaceTypeToString(info.param);
    ecclesia::Check(res.has_value(), "the interface is supported");
    return std::string(*res);
  }
};

// The class handles non value-parameterized tests.
class RawInterfaceTest : public ::testing::Test {};

TEST_F(RawInterfaceTest, FailedConnectionReturnsNullInterface) {
  auto redfish_intf =
      NewRawInterface("bad_endpoint", RedfishInterface::kTrusted);
  ASSERT_TRUE(redfish_intf);
  auto root = redfish_intf->GetRoot();
  EXPECT_FALSE(root.AsObject());
  EXPECT_FALSE(root.AsIterable());
}

// In real BmcWeb, Redfish root will always be available even if
// authentication fails; but for simpilicity, the mockup server return errors
// on every URL when TlsAuth fails.
TEST_F(RawInterfaceTest, ClientsWithoutCertsAreRejected) {
  auto mockup_server = GetTlsServer();
  auto config =
      absl::get<TestingMockupServer::ConfigNetwork>(mockup_server->GetConfig());
  auto endpoint = absl::StrCat("https://", config.hostname, ":", config.port);
  auto interface = NewRawInterface(endpoint, RedfishInterface::kTrusted);
  EXPECT_THAT(interface->GetRoot().AsObject(), IsNull());
}

TEST_F(RawInterfaceTest, ClientsWithoutProperCertsAreRejected) {
  auto mockup_server = GetTlsServer();
  auto config =
      absl::get<TestingMockupServer::ConfigNetwork>(mockup_server->GetConfig());
  TlsArgs args;
  args.endpoint = absl::StrCat("https://", config.hostname, ":", config.port);
  args.verify_hostname = false;
  args.verify_peer = false;
  args.cert_file = kMockupServerCertFile;
  args.key_file = kMockupServerKeyFile;
  auto interface = NewRawTlsAuthInterface(args);
  EXPECT_THAT(interface->GetRoot().AsObject(), IsNull());
}

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
      public testing::WithParamInterface<InterfaceType> {
 protected:
  RawInterfaceWithParamTest() {}
  // Sets up raw_intf_ based on auth types
  void SetUp() {
    switch (GetParam()) {
      case InterfaceType::kBasicAuth:
        mockup_server_ = absl::make_unique<TestingMockupServer>(
            "barebones_session_auth/mockup.shar");
        raw_intf_ = mockup_server_->RedfishClientBasicAuthInterface();
        break;
      case InterfaceType::kSessionAuth:
        mockup_server_ = absl::make_unique<TestingMockupServer>(
            "barebones_session_auth/mockup.shar");
        raw_intf_ = mockup_server_->RedfishClientSessionAuthInterface();
        break;
      case InterfaceType::kTlsAuth:
        mockup_server_ = GetTlsServer();
        raw_intf_ = mockup_server_->RedfishClientTlsAuthInterface();
        break;
      default:
        mockup_server_ =
            absl::make_unique<TestingMockupServer>("indus_hmb_cn/mockup.shar");
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

  auto res = raw_intf_
                 ->PostUri("/redfish/v1/Chassis",
                           {
                               {"key1", 1},
                               {"key2", 1.3},
                               {"key3", "test"},
                               {"key4", true},
                               {"key5", std::string("value5")},
                           })
                 .AsObject();
  // After propagate status code to RedfishInvariant, add test to verify
  // status code for different type of resources. For example, for resource
  // support Action, return 204, others return 404.
  // For 204 response, there is no payload. So this will return empty
  // RedfishInvariant. The following ASSERT will fail.
  // ASSERT_TRUE(res);

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

  auto res = raw_intf_
                 ->PostUri("/redfish/v1/Chassis",
                           "{"
                           "\"key1\": 1,"
                           "\"key2\": 1.3,"
                           "\"key3\": \"test\","
                           "\"key4\": true"
                           "}")
                 .AsObject();
  // After propagate status code to RedfishInvariant, add test to verify
  // status code for different type of resources. For example, for resource
  // support Action, return 204, others return 404.
  // For 204 response, there is no payload. So this will return empty
  // RedfishInvariant. The following ASSERT will fail.
  // ASSERT_TRUE(res);

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

INSTANTIATE_TEST_SUITE_P(Interfaces, RawInterfaceWithParamTest,
                         testing::Values(InterfaceType::kNoAuth,
                                         InterfaceType::kBasicAuth,
                                         InterfaceType::kSessionAuth,
                                         InterfaceType::kTlsAuth),
                         PrintToStringParamName());
}  // namespace
}  // namespace libredfish
