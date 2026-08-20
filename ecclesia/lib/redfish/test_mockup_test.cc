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

#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

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

// Globals to control mocks
pid_t g_mock_waitpid_return_val = 0;
int g_mock_waitpid_errno = 0;
int g_mock_waitpid_calls = 0;
int g_mock_kill_calls_sigterm = 0;
int g_mock_kill_calls_sigkill = 0;

// Real functions
pid_t (*g_real_waitpid)(pid_t, int*, int) = waitpid;
int (*g_real_kill)(pid_t, int) = kill;

pid_t MockWaitpid(pid_t pid, int* status, int options) {
  if ((options & WNOHANG) != 0) {
    g_mock_waitpid_calls++;
    if (g_mock_waitpid_errno != 0) {
      errno = g_mock_waitpid_errno;
      return -1;
    }
    return g_mock_waitpid_return_val;
  }
  // Blocking waitpid (fallback path after SIGKILL)
  return g_real_waitpid(pid, status, options);
}

int MockKill(pid_t pid, int sig) {
  if (sig == SIGTERM) {
    g_mock_kill_calls_sigterm++;
  } else if (sig == SIGKILL) {
    g_mock_kill_calls_sigkill++;
  }
  return g_real_kill(pid, sig);
}

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

TEST(TestingMockupServerTest, DestructorTimeoutFallbackToSigkill) {
  TestFilesystem testfs(GetTestTempdirPath());
  std::string local_file_dir =
      ecclesia::JoinFilePaths(GetTestTempdirPath(), "mockup");

  g_mock_waitpid_return_val = 0;
  g_mock_waitpid_errno = 0;
  g_mock_waitpid_calls = 0;
  g_mock_kill_calls_sigterm = 0;
  g_mock_kill_calls_sigkill = 0;

  {
    ecclesia::TestingMockupServer server(
        [&]() { return CreateLocalMockupDirectory("/mockup", &testfs); }, "");

    ecclesia::TestingMockupServer::SetSystemOpsForTesting(MockWaitpid,
                                                          MockKill);
  }

  ecclesia::TestingMockupServer::SetSystemOpsForTesting(nullptr, nullptr);

  EXPECT_EQ(g_mock_kill_calls_sigterm, 1);
  EXPECT_EQ(g_mock_kill_calls_sigkill, 1);
  EXPECT_EQ(g_mock_waitpid_calls, 50);
}

TEST(TestingMockupServerTest, DestructorCleanExitNoSigkill) {
  TestFilesystem testfs(GetTestTempdirPath());
  std::string local_file_dir =
      ecclesia::JoinFilePaths(GetTestTempdirPath(), "mockup");

  g_mock_kill_calls_sigterm = 0;
  g_mock_kill_calls_sigkill = 0;
  g_mock_waitpid_return_val = 0;
  g_mock_waitpid_calls = 0;

  auto local_kill = [](pid_t pid, int sig) -> int {
    if (sig == SIGTERM) {
      g_mock_kill_calls_sigterm++;
      g_mock_waitpid_return_val = -pid;
    } else if (sig == SIGKILL) {
      g_mock_kill_calls_sigkill++;
    }
    return g_real_kill(pid, sig);
  };

  auto local_waitpid = [](pid_t pid, int* status, int options) -> pid_t {
    if ((options & WNOHANG) != 0) {
      g_mock_waitpid_calls++;
      return g_mock_waitpid_return_val;
    }
    return g_real_waitpid(pid, status, options);
  };

  {
    ecclesia::TestingMockupServer server(
        [&]() { return CreateLocalMockupDirectory("/mockup", &testfs); }, "");

    ecclesia::TestingMockupServer::SetSystemOpsForTesting(local_waitpid,
                                                          local_kill);
  }

  ecclesia::TestingMockupServer::SetSystemOpsForTesting(nullptr, nullptr);

  EXPECT_EQ(g_mock_kill_calls_sigterm, 1);
  EXPECT_EQ(g_mock_kill_calls_sigkill, 0);
  EXPECT_EQ(g_mock_waitpid_calls, 1);
}

TEST(TestingMockupServerTest, DestructorHandlesEINTR) {
  TestFilesystem testfs(GetTestTempdirPath());
  std::string local_file_dir =
      ecclesia::JoinFilePaths(GetTestTempdirPath(), "mockup");

  g_mock_kill_calls_sigterm = 0;
  g_mock_kill_calls_sigkill = 0;
  g_mock_waitpid_return_val = 0;
  g_mock_waitpid_calls = 0;

  static int eintr_count = 0;
  eintr_count = 0;

  auto local_kill = [](pid_t pid, int sig) -> int {
    if (sig == SIGTERM) {
      g_mock_kill_calls_sigterm++;
      g_mock_waitpid_return_val = -pid;
    } else if (sig == SIGKILL) {
      g_mock_kill_calls_sigkill++;
    }
    return g_real_kill(pid, sig);
  };

  auto local_waitpid = [](pid_t pid, int* status, int options) -> pid_t {
    if ((options & WNOHANG) != 0) {
      g_mock_waitpid_calls++;
      if (eintr_count < 3) {
        eintr_count++;
        errno = EINTR;
        return -1;
      }
      return g_mock_waitpid_return_val;
    }
    return g_real_waitpid(pid, status, options);
  };

  {
    ecclesia::TestingMockupServer server(
        [&]() { return CreateLocalMockupDirectory("/mockup", &testfs); }, "");

    ecclesia::TestingMockupServer::SetSystemOpsForTesting(local_waitpid,
                                                          local_kill);
  }

  ecclesia::TestingMockupServer::SetSystemOpsForTesting(nullptr, nullptr);

  EXPECT_EQ(g_mock_kill_calls_sigterm, 1);
  EXPECT_EQ(g_mock_kill_calls_sigkill, 0);
  EXPECT_EQ(eintr_count, 3);
  EXPECT_EQ(g_mock_waitpid_calls, 4);
}

TEST(TestingMockupServerTest, DestructorHandlesECHILD) {
  TestFilesystem testfs(GetTestTempdirPath());
  std::string local_file_dir =
      ecclesia::JoinFilePaths(GetTestTempdirPath(), "mockup");

  g_mock_kill_calls_sigterm = 0;
  g_mock_kill_calls_sigkill = 0;
  g_mock_waitpid_calls = 0;

  auto local_kill = [](pid_t pid, int sig) -> int {
    if (sig == SIGTERM) {
      g_mock_kill_calls_sigterm++;
    } else if (sig == SIGKILL) {
      g_mock_kill_calls_sigkill++;
    }
    return g_real_kill(pid, sig);
  };

  auto local_waitpid = [](pid_t pid, int* status, int options) -> pid_t {
    if ((options & WNOHANG) != 0) {
      g_mock_waitpid_calls++;
      errno = ECHILD;
      return -1;
    }
    return g_real_waitpid(pid, status, options);
  };

  {
    ecclesia::TestingMockupServer server(
        [&]() { return CreateLocalMockupDirectory("/mockup", &testfs); }, "");

    ecclesia::TestingMockupServer::SetSystemOpsForTesting(local_waitpid,
                                                          local_kill);
  }

  ecclesia::TestingMockupServer::SetSystemOpsForTesting(nullptr, nullptr);

  EXPECT_EQ(g_mock_kill_calls_sigterm, 1);
  EXPECT_EQ(g_mock_kill_calls_sigkill, 0);
  EXPECT_EQ(g_mock_waitpid_calls, 1);
}

}  // namespace

}  // namespace ecclesia
