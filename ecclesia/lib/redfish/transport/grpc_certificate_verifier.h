/*
 * Copyright 2022 Google LLC
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

#ifndef ECCLESIA_LIB_REDFISH_TRANSPORT_GRPC_CERTIFICATE_VERIFIER_H_
#define ECCLESIA_LIB_REDFISH_TRANSPORT_GRPC_CERTIFICATE_VERIFIER_H_

#include <functional>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/transport/x509_certificate.h"
#include "grpcpp/security/tls_certificate_verifier.h"
#include "grpcpp/support/status.h"

namespace ecclesia {

// An certificate verifier for multi-node machines.
// The class ensures peer is an authenticated BMC node on the same multi-node
// machine via performing matching on X509v3 Subject Alternative Name (SAN).
class BmcVerifier : public grpc::experimental::ExternalCertificateVerifier {
 public:
  using VerifyFunc = absl::Status(const SubjectAltName &node_san,
                                  const SubjectAltName &peer_san);

  // Constructs a verifier for the node identified by the given SubjectAltName.
  //  |SubjectAltName.spiffe_id| shall be extracted from node's certificate.
  //  |SubjectAltName.fqdn| shall be the authorized FQDN of the node.
  // Users shall also specify how to perform verification via |verify| which
  //  takes the node's SAN and Peer's SAN.
  BmcVerifier(const SubjectAltName &node_san,
              std::function<VerifyFunc> verify_func);

  ~BmcVerifier() override = default;

  bool Verify(grpc::experimental::TlsCustomVerificationCheckRequest *request,
              std::function<void(::grpc::Status)> callback,
              grpc::Status *sync_status) override;

  void Cancel(
      grpc::experimental::TlsCustomVerificationCheckRequest *) override {}

 private:
  SubjectAltName node_san_;
  std::function<VerifyFunc> verify_func_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_TRANSPORT_GRPC_CERTIFICATE_VERIFIER_H_
