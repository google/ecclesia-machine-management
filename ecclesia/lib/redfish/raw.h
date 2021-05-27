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

#ifndef ECCLESIA_LIB_REDFISH_RAW_H_
#define ECCLESIA_LIB_REDFISH_RAW_H_

#include <memory>
#include <string>

#include "ecclesia/lib/http/client.h"
#include "ecclesia/lib/redfish/interface.h"

namespace libredfish {

// Redfish arguments for BasicAuth or SessionAuth.
struct PasswordArgs {
  std::string endpoint;
  std::string username;
  std::string password;
};

// Redfish arguments for TLS based Authentication.
//   if verify_peer is true, ca_cert_file is optional;
//   if verify_peer is false, ca_cert_file is discarded.
struct TlsArgs {
  std::string endpoint;
  bool verify_peer;
  bool verify_hostname;
  // Absolute path to the PEM encoded certificate file;
  std::string cert_file;
  // Absolute path to the PEM encoded private key file;
  std::string key_file;
  // Absolute path to the certificate authority bundle;
  absl::optional<std::string> ca_cert_file;
};

// Constructor method for creating a RawInterface.
// Returns nullptr in case the interface failed to be constructed.
std::unique_ptr<RedfishInterface> NewRawInterface(
    const std::string &endpoint, RedfishInterface::TrustedEndpoint trusted,
    std::unique_ptr<ecclesia::HttpClient> client = nullptr);

// Constructor method for creating a RawInterface with session auth.
// Returns nullptr in case the interface failed to be constructed.
// This does not take a "client" parameter because it is currently not
// needed nor tested.
std::unique_ptr<RedfishInterface> NewRawSessionAuthInterface(
    const PasswordArgs &connectionArgs);

// Constructor method for creating a RawInterface with basic auth.
// Returns nullptr in case the interface failed to be constructed.
// This does not take a "client" parameter because it is currently not
// needed nor tested.
std::unique_ptr<RedfishInterface> NewRawBasicAuthInterface(
    const PasswordArgs &connectionArgs);

// Constructor method for creating a RawInterface with Tls auth.
// Returns nullptr in case the interface failed to be constructed.
std::unique_ptr<RedfishInterface> NewRawTlsAuthInterface(
    const TlsArgs &connectionArgs);

}  // namespace libredfish

#endif  // ECCLESIA_LIB_REDFISH_RAW_H_
