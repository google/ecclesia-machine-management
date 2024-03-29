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

#include "ecclesia/lib/redfish/test_mockup.h"

#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "ecclesia/lib/file/path.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/http/client.h"
#include "ecclesia/lib/http/cred.pb.h"
#include "ecclesia/lib/http/curl_client.h"
#include "ecclesia/lib/network/ip.h"
#include "ecclesia/lib/network/testing.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/transport/cache.h"
#include "ecclesia/lib/redfish/transport/http.h"
#include "ecclesia/lib/redfish/transport/http_redfish_intf.h"
#include "ecclesia/lib/redfish/transport/interface.h"

namespace ecclesia {
namespace {

// Constants defining how long to wait and sleep while waiting for the daemon
// to start and make its serving available.
constexpr absl::Duration kDaemonStartTimeout = absl::Seconds(30);
constexpr absl::Duration kDaemonStartSleepDuration = absl::Milliseconds(50);

// The mock-up server will likely be ready in seconds
// Tune this value until you didn't see retries very often
constexpr absl::Duration kDaemonAuthStartEstimation = absl::Seconds(1);

constexpr absl::string_view kMockupServerPath =
    "com_google_ecclesia/external/redfishMockupServer/redfishMockupServer.par";

std::unique_ptr<ecclesia::HttpRedfishTransport> ConfigToTransport(
    std::unique_ptr<ecclesia::HttpClient> client, absl::string_view scheme,
    const TestingMockupServer::ConfigNetwork &conn) {
  return ecclesia::HttpRedfishTransport::MakeNetwork(
      std::move(client),
      absl::StrCat(scheme, "://", conn.hostname, ":", conn.port));
}
std::unique_ptr<ecclesia::HttpRedfishTransport> ConfigToTransport(
    std::unique_ptr<ecclesia::HttpClient> client, absl::string_view /*unused*/,
    const TestingMockupServer::ConfigUnix &conn) {
  return ecclesia::HttpRedfishTransport::MakeUds(std::move(client),
                                                 conn.socket_path);
}

}  // namespace

TestingMockupServer::TestingMockupServer(absl::string_view mockup_shar,
                                         absl::string_view uds_path)
    : connection_config_(ConfigUnix{.socket_path = std::string(uds_path)}) {
  std::string mockup_path = ecclesia::GetTestDataDependencyPath(
      ecclesia::JoinFilePaths("redfish_mockups", mockup_shar));
  std::vector<std::string> string_argv = {mockup_path, "--unix",
                                          std::string(uds_path)};
  SetUpMockupServer(
      string_argv,
      [this]() { return RedfishClientInterface(/*client=*/nullptr); },
      std::nullopt);
}

TestingMockupServer::TestingMockupServer(absl::string_view mockup_shar)
    : connection_config_(ConfigNetwork{
          .hostname = "localhost", .port = ecclesia::FindUnusedPortOrDie()}) {
  std::string mockup_path = ecclesia::GetTestDataDependencyPath(
      ecclesia::JoinFilePaths("redfish_mockups", mockup_shar));
  std::vector<std::string> string_argv = {
      mockup_path, "--host", "localhost", "--port",
      absl::StrCat(std::get<ConfigNetwork>(connection_config_).port)};
  if (IsIpv6LocalhostAvailable()) {
    string_argv.push_back("--ipv6");
  }
  SetUpMockupServer(
      string_argv,
      [this]() { return RedfishClientInterface(/*client=*/nullptr); },
      std::nullopt);
}

TestingMockupServer::TestingMockupServer(
    const MockupFilePreparer &mockup_file_preparer,
    absl::string_view mockup_path)
    : connection_config_(ConfigNetwork{
          .hostname = "localhost", .port = ecclesia::FindUnusedPortOrDie()}) {
  absl::StatusOr<std::string> local_file_dir = mockup_file_preparer();
  CHECK_OK(local_file_dir.status()) << "Failed to prepare local mockup dir";

  std::string server_path = std::string(mockup_path);
  if (server_path.empty()) {
    char *srcdir = std::getenv("TEST_SRCDIR");
    if (srcdir != nullptr) {
      server_path = ecclesia::JoinFilePaths(srcdir, kMockupServerPath);
    }
  }
  CHECK(!server_path.empty()) << "Invalid path for mockup server";

  LOG(INFO) << absl::StrCat(
      "Starting server: ", server_path,
      " that serves mockup files from dir: ", *local_file_dir);

  std::vector<std::string> string_argv = {
      server_path,
      "--dir",
      *local_file_dir,
      "--host",
      "localhost",
      "--port",
      absl::StrCat(std::get<ConfigNetwork>(connection_config_).port)};
  if (IsIpv6LocalhostAvailable()) {
    string_argv.push_back("--ipv6");
  }
  SetUpMockupServer(
      string_argv,
      [this]() { return RedfishClientInterface(/*client=*/nullptr); },
      std::nullopt);
}

TestingMockupServer::TestingMockupServer(absl::string_view mockup_shar,
                                         const ServerTlsConfig &server_config,
                                         const ClientTlsConfig &client_config)
    : connection_config_(ConfigNetwork{
          .hostname = "localhost", .port = ecclesia::FindUnusedPortOrDie()}),
      client_tls_config_(client_config) {
  std::string mockup_path = ecclesia::GetTestDataDependencyPath(
      ecclesia::JoinFilePaths("redfish_mockups", mockup_shar));
  std::vector<std::string> string_argv = {
      mockup_path,
      "--host",
      "localhost",
      "--port",
      absl::StrCat(std::get<ConfigNetwork>(connection_config_).port),
      "--mode",
      "mtls",
      "--cert",
      server_config.cert_file,
      "--key",
      server_config.key_file,
      "--ca",
      server_config.ca_cert_file};
  if (IsIpv6LocalhostAvailable()) {
    string_argv.push_back("--ipv6");
  }
  SetUpMockupServer(
      string_argv, [this]() { return RedfishClientTlsAuthInterface(); },
      kDaemonAuthStartEstimation);
}

void TestingMockupServer::SetUpMockupServer(
    std::vector<std::string> &string_argv,
    const std::function<std::unique_ptr<RedfishInterface>()> &factory,
    std::optional<absl::Duration> start_estimation) {
  std::vector<char *> server_argv(string_argv.size() + 1);
  for (size_t i = 0; i < string_argv.size(); ++i) {
    server_argv[i] = &string_argv[i][0];
  }
  // Launch the supprocess using spawn. We spawn it into a unique process group
  // so that at shutdown we can terminate the entire tree.
  absl::Time start_time = absl::Now();
  absl::Time give_up_time = start_time + kDaemonStartTimeout;
  posix_spawnattr_t attr;
  posix_spawnattr_init(&attr);
  posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP);
  // Pass our environment to the child to at least get TEST_TMPDIR.
  int result = posix_spawn(&server_pid_, server_argv[0], nullptr, &attr,
                           server_argv.data(), environ);
  CHECK(result == 0) << "mockup server process started, "
                     << "posix_spawn() returned " << result;

  // Wait for the client to be up.
  bool server_ready = false;
  // To decrease the number of times we do retries (a retry becomes more
  // expensive when involving authentication), we wait for some time before
  // starting to check if the mock up server is ready
  if (start_estimation.has_value()) {
    absl::SleepFor(*start_estimation);
  }
  do {
    auto client = factory();
    // The client is not ready yet
    if (client == nullptr) {
      // Wait a little while before trying again.
      absl::SleepFor(kDaemonStartSleepDuration);
      continue;
    }

    // Try to fetch the root URI from the server. If it works, we're running.
    if (client->GetRoot().AsObject()) {
      server_ready = true;
      break;
    }

    // If fetching the URI failed, check to make sure the process is still
    // running. If it isn't then terminate with a fatal log.
    int status;
    pid_t waited = waitpid(server_pid_, &status, WNOHANG);
    if (waited == -1) {
      PLOG(FATAL) << "waitpid() failed";
    } else if (waited == 0) {
      // This is the good case, it means we're still waiting, so do nothing.
    } else {
      // The process terminated in some way, try and log a useful indicator of
      // how it terminated.
      if (WIFEXITED(status)) {
        LOG(FATAL) << "mockup server terminated early with exit code "
                   << WEXITSTATUS(status);
      } else if (WIFSIGNALED(status)) {
        LOG(FATAL) << "mockup server terminated early with signal "
                   << WTERMSIG(status);
      } else {
        LOG(FATAL) << "mockup server terminated in an unknown way";
      }
    }
    // Wait a little while before trying again.
    absl::SleepFor(kDaemonStartSleepDuration);
  } while (!server_ready && absl::Now() < give_up_time);
  CHECK(server_ready) << "mockup server came up";
}

TestingMockupServer::~TestingMockupServer() {
  // Terminate the entire process group of the server.
  kill(-server_pid_, SIGTERM);
  waitpid(server_pid_, nullptr, 0);
}

std::unique_ptr<RedfishInterface> TestingMockupServer::RedfishClientInterface(
    std::unique_ptr<ecclesia::HttpClient> client) {
  if (client == nullptr) {
    client = std::make_unique<ecclesia::CurlHttpClient>(
        ecclesia::LibCurlProxy::CreateInstance(), ecclesia::HttpCredential());
  }

  std::unique_ptr<ecclesia::RedfishTransport> transport = std::visit(
      [&client](auto &conn) {
        return ConfigToTransport(std::move(client), "http", conn);
      },
      connection_config_);

  auto cache = std::make_unique<ecclesia::NullCache>(transport.get());
  auto intf = NewHttpInterface(std::move(transport), std::move(cache),
                               RedfishInterface::kTrusted);
  CHECK(intf != nullptr) << "can connect to the redfish mockup server";
  return intf;
}

std::unique_ptr<RedfishInterface>
TestingMockupServer::RedfishClientSessionAuthInterface(
    std::unique_ptr<ecclesia::HttpClient> client) {
  if (client == nullptr) {
    client = std::make_unique<ecclesia::CurlHttpClient>(
        ecclesia::LibCurlProxy::CreateInstance(), ecclesia::HttpCredential());
  }

  std::unique_ptr<ecclesia::HttpRedfishTransport> transport = std::visit(
      [&client](auto &conn) {
        return ConfigToTransport(std::move(client), "http", conn);
      },
      connection_config_);
  CHECK(transport != nullptr) << "can create a redfish transport.";

  if (auto auth_status = transport->DoSessionAuth("FakeName", "FakePassword");
      !auth_status.ok()) {
    LOG(ERROR) << "Failed to do session auth: " << auth_status.message();
    return nullptr;
  }

  auto cache = std::make_unique<ecclesia::NullCache>(transport.get());
  auto intf = NewHttpInterface(std::move(transport), std::move(cache),
                               RedfishInterface::kTrusted);
  CHECK(intf != nullptr) << "can connect to the redfish mockup server";
  return intf;
}

std::unique_ptr<RedfishInterface>
TestingMockupServer::RedfishClientTlsAuthInterface() {
  CHECK(client_tls_config_.has_value()) << "client TLS configuration exists";

  ecclesia::TlsCredential creds;
  creds.set_verify_hostname(client_tls_config_->verify_hostname);
  creds.set_verify_peer(client_tls_config_->verify_peer);
  creds.set_server_ca_cert_file(client_tls_config_->ca_cert_file.value_or(""));
  creds.set_cert_file(client_tls_config_->cert_file);
  creds.set_key_file(client_tls_config_->key_file);
  auto client = std::make_unique<ecclesia::CurlHttpClient>(
      ecclesia::LibCurlProxy::CreateInstance(), std::move(creds));

  std::unique_ptr<ecclesia::RedfishTransport> transport = std::visit(
      [&client](auto &conn) {
        return ConfigToTransport(std::move(client), "https", conn);
      },
      connection_config_);

  auto cache = std::make_unique<ecclesia::NullCache>(transport.get());
  auto intf = NewHttpInterface(std::move(transport), std::move(cache),
                               RedfishInterface::kTrusted);
  CHECK(intf != nullptr) << "can connect to the redfish mockup server";
  return intf;
}

std::variant<TestingMockupServer::ConfigNetwork,
             TestingMockupServer::ConfigUnix>
TestingMockupServer::GetConfig() const {
  return connection_config_;
}

}  // namespace ecclesia
