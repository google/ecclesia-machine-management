/*
 * Copyright 2021 Google LLC
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

#ifndef ECCLESIA_LIB_REDFISH_TRANSPORT_GRPC_DYNAMIC_OPTIONS_H_
#define ECCLESIA_LIB_REDFISH_TRANSPORT_GRPC_DYNAMIC_OPTIONS_H_

#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "grpcpp/security/credentials.h"
#include "grpcpp/security/tls_certificate_provider.h"
#include "grpcpp/security/tls_certificate_verifier.h"
#include "grpcpp/security/tls_credentials_options.h"

namespace ecclesia {

class GrpcDynamicImplOptions {
 public:
  enum class AuthType {
    kInsecure,
    kTlsVerifyServer,
    kTlsVerifyServerSkipHostname,
    kTlsNotVerifyServer
  };

  GrpcDynamicImplOptions()
      : auth_type_(AuthType::kInsecure),
        timeout_(absl::Seconds(3)) {}

  // Authentication options.
  // Use gRPC InsecureCredentials.
  void SetToInsecure();
  // Use gRPC TlsCredentials.
  void SetToTls(absl::string_view root_certs, absl::string_view key,
                absl::string_view cert);
  // Use gRPC TlsCredentials, but skip hostname check.
  void SetToTlsSkipHostname(
      absl::string_view root_certs, absl::string_view key,
      absl::string_view cert,
      std::shared_ptr<grpc::experimental::CertificateVerifier> cert_verifier);
  // Use gRPC TlsCredentials, but don't verify server at all.
  void SetToTlsNotVerifyServer(
      absl::string_view key, absl::string_view cert,
      std::shared_ptr<grpc::experimental::CertificateVerifier> cert_verifier);

  void SetTimeout(absl::Duration timeout) { timeout_ = timeout; }

  absl::Duration GetTimeout() const { return timeout_; }

  // Get gRPC channel credentials according to authentication options.
  std::shared_ptr<grpc::ChannelCredentials> GetChannelCredentials() const;

 protected:
  AuthType auth_type_;
  std::string root_certs_;
  grpc::experimental::IdentityKeyCertPair key_cert_;
  std::shared_ptr<grpc::experimental::CertificateVerifier> cert_verifier_;
  absl::Duration timeout_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_TRANSPORT_GRPC_DYNAMIC_OPTIONS_H_
