/*
 * Copyright 2024 Google LLC
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

#include "ecclesia/lib/redfish/test_mockup.h"

#include <memory>
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "ecclesia/lib/file/path.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/status/macros.h"

namespace ecclesia {
namespace {

using ::testing::Eq;

absl::Status CreateMockupResource(const std::string& parent_dir,
                                  const std::string& resource_uri,
                                  const std::string& data,
                                  TestFilesystem* testfs) {
  std::string resource_dir_path = absl::StrCat(parent_dir, resource_uri);
  testfs->CreateDir(resource_dir_path);
  std::string file_path = absl::StrCat(resource_dir_path, "/index.json");
  testfs->WriteFile(file_path, data);
  return absl::OkStatus();
}

absl::StatusOr<std::string> CreateLocalMockupDirectory(
    const std::string& local_file_dir, TestFilesystem* testfs) {
  LOG(INFO) << "Creating mockup dir: " << local_file_dir;
  testfs->CreateDir(local_file_dir);

  std::string data = R"json({
    "v1": "/redfish/v1/"
})json";
  ECCLESIA_RETURN_IF_ERROR(
      CreateMockupResource(local_file_dir, "/redfish", data, testfs));

  data = R"json({
    "@odata.id": "/redfish/v1",
    "@odata.type": "#ServiceRoot.v1_11_0.ServiceRoot",
    "AccountService": {
        "@odata.id": "/redfish/v1/AccountService"
    },
    "Cables": {
        "@odata.id": "/redfish/v1/Cables"
    },
    "CertificateService": {
        "@odata.id": "/redfish/v1/CertificateService"
    },
    "Chassis": {
        "@odata.id": "/redfish/v1/Chassis"
    },
    "EventService": {
        "@odata.id": "/redfish/v1/EventService"
    },
    "Id": "RootService",
    "JsonSchemas": {
        "@odata.id": "/redfish/v1/JsonSchemas"
    },
    "Links": {
        "Sessions": {
            "@odata.id": "/redfish/v1/SessionService/Sessions"
        }
    },
    "Managers": {
        "@odata.id": "/redfish/v1/Managers"
    },
    "Name": "Some Test Root Service xyz",
    "RedfishVersion": "1.9.0",
    "Registries": {
        "@odata.id": "/redfish/v1/Registries"
    },
    "SessionService": {
        "@odata.id": "/redfish/v1/SessionService"
    },
    "Storage": {
        "@odata.id": "/redfish/v1/Storage"
    },
    "Systems": {
        "@odata.id": "/redfish/v1/Systems"
    },
    "Tasks": {
        "@odata.id": "/redfish/v1/TaskService"
    },
    "TelemetryService": {
        "@odata.id": "/redfish/v1/TelemetryService"
    },
    "UUID": "adfaf1234512351",
    "UpdateService": {
        "@odata.id": "/redfish/v1/UpdateService"
    }
})json";
  ECCLESIA_RETURN_IF_ERROR(
      CreateMockupResource(local_file_dir, "/redfish/v1", data, testfs));

  return testfs->GetTruePath(local_file_dir);
}

TEST(TestingMockupServerTest, GetRedfishResultsFromServer) {
  TestFilesystem testfs(GetTestTempdirPath());
  std::string local_file_dir =
      ecclesia::JoinFilePaths(GetTestTempdirPath(), "mockup");
  ecclesia::TestingMockupServer server(
      [&]() { return CreateLocalMockupDirectory("/mockup", &testfs); }, "");

  auto rf_intf = server.RedfishClientInterface();
  ASSERT_TRUE(rf_intf != nullptr);
  ecclesia::RedfishVariant result = rf_intf->UncachedGetUri("/redfish");
  auto obj = result.AsObject();
  ASSERT_NE(obj, nullptr);
  EXPECT_THAT(obj->GetNodeValue<std::string>("v1"), Eq("/redfish/v1/"));

  result = rf_intf->UncachedGetUri("/redfish/v1");
  obj = result.AsObject();
  ASSERT_NE(obj, nullptr);
  EXPECT_THAT(obj->GetNodeValue<std::string>("Name"),
              Eq("Some Test Root Service xyz"));

  result = rf_intf->UncachedGetUri("/redfish/v1/UnknownResource");
  EXPECT_FALSE(result.status().ok());
}

}  // namespace

}  // namespace ecclesia
