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

// Conversions for using RPC/protobuf Status objects with absl::Status objects.

#ifndef ECCLESIA_LIB_STATUS_RPC_H_
#define ECCLESIA_LIB_STATUS_RPC_H_

#include "google/rpc/status.pb.h"
#include "absl/status/status.h"
#include "grpcpp/support/status.h"

namespace ecclesia {

// Given an RPC status, convert it to an equivalent absl::Status.
absl::Status StatusFromRpcStatus(const google::rpc::Status &status);

// Given an absl::Status, convert it to an equivalent google::rpc::Status.
google::rpc::Status StatusToRpcStatus(const absl::Status &status);

// Given an absl::Status, convert it to an equivalent gRPC status.
grpc::Status StatusToGrpcStatus(const absl::Status &status);

// Given a gRPC status, convert it to an equivalent absl::Status.
absl::Status StatusFromGrpcStatus(const grpc::Status &status);

// Conversion helpers between abseil and open-source protobuf status. In google,
// all statuses are abseil so the specialization should always be invoked.
template <typename T>
absl::Status AsAbslStatus(const T& status) {
  static_assert(!std::is_same_v<decltype(status), absl::Status>);
  return absl::Status(absl::StatusCode(static_cast<int>(status.code())),
                      status.message().as_string());
}
// Equivalent for gRPC status.
template <>
inline absl::Status AsAbslStatus(const grpc::Status& status) {
  return absl::Status(absl::StatusCode(static_cast<int>(status.error_code())),
                      status.error_message());
}
// Equivalent for google RPC status.
template <>
inline absl::Status AsAbslStatus(const google::rpc::Status& status) {
  return absl::Status(absl::StatusCode(static_cast<int>(status.code())),
                      status.message());
}
// Internal specialization
template <>
inline absl::Status AsAbslStatus<absl::Status>(const absl::Status& status) {
  return status;
}

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_STATUS_RPC_H_
