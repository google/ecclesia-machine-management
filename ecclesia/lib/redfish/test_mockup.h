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

#ifndef ECCLESIA_LIB_REDFISH_TEST_MOCKUP_H_
#define ECCLESIA_LIB_REDFISH_TEST_MOCKUP_H_

#include <sys/types.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ecclesia/lib/http/client.h"
#include "ecclesia/lib/redfish/interface.h"

namespace ecclesia {

// TestingMockupServer spins up a Redfish Mockup Server on localhost and allows
// raw Redfish interfaces to be connected to it.
class TestingMockupServer {
 public:
  // Server side mTls configuration
  struct ServerTlsConfig {
    std::string cert_file;
    std::string key_file;
    std::string ca_cert_file;
  };
  // Client side mTls configuration
  struct ClientTlsConfig {
    bool verify_peer;
    bool verify_hostname;
    std::string cert_file;
    std::string key_file;
    std::optional<std::string> ca_cert_file;
  };

  // In the BUILD file of your test implementation, ensure that you have
  // included the SAR binary as a data dependency.
  //
  // For example:
  // cc_test(
  //   ...
  //   data =
  //   ["//ecclesia/redfish_mockups/indus_hmb_cn:indus_hmb_cn_mockup.shar"],
  //   ...
  // )
  //
  // Then, provide the name of the mockup .shar file. Currently only mockups
  // defined in redfish_mockups are supported.For example:
  //   mockup_sar = "indus_hmb_cn_mockup.shar"
  explicit TestingMockupServer(absl::string_view mockup_shar);

  // A mockup file preparer is supposed to prepare a directory containing the
  // redfish mockup (index.json files). Upon success, it should return the path
  // to the parent directory of "/redfish". Otherwise, it returns a status
  // instead.
  // For example, a directory "<path>/foo/" contains
  // "<path>/foo/redfish/index.json", "<path>/foo/redfish/v1/index.json", etc.
  // Then this MockupFilePreparer should return "<path>/foo/" if it successfully
  // prepares the mockup files.
  using MockupFilePreparer = std::function<absl::StatusOr<std::string>()>;
  TestingMockupServer(const MockupFilePreparer &mockup_file_preparer,
                      absl::string_view mockup_path);

  TestingMockupServer(absl::string_view mockup_shar,
                      absl::string_view uds_path);

  // Creates an mTLS enabled mockup server
  TestingMockupServer(absl::string_view mockup_shar,
                      const ServerTlsConfig &server_config,
                      const ClientTlsConfig &client_config);
  ~TestingMockupServer();

  // Returns a new RedfishInterface connected to the mockup server.
  std::unique_ptr<RedfishInterface> RedfishClientInterface(
      std::unique_ptr<ecclesia::HttpClient> client = nullptr);

  // Returns a new RedfishInterface connected to the mockup server.
  // Auth type is REDFISH_AUTH_SESSION
  std::unique_ptr<RedfishInterface> RedfishClientSessionAuthInterface(
      std::unique_ptr<ecclesia::HttpClient> client = nullptr);

  // Returns a new RedfishInterface connected to the mockup server.
  // Auth type is REDFISH_AUTH_TLS
  std::unique_ptr<RedfishInterface> RedfishClientTlsAuthInterface();

  // The hostname and port the server will be listening on.
  struct ConfigNetwork {
    std::string hostname;
    int port;
  };
  // The unix domain socket that the server will be listening on
  struct ConfigUnix {
    std::string socket_path;
  };
  std::variant<ConfigNetwork, ConfigUnix> GetConfig() const;

 private:
  void SetUpMockupServer(
      std::vector<std::string> &string_argv,
      const std::function<std::unique_ptr<RedfishInterface>()> &factory,
      std::optional<absl::Duration> start_estimation);

  // The connection configuration of this mockup.
  std::variant<ConfigNetwork, ConfigUnix> connection_config_;
  // The pid of the server subprocess.
  pid_t server_pid_;
  // Client side Tls configuration of this mockup.
  std::optional<ClientTlsConfig> client_tls_config_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_TEST_MOCKUP_H_
