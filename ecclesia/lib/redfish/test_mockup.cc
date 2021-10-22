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

#include <cstddef>
#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>

#include "absl/base/macros.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "ecclesia/lib/file/path.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/http/client.h"
#include "ecclesia/lib/http/cred.pb.h"
#include "ecclesia/lib/http/curl_client.h"
#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/lib/logging/posix.h"
#include "ecclesia/lib/network/testing.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/transport/http.h"
#include "ecclesia/lib/redfish/transport/http_redfish_intf.h"
#include "ecclesia/lib/redfish/transport/interface.h"

extern char **environ;

namespace libredfish {
namespace {

// Constants defining how long to wait and sleep while waiting for the daemon
// to start and make its serving available.
constexpr absl::Duration kDaemonStartTimeout = absl::Seconds(30);
constexpr absl::Duration kDaemonStartSleepDuration = absl::Milliseconds(50);

// The mock-up server will likely be ready in seconds
// Tune this value until you didn't see retries very often
constexpr absl::Duration kDaemonAuthStartEstimation = absl::Seconds(1);

// The URI scheme is ignored for unix sockets.
std::string ConfigToEndpoint(absl::string_view scheme,
                             const TestingMockupServer::ConfigUnix &config) {
  return absl::StrCat("unix://", config.socket_path);
}

std::string ConfigToEndpoint(absl::string_view scheme,
                             const TestingMockupServer::ConfigNetwork &config) {
  return absl::StrCat(scheme, "://", config.hostname, ":", config.port);
}

std::unique_ptr<ecclesia::HttpRedfishTransport> ConfigToTransport(
    std::unique_ptr<ecclesia::HttpClient> client,
    absl::string_view scheme,
    const TestingMockupServer::ConfigNetwork &conn) {
  return ecclesia::HttpRedfishTransport::MakeNetwork(
      std::move(client),
      absl::StrCat(scheme, "://", conn.hostname, ":", conn.port));
}
std::unique_ptr<ecclesia::HttpRedfishTransport> ConfigToTransport(
    std::unique_ptr<ecclesia::HttpClient> client,
    absl::string_view,
    const TestingMockupServer::ConfigUnix &conn) {
  return ecclesia::HttpRedfishTransport::MakeUds(std::move(client),
                                                 conn.socket_path);
}

}  // namespace

TestingMockupServer::TestingMockupServer(absl::string_view mockup_shar,
                                         absl::string_view uds_path)
    : connection_config_(ConfigUnix{.socket_path = uds_path.data()}) {
  std::string mockup_path = ecclesia::GetTestDataDependencyPath(
      ecclesia::JoinFilePaths("redfish_mockups", mockup_shar));
  std::string string_argv[] = {mockup_path, "--unix", uds_path.data()};
  char *argv[ABSL_ARRAYSIZE(string_argv) + 1] = {};
  for (size_t i = 0; i < ABSL_ARRAYSIZE(string_argv); ++i) {
    argv[i] = &string_argv[i][0];
  }
  SetUpMockupServer(
      argv, [this]() { return RedfishClientInterface(); }, std::nullopt);
}

TestingMockupServer::TestingMockupServer(absl::string_view mockup_shar)
    : connection_config_(ConfigNetwork{
          .hostname = "[::1]", .port = ecclesia::FindUnusedPortOrDie()}) {
  std::string mockup_path = ecclesia::GetTestDataDependencyPath(
      ecclesia::JoinFilePaths("redfish_mockups", mockup_shar));
  std::string string_argv[] = {
      mockup_path,
      "--host",
      "::1",
      "--port",
      absl::StrCat(std::get<ConfigNetwork>(connection_config_).port),
      "--ipv6"};
  char *argv[ABSL_ARRAYSIZE(string_argv) + 1] = {};
  for (size_t i = 0; i < ABSL_ARRAYSIZE(string_argv); ++i) {
    argv[i] = &string_argv[i][0];
  }
  SetUpMockupServer(
      argv, [this]() { return RedfishClientInterface(); }, std::nullopt);
}

TestingMockupServer::TestingMockupServer(absl::string_view mockup_shar,
                                         const ServerTlsConfig &server_config,
                                         const ClientTlsConfig &client_config)
    : connection_config_(ConfigNetwork{
          .hostname = "[::1]", .port = ecclesia::FindUnusedPortOrDie()}),
      client_tls_config_(client_config) {
  std::string mockup_path = ecclesia::GetTestDataDependencyPath(
      ecclesia::JoinFilePaths("redfish_mockups", mockup_shar));
  std::string string_argv[] = {
      mockup_path,
      "--host",
      "::1",
      "--port",
      absl::StrCat(std::get<ConfigNetwork>(connection_config_).port),
      "--ipv6",
      "--mode",
      "mtls",
      "--cert",
      server_config.cert_file,
      "--key",
      server_config.key_file,
      "--ca",
      server_config.ca_cert_file};
  char *argv[ABSL_ARRAYSIZE(string_argv) + 1] = {};
  for (size_t i = 0; i < ABSL_ARRAYSIZE(string_argv); ++i) {
    argv[i] = &string_argv[i][0];
  }
  SetUpMockupServer(
      argv, [this]() { return RedfishClientTlsAuthInterface(); },
      kDaemonAuthStartEstimation);
}

void TestingMockupServer::SetUpMockupServer(
    char **server_argv,
    const std::function<std::unique_ptr<RedfishInterface>()> &factory,
    std::optional<absl::Duration> start_estimation) {
  // Launch the supprocess using spawn. We spawn it into a unique process group
  // so that at shutdown we can terminate the entire tree.
  absl::Time start_time = absl::Now();
  absl::Time give_up_time = start_time + kDaemonStartTimeout;
  posix_spawnattr_t attr;
  posix_spawnattr_init(&attr);
  posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP);
  // Pass our environment to the child to at least get TEST_TMPDIR.
  int result = posix_spawn(&server_pid_, server_argv[0], nullptr, &attr,
                           server_argv, environ);
  ecclesia::Check(result == 0, "mockup server process started")
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

    // Try to fetch the redfish URI from the server. If it works, we're running.
    if (client->GetUri("/redfish").AsObject()) {
      server_ready = true;
      break;
    }

    // If fetching the URI failed, check to make sure the process is still
    // running. If it isn't then terminate with a fatal log.
    int status;
    pid_t waited = waitpid(server_pid_, &status, WNOHANG);
    if (waited == -1) {
      ecclesia::PosixFatalLog() << "waitpid() failed";
    } else if (waited == 0) {
      // This is the good case, it means we're still waiting, so do nothing.
    } else {
      // The process terminated in some way, try and log a useful indicator of
      // how it terminated.
      if (WIFEXITED(status)) {
        ecclesia::FatalLog() << "mockup server terminated early with exit code "
                             << WEXITSTATUS(status);
      } else if (WIFSIGNALED(status)) {
        ecclesia::FatalLog() << "mockup server terminated early with signal "
                             << WTERMSIG(status);
      } else {
        ecclesia::FatalLog() << "mockup server terminated in an unknown way";
      }
    }
    // Wait a little while before trying again.
    absl::SleepFor(kDaemonStartSleepDuration);
  } while (!server_ready && absl::Now() < give_up_time);
  ecclesia::Check(server_ready, "mockup server came up");
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

  auto intf = libredfish::NewHttpInterface(
      std::move(transport), libredfish::RedfishInterface::kTrusted);
  ecclesia::Check(intf != nullptr, "can connect to the redfish mockup server");
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
  ecclesia::Check(transport != nullptr, "can create a redfish transport.");

  if (auto auth_status = transport->DoSessionAuth("FakeName", "FakePassword");
      !auth_status.ok()) {
    ecclesia::ErrorLog() << "Failed to do session auth: "
                         << auth_status.message();
    return nullptr;
  }

  auto intf = libredfish::NewHttpInterface(
      std::move(transport), libredfish::RedfishInterface::kTrusted);
  ecclesia::Check(intf != nullptr, "can connect to the redfish mockup server");
  return intf;
}

std::unique_ptr<RedfishInterface>
TestingMockupServer::RedfishClientTlsAuthInterface() {
  ecclesia::Check(client_tls_config_.has_value(),
                  "client TLS configuration exists");

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

  auto intf = libredfish::NewHttpInterface(
      std::move(transport), libredfish::RedfishInterface::kTrusted);
  ecclesia::Check(intf != nullptr, "can connect to the redfish mockup server");
  return intf;
}

std::variant<TestingMockupServer::ConfigNetwork,
             TestingMockupServer::ConfigUnix>
TestingMockupServer::GetConfig() const {
  return connection_config_;
}

}  // namespace libredfish
