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

#include "ecclesia/lib/redfish/transport/grpc_certificate_verifier.h"

#include <functional>
#include <memory>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/transport/x509_certificate.h"
#include "ecclesia/lib/status/rpc.h"
#include "grpcpp/security/tls_certificate_verifier.h"
#include "grpcpp/support/status.h"
#include "grpcpp/support/string_ref.h"
#include "openssl/base.h"

namespace ecclesia {

namespace {
using ::grpc::experimental::TlsCustomVerificationCheckRequest;

bool ReturnSyncAndAssignVerifierStatus(const absl::Status &status,
                                       grpc::Status &output_status) {
  constexpr bool kIsVerifierSync = true;
  output_status = StatusToGrpcStatus(status);
  return kIsVerifierSync;
}
}  // namespace

BmcVerifier::BmcVerifier(const SubjectAltName &node_san,
                         std::function<VerifyFunc> verify_func)
    : node_san_(node_san), verify_func_(std::move(verify_func)) {}

bool BmcVerifier::Verify(TlsCustomVerificationCheckRequest *request,
                         std::function<void(::grpc::Status)> /*callback*/,
                         grpc::Status *sync_status) {
  absl::string_view peer_cert_buffer(request->peer_cert().data(),
                                     request->peer_cert().size());
  absl::StatusOr<SubjectAltName> peer_san = GetSubjectAltName(peer_cert_buffer);
  if (!peer_san.ok()) {
    return ReturnSyncAndAssignVerifierStatus(peer_san.status(), *sync_status);
  }
  return ReturnSyncAndAssignVerifierStatus(
      verify_func_(node_san_, std::move(peer_san.value())), *sync_status);
}

}  // namespace ecclesia
