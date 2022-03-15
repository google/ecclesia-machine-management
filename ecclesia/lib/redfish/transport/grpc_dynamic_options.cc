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

#include "ecclesia/lib/redfish/transport/grpc_dynamic_options.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/logging/logging.h"
#include "grpcpp/security/credentials.h"
#include "grpcpp/security/tls_certificate_provider.h"
#include "grpcpp/security/tls_certificate_verifier.h"
#include "grpcpp/security/tls_credentials_options.h"

namespace ecclesia {

namespace {
// gRPC API requires we pass a parsable root cert even if we do not verify the
// cert signature.
const char* kUnusedFakeRootCert =
    R"(-----BEGIN CERTIFICATE-----
MIIDiDCCAnCgAwIBAgIIdwMbYuGm3akwDQYJKoZIhvcNAQELBQAwRTEXMBUGA1UE
ChMOR29vZ2xlIFRFU1RJTkcxKjAoBgNVBAMMIUdvb2dsZSBCTUNXZWIgKipUZXN0
aW5nKiogUm9vdCBDQTAgFw03MDAxMDEwMDAwMDAaGA8yMTI1MDEwMTAwMDAwMFow
RTEXMBUGA1UEChMOR29vZ2xlIFRFU1RJTkcxKjAoBgNVBAMMIUdvb2dsZSBCTUNX
ZWIgKipUZXN0aW5nKiogUm9vdCBDQTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCC
AQoCggEBAKprOCLzYyqb6Mxo+I3n70P32PQLtUQdgjDXHF2cpwI0w3ezBYn4Dx0H
T7RCRvMMlxnAcRPCTBcU4I+7HnUX+mBZvFkJPJukttCOS3usC6GtD8UMNassFkSL
Vepy5AV1cwXweklebe9gAgC8NsFetqbHz12jTGUfngYDdqhg2haiTTwXyoal+575
c6F2S5krWMnBcm9/bhQufi8nC8AzQ2r1e4/bSe64F+vXzsLODsGs7mtzXeEDB2/N
7pEhQbXmkuBudXtE5CpLwvNa1Y3XLZfXXewlzlxjqQarihEolv6e/XOAJma3XMh8
Ip5QE1F/YvX3PICAbjsaX4wmWNTv7tECAwEAAaN6MHgwDgYDVR0PAQH/BAQDAgIE
MB0GA1UdJQQWMBQGCCsGAQUFBwMBBggrBgEFBQcDAjAPBgNVHRMBAf8EBTADAQH/
MBkGA1UdDgQSBBCuOIbUVxFjIbMIYjmL68mVMBsGA1UdIwQUMBKAEK44htRXEWMh
swhiOYvryZUwDQYJKoZIhvcNAQELBQADggEBAAZ7KtkYl4CLzPTLUhSiisckk+9K
/dbiG9TjRoiD8iG2j6/lTtZr0dkEq2xgz7kfHTbINzDONn45ea+BojAKSgWcwjzj
TvTX0lubQcTLNBHVAZQ8Ws/7QN2+WF6QPpKWzk44O1Lw1JLhaLSe4GauIQICt0P5
zUVrgX2EP1FdbS/PxgQvwM3gbHJh7iqWr7ASVypLzA5+YfuPRIJAEOimQbrs/0Gq
UEYO7y3ijBKwLs0lm6rCzCjwacWb/tFIJK4FNTg7iTWs2t94HWopoZqp0mouAERe
qOFbsHcmv8mSUfig0AaTorZQpS8htNtcsCl5HhNgAyCQh+QzvBvesrEz5Cw=
-----END CERTIFICATE-----
)";

constexpr unsigned int kRefreshIntervalSec = 0;
}  // namespace

void GrpcDynamicImplOptions::SetToInsecure() {
  auth_type_ = AuthType::kInsecure;
}
void GrpcDynamicImplOptions::SetToTls(absl::string_view root_certs_buffer,
                                      absl::string_view key_buffer,
                                      absl::string_view cert_buffer) {
  auth_type_ = AuthType::kTlsVerifyServer;
  root_certs_ = root_certs_buffer;
  key_cert_ = {.private_key = std::string(key_buffer),
               .certificate_chain = std::string(cert_buffer)};
}

void GrpcDynamicImplOptions::SetToTlsSkipHostname(
    absl::string_view root_certs_buffer, absl::string_view key_buffer,
    absl::string_view cert_buffer,
    std::shared_ptr<grpc::experimental::CertificateVerifier> cert_verifier) {
  auth_type_ = AuthType::kTlsVerifyServerSkipHostname;
  root_certs_ = root_certs_buffer;
  key_cert_ = {.private_key = std::string(key_buffer),
               .certificate_chain = std::string(cert_buffer)};
  cert_verifier_ = std::move(cert_verifier);
}

void GrpcDynamicImplOptions::SetToTlsNotVerifyServer(
    absl::string_view key_buffer, absl::string_view cert_buffer,
    std::shared_ptr<grpc::experimental::CertificateVerifier> cert_verifier) {
  auth_type_ = AuthType::kTlsNotVerifyServer;
  key_cert_ = {.private_key = std::string(key_buffer),
               .certificate_chain = std::string(cert_buffer)};
  cert_verifier_ = std::move(cert_verifier);
}

std::shared_ptr<grpc::ChannelCredentials>
GrpcDynamicImplOptions::GetChannelCredentials() const {
  switch (auth_type_) {
    case AuthType::kTlsVerifyServer: {
      grpc::experimental::TlsChannelCredentialsOptions tls_options;
      tls_options.watch_identity_key_cert_pairs();
      tls_options.watch_root_certs();
      tls_options.set_certificate_provider(GetCertificateProvider());
      tls_options.set_verify_server_certs(true);
      return grpc::experimental::TlsCredentials(tls_options);
    }
    case AuthType::kTlsVerifyServerSkipHostname: {
      grpc::experimental::TlsChannelCredentialsOptions tls_options;
      tls_options.watch_identity_key_cert_pairs();
      tls_options.watch_root_certs();
      tls_options.set_certificate_provider(GetCertificateProvider());
      tls_options.set_certificate_verifier(cert_verifier_);
      tls_options.set_verify_server_certs(true);
      tls_options.set_check_call_host(false);
      return grpc::experimental::TlsCredentials(tls_options);
    }
    case AuthType::kTlsNotVerifyServer: {
      grpc::experimental::TlsChannelCredentialsOptions tls_options;
      tls_options.watch_identity_key_cert_pairs();
      tls_options.watch_root_certs();
      tls_options.set_certificate_provider(GetCertificateProvider());
      tls_options.set_certificate_verifier(cert_verifier_);
      tls_options.set_verify_server_certs(false);
      tls_options.set_check_call_host(false);
      return grpc::experimental::TlsCredentials(tls_options);
    }
    case AuthType::kInsecure:
      return grpc::InsecureChannelCredentials();
      // No default. We own the AuthType enum.
  }
  Check(false, absl::StrCat("Unexpected value for AuthType: ", auth_type_));
  return grpc::InsecureChannelCredentials();
}

std::shared_ptr<grpc::experimental::CertificateProviderInterface>
GrpcDynamicImplOptions::GetCertificateProvider() const {
  switch (auth_type_) {
    case AuthType::kTlsVerifyServer: {
      return std::make_shared<
          grpc::experimental::StaticDataCertificateProvider>(
          root_certs_,
          std::vector<grpc::experimental::IdentityKeyCertPair>{key_cert_});
    }
    case AuthType::kTlsVerifyServerSkipHostname: {
      return std::make_shared<
          grpc::experimental::StaticDataCertificateProvider>(
          root_certs_,
          std::vector<grpc::experimental::IdentityKeyCertPair>{key_cert_});
    }
    case AuthType::kTlsNotVerifyServer: {
      return std::make_shared<
          grpc::experimental::StaticDataCertificateProvider>(
          kUnusedFakeRootCert,
          std::vector<grpc::experimental::IdentityKeyCertPair>{key_cert_});
    }
    case AuthType::kInsecure:
      return nullptr;
      // No default. We own the AuthType enum.
  }
  Check(false, absl::StrCat("Unexpected value for AuthType: ", auth_type_));
  return nullptr;
}

void GrpcTransportOptions::SetToTls(absl::string_view root_certs_path,
                                    absl::string_view key_path,
                                    absl::string_view cert_path) {
  auth_type_ = AuthType::kTlsVerifyServer;
  root_certs_path_ = root_certs_path;
  key_path_ = key_path;
  cert_path_ = cert_path;
}

void GrpcTransportOptions::SetToTlsSkipHostname(
    absl::string_view root_certs_path, absl::string_view key_path,
    absl::string_view cert_path,
    std::shared_ptr<grpc::experimental::CertificateVerifier> cert_verifier) {
  auth_type_ = AuthType::kTlsVerifyServerSkipHostname;
  root_certs_path_ = root_certs_path;
  key_path_ = key_path;
  cert_path_ = cert_path;
  cert_verifier_ = std::move(cert_verifier);
}

void GrpcTransportOptions::SetToTlsNotVerifyServer(
    absl::string_view key_path, absl::string_view cert_path,
    std::shared_ptr<grpc::experimental::CertificateVerifier> cert_verifier) {
  auth_type_ = AuthType::kTlsNotVerifyServer;
  key_path_ = key_path;
  cert_path_ = cert_path;
  cert_verifier_ = std::move(cert_verifier);
}

std::shared_ptr<grpc::experimental::CertificateProviderInterface>
GrpcTransportOptions::GetCertificateProvider() const {
  switch (auth_type_) {
    case AuthType::kTlsVerifyServer: {
      return std::make_shared<
          grpc::experimental::FileWatcherCertificateProvider>(
          key_path_, cert_path_, root_certs_path_, kRefreshIntervalSec);
    }
    case AuthType::kTlsVerifyServerSkipHostname: {
      return std::make_shared<
          grpc::experimental::FileWatcherCertificateProvider>(
          key_path_, cert_path_, root_certs_path_, kRefreshIntervalSec);
    }
    case AuthType::kTlsNotVerifyServer: {
      return std::make_shared<
          grpc::experimental::FileWatcherCertificateProvider>(
          key_path_, cert_path_, kRefreshIntervalSec);
    }
    case AuthType::kInsecure:
      return nullptr;
      // No default. We own the AuthType enum.
  }
  Check(false, absl::StrCat("Unexpected value for AuthType: ", auth_type_));
  return nullptr;
}
}  // namespace ecclesia
