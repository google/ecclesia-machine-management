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
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/http/codes.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/property_definitions.h"
#include "ecclesia/lib/redfish/test_mockup.h"
#include "ecclesia/magent/daemons/common.h"
#include "tensorflow_serving/util/net_http/server/public/httpserver_interface.h"

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

struct RawTestConfig {
  BackendType backend_type;
  InterfaceType interface_type;
};

const RawTestConfig kRawTestCases[] = {
    {BackendType::kDefault, InterfaceType::kNoAuth},
    {BackendType::kDefault, InterfaceType::kBasicAuth},
    {BackendType::kDefault, InterfaceType::kSessionAuth},
    {BackendType::kDefault, InterfaceType::kTlsAuth},
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
      public testing::WithParamInterface<RawTestConfig> {
 protected:
  RawInterfaceWithParamTest() {}
  // Sets up raw_intf_ based on auth types
  void SetUp() {
    switch (GetParam().interface_type) {
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

// The class exposes an HTTP server where custom handlers can be implemented.
class ManualServerTest : public ::testing::Test {
 protected:
  ManualServerTest() : server_(ecclesia::CreateServer(0)) {
    server_->StartAcceptingRequests();
    intf_ = libredfish::NewRawInterface(
        absl::StrCat("http://localhost:", server_->listen_port()),
        RedfishInterface::kTrusted);
  }
  ~ManualServerTest() {
    if (server_) {
      server_->Terminate();
      server_->WaitForTermination();
    }
  }
  std::unique_ptr<tensorflow::serving::net_http::HTTPServerInterface> server_;
  std::unique_ptr<libredfish::RedfishInterface> intf_;
};

// The following test cases: see Redfish Specification (2021.1)
// Section 7.5.1 Modification success responses
TEST_F(ManualServerTest, PatchOk200) {
  server_->RegisterRequestHandler(
      "/redfish/v1/Chassis",
      [](tensorflow::serving::net_http::ServerRequestInterface *req) {
        EXPECT_THAT(req->http_method(), Eq("PATCH"));
        tensorflow::serving::net_http::SetContentType(req, "application/json");
        req->WriteResponseString(R"json({ "Name": "Hello" })json");
        req->ReplyWithStatus(tensorflow::serving::net_http::HTTPStatusCode::OK);
      },
      tensorflow::serving::net_http::RequestHandlerOptions());

  auto patch = intf_->PatchUri("/redfish/v1/Chassis", {{"Name", "Hello"}});
  EXPECT_THAT(patch.httpcode(), Eq(ecclesia::HTTP_CODE_REQUEST_OK));
  EXPECT_TRUE(patch.status().ok()) << patch.status().message();
  auto obj = patch.AsObject();
  ASSERT_TRUE(obj);
  EXPECT_THAT(obj->GetNodeValue<libredfish::PropertyName>(), "Hello");
}

TEST_F(ManualServerTest, PatchOk200WithExtendedInfo) {
  server_->RegisterRequestHandler(
      "/redfish/v1/Chassis",
      [](tensorflow::serving::net_http::ServerRequestInterface *req) {
        EXPECT_THAT(req->http_method(), Eq("PATCH"));
        tensorflow::serving::net_http::SetContentType(req, "application/json");
        req->WriteResponseString(R"json({
          "Name": "Goodbye",
          "@Message.ExtendedInfo": [
            {
              "MessageId": "Base.1.0.PropertyDuplicate",
              "Message": "BaseIndicates that a duplicate property was included in the request body",
              "RelatedProperties": [
                "#/Name"
              ],
              "Severity": "Warning",
              "Resolution": "Remove the duplicate property from the request body."
            }
          ]
        })json");
        req->ReplyWithStatus(tensorflow::serving::net_http::HTTPStatusCode::OK);
      },
      tensorflow::serving::net_http::RequestHandlerOptions());

  auto patch = intf_->PatchUri("/redfish/v1/Chassis",
                               {{"Name", "Hello"}, {"Name", "Goodbye"}});
  EXPECT_THAT(patch.httpcode(), Eq(ecclesia::HTTP_CODE_REQUEST_OK));
  EXPECT_TRUE(patch.status().ok()) << patch.status().message();
  auto obj = patch.AsObject();
  ASSERT_TRUE(obj);
  EXPECT_THAT(obj->GetNodeValue<libredfish::PropertyName>(), "Goodbye");
}

TEST_F(ManualServerTest, PatchOk202) {
  server_->RegisterRequestHandler(
      "/redfish/v1/Chassis",
      [](tensorflow::serving::net_http::ServerRequestInterface *req) {
        EXPECT_THAT(req->http_method(), Eq("PATCH"));
        req->AppendResponseHeader("Location", "/upload_monitor");
        req->ReplyWithStatus(
            tensorflow::serving::net_http::HTTPStatusCode::ACCEPTED);
      },
      tensorflow::serving::net_http::RequestHandlerOptions());
  // 202 gets redirected:
  server_->RegisterRequestHandler(
      "/upload_monitor",
      [](tensorflow::serving::net_http::ServerRequestInterface *req) {
        EXPECT_THAT(req->http_method(), Eq("GET"));
        tensorflow::serving::net_http::SetContentType(req, "application/json");
        req->WriteResponseString(R"json({ "Name": "Hello" })json");
        req->ReplyWithStatus(tensorflow::serving::net_http::HTTPStatusCode::OK);
      },
      tensorflow::serving::net_http::RequestHandlerOptions());

  auto patch = intf_->PatchUri("/redfish/v1/Chassis", {{"Name", "Hello"}});
  EXPECT_THAT(patch.httpcode(), Eq(ecclesia::HTTP_CODE_REQUEST_OK));
  EXPECT_TRUE(patch.status().ok()) << patch.status().message();
  auto obj = patch.AsObject();
  ASSERT_TRUE(obj);
  EXPECT_THAT(obj->GetNodeValue<libredfish::PropertyName>(), "Hello");
}

TEST_F(ManualServerTest, PatchOk204) {
  server_->RegisterRequestHandler(
      "/redfish/v1/Chassis",
      [](tensorflow::serving::net_http::ServerRequestInterface *req) {
        EXPECT_THAT(req->http_method(), Eq("PATCH"));
        req->ReplyWithStatus(
            tensorflow::serving::net_http::HTTPStatusCode::NO_CONTENT);
      },
      tensorflow::serving::net_http::RequestHandlerOptions());

  auto patch = intf_->PatchUri("/redfish/v1/Chassis", {{"Name", "Hello"}});
  EXPECT_THAT(patch.httpcode(), Eq(ecclesia::HTTP_CODE_NO_CONTENT));
  EXPECT_TRUE(patch.status().ok()) << patch.status().message();
  auto obj = patch.AsObject();
  EXPECT_FALSE(obj);
}

TEST_F(ManualServerTest, Patch400WithExtendedInfo) {
  server_->RegisterRequestHandler(
      "/redfish/v1/Chassis",
      [](tensorflow::serving::net_http::ServerRequestInterface *req) {
        EXPECT_THAT(req->http_method(), Eq("PATCH"));
        tensorflow::serving::net_http::SetContentType(req, "application/json");
        req->WriteResponseString(R"json({
          "Name": "Static Name",
          "@Message.ExtendedInfo": [
            {
              "MessageId": "Base.1.0.PropertyNotWritable",
              "Message": "The property Name is a read only property and cannot be assigned a value.",
              "RelatedProperties": [
                "#/Name"
              ],
              "Severity": "Warning",
              "Resolution": "Remove the property from the request body and resubmit the request if the operation failed."
            }
          ]
        })json");
        req->ReplyWithStatus(
            tensorflow::serving::net_http::HTTPStatusCode::BAD_REQUEST);
      },
      tensorflow::serving::net_http::RequestHandlerOptions());

  auto patch = intf_->PatchUri("/redfish/v1/Chassis", {{"Name", "Hello"}});
  EXPECT_THAT(patch.httpcode(), Eq(ecclesia::HTTP_CODE_BAD_REQUEST));
  EXPECT_THAT(patch.status().code(), Eq(absl::StatusCode::kInvalidArgument))
      << patch.status().message();
  auto obj = patch.AsObject();
  ASSERT_TRUE(obj);
  EXPECT_THAT(obj->GetNodeValue<libredfish::PropertyName>(), "Static Name");
}

TEST_F(ManualServerTest, Patch405) {
  server_->RegisterRequestHandler(
      "/redfish/v1/Chassis",
      [](tensorflow::serving::net_http::ServerRequestInterface *req) {
        EXPECT_THAT(req->http_method(), Eq("PATCH"));
        req->ReplyWithStatus(
            tensorflow::serving::net_http::HTTPStatusCode::METHOD_NA);
      },
      tensorflow::serving::net_http::RequestHandlerOptions());

  auto patch = intf_->PatchUri("/redfish/v1/Chassis", {{"Name", "Hello"}});
  EXPECT_THAT(patch.httpcode(), Eq(ecclesia::HTTP_CODE_METHOD_NA));
  EXPECT_THAT(patch.status().code(), Eq(absl::StatusCode::kFailedPrecondition))
      << patch.status().message();
  auto obj = patch.AsObject();
  EXPECT_FALSE(obj);
}

TEST_F(ManualServerTest, PostOk200) {
  server_->RegisterRequestHandler(
      "/redfish/v1/Chassis",
      [](tensorflow::serving::net_http::ServerRequestInterface *req) {
        EXPECT_THAT(req->http_method(), Eq("POST"));
        tensorflow::serving::net_http::SetContentType(req, "application/json");
        req->WriteResponseString(R"json({
          "error": {
            "message": "Success."
          }
        })json");
        req->ReplyWithStatus(tensorflow::serving::net_http::HTTPStatusCode::OK);
      },
      tensorflow::serving::net_http::RequestHandlerOptions());

  auto patch = intf_->PostUri("/redfish/v1/Chassis", {{"Name", "Hello"}});
  EXPECT_THAT(patch.httpcode(), Eq(ecclesia::HTTP_CODE_REQUEST_OK));
  EXPECT_TRUE(patch.status().ok()) << patch.status().message();
  auto obj = patch.AsObject();
  ASSERT_TRUE(obj);
  auto error = (*obj)["error"].AsObject();
  ASSERT_TRUE(error);
  EXPECT_THAT(error->GetNodeValue<std::string>("message"), Eq("Success."));
}

TEST_F(ManualServerTest, PostOk201) {
  server_->RegisterRequestHandler(
      "/redfish/v1/Chassis",
      [](tensorflow::serving::net_http::ServerRequestInterface *req) {
        EXPECT_THAT(req->http_method(), Eq("POST"));
        req->AppendResponseHeader("Location", "/redfish/v1/Chassis/chassis");
        req->ReplyWithStatus(
            tensorflow::serving::net_http::HTTPStatusCode::CREATED);
      },
      tensorflow::serving::net_http::RequestHandlerOptions());
  // 202 gets redirected:
  server_->RegisterRequestHandler(
      "/redfish/v1/Chassis/chassis",
      [](tensorflow::serving::net_http::ServerRequestInterface *req) {
        EXPECT_THAT(req->http_method(), Eq("GET"));
        tensorflow::serving::net_http::SetContentType(req, "application/json");
        req->WriteResponseString(R"json({ "Name": "Hello" })json");
        req->ReplyWithStatus(tensorflow::serving::net_http::HTTPStatusCode::OK);
      },
      tensorflow::serving::net_http::RequestHandlerOptions());

  auto patch = intf_->PostUri("/redfish/v1/Chassis", {{"Name", "Hello"}});
  EXPECT_THAT(patch.httpcode(), Eq(ecclesia::HTTP_CODE_REQUEST_OK));
  EXPECT_TRUE(patch.status().ok()) << patch.status().message();
  auto obj = patch.AsObject();
  ASSERT_TRUE(obj);
  EXPECT_THAT(obj->GetNodeValue<libredfish::PropertyName>(), "Hello");
}

TEST_F(ManualServerTest, PostOk204) {
  server_->RegisterRequestHandler(
      "/redfish/v1/Chassis",
      [](tensorflow::serving::net_http::ServerRequestInterface *req) {
        EXPECT_THAT(req->http_method(), Eq("POST"));
        req->ReplyWithStatus(
            tensorflow::serving::net_http::HTTPStatusCode::NO_CONTENT);
      },
      tensorflow::serving::net_http::RequestHandlerOptions());

  auto patch = intf_->PostUri("/redfish/v1/Chassis", {{"Name", "Hello"}});
  EXPECT_THAT(patch.httpcode(), Eq(ecclesia::HTTP_CODE_NO_CONTENT));
  EXPECT_TRUE(patch.status().ok()) << patch.status().message();
  auto obj = patch.AsObject();
  EXPECT_FALSE(obj);
}

TEST_F(ManualServerTest, Post400) {
  server_->RegisterRequestHandler(
      "/redfish/v1/Chassis",
      [](tensorflow::serving::net_http::ServerRequestInterface *req) {
        EXPECT_THAT(req->http_method(), Eq("POST"));
        tensorflow::serving::net_http::SetContentType(req, "application/json");
        req->WriteResponseString(R"json({
          "error": {
            "message": "Failed."
          }
        })json");
        req->ReplyWithStatus(
            tensorflow::serving::net_http::HTTPStatusCode::BAD_REQUEST);
      },
      tensorflow::serving::net_http::RequestHandlerOptions());

  auto patch = intf_->PostUri("/redfish/v1/Chassis", {{"Name", "Hello"}});
  EXPECT_THAT(patch.httpcode(), Eq(ecclesia::HTTP_CODE_BAD_REQUEST));
  EXPECT_THAT(patch.status().code(), Eq(absl::StatusCode::kInvalidArgument))
      << patch.status().message();
  auto obj = patch.AsObject();
  ASSERT_TRUE(obj);
  auto error = (*obj)["error"].AsObject();
  ASSERT_TRUE(error);
  EXPECT_THAT(error->GetNodeValue<std::string>("message"), Eq("Failed."));
}

TEST_F(ManualServerTest, Post400NoContent) {
  server_->RegisterRequestHandler(
      "/redfish/v1/Chassis",
      [](tensorflow::serving::net_http::ServerRequestInterface *req) {
        EXPECT_THAT(req->http_method(), Eq("POST"));
        req->ReplyWithStatus(
            tensorflow::serving::net_http::HTTPStatusCode::BAD_REQUEST);
      },
      tensorflow::serving::net_http::RequestHandlerOptions());

  auto patch = intf_->PostUri("/redfish/v1/Chassis", {{"Name", "Hello"}});
  EXPECT_THAT(patch.httpcode(), Eq(ecclesia::HTTP_CODE_BAD_REQUEST));
  EXPECT_THAT(patch.status().code(), Eq(absl::StatusCode::kInvalidArgument))
      << patch.status().message();
  auto obj = patch.AsObject();
  EXPECT_FALSE(obj);
}

TEST_F(ManualServerTest, Post401NoContent) {
  server_->RegisterRequestHandler(
      "/redfish/v1/Chassis",
      [](tensorflow::serving::net_http::ServerRequestInterface *req) {
        EXPECT_THAT(req->http_method(), Eq("POST"));
        req->ReplyWithStatus(
            tensorflow::serving::net_http::HTTPStatusCode::UNAUTHORIZED);
      },
      tensorflow::serving::net_http::RequestHandlerOptions());

  auto patch = intf_->PostUri("/redfish/v1/Chassis", {{"Name", "Hello"}});
  EXPECT_THAT(patch.httpcode(), Eq(ecclesia::HTTP_CODE_UNAUTHORIZED));
  EXPECT_THAT(patch.status().code(), Eq(absl::StatusCode::kUnauthenticated))
      << patch.status().message();
  auto obj = patch.AsObject();
  EXPECT_FALSE(obj);
}

}  // namespace
}  // namespace libredfish
