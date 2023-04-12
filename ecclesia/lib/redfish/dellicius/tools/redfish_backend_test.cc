/*
 * Copyright 2023 Google LLC
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

#include "ecclesia/lib/redfish/dellicius/tools/redfish_backend.h"

#include <memory>

#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ecclesia/lib/redfish/transport/interface.h"

namespace ecclesia {
namespace {

TEST(RedfishTransportTest, CreateHttpTransport) {
  absl::StatusOr<std::unique_ptr<RedfishTransport>> transport =
      CreateRedfishTransport("localhost:8000", "http");
  EXPECT_TRUE(transport.ok());
}
TEST(RedfishTransportTest, CreateLoasGrpcTransport) {
  absl::StatusOr<std::unique_ptr<RedfishTransport>> transport =
      CreateRedfishTransport("localhost:8000", "loas_grpc");
  EXPECT_EQ(transport.status().code(), absl::StatusCode::kUnimplemented);
  EXPECT_EQ(transport.status().message(),
            "Loas based credentials is not available");
}
TEST(RedfishTransportTest, CreateInsecureGrpcTransport) {
  absl::StatusOr<std::unique_ptr<RedfishTransport>> transport =
      CreateRedfishTransport("localhost:8000", "insecure_grpc");
  EXPECT_TRUE(transport.ok());
}
TEST(RedfishTransportTest, CreateUnknown) {
  absl::StatusOr<std::unique_ptr<RedfishTransport>> transport =
      CreateRedfishTransport("localhost:8000", "unknown");
  EXPECT_EQ(transport.status().code(), absl::StatusCode::kInternal);
  EXPECT_EQ(transport.status().message(), "Unknown transport type: unknown");
}

}  // namespace
}  // namespace ecclesia
