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
#include <string>

#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/redfish/transport/interface.h"

namespace ecclesia {
namespace {

TEST(RedfishTransportTest, CreateHttpTransport) {
  RedfishTransportConfig transport_config = {
      .hostname = "localhost",
      .port = 8000,
      .type = "http",
  };
  absl::StatusOr<std::unique_ptr<RedfishTransport>> transport =
      CreateRedfishTransport(transport_config);
  EXPECT_TRUE(transport.ok());
}
TEST(RedfishTransportTest, CreateLoasGrpcTransport) {
  RedfishTransportConfig transport_config = {
      .hostname = "localhost",
      .port = 8000,
      .type = "loas_grpc",
  };
  absl::StatusOr<std::unique_ptr<RedfishTransport>> transport =
      CreateRedfishTransport(transport_config);
  EXPECT_EQ(transport.status().code(), absl::StatusCode::kUnimplemented);
  EXPECT_EQ(transport.status().message(),
            "Loas based credentials is not available");
}
TEST(RedfishTransportTest, CreateInsecureGrpcTransport) {
  RedfishTransportConfig transport_config = {
      .hostname = "localhost",
      .port = 8000,
      .type = "insecure_grpc",
  };
  absl::StatusOr<std::unique_ptr<RedfishTransport>> transport =
      CreateRedfishTransport(transport_config);
  EXPECT_TRUE(transport.ok());
}
TEST(RedfishTransportTest, CreateMtlsGrpcTransport) {
  RedfishTransportConfig transport_config = {
      .hostname = "localhost",
      .port = 8000,
      .type = "mtls_grpc",
  };
  absl::StatusOr<std::unique_ptr<RedfishTransport>> transport =
      CreateRedfishTransport(transport_config);
  EXPECT_EQ(transport.status().code(), absl::StatusCode::kUnimplemented);
  EXPECT_EQ(transport.status().message(),
            "MTLS based credentials is not available");
}

TEST(RedfishTransportTest, CreateUnknown) {
  RedfishTransportConfig transport_config = {
      .hostname = "localhost",
      .port = 8000,
      .type = "unknown",
  };
  absl::StatusOr<std::unique_ptr<RedfishTransport>> transport =
      CreateRedfishTransport(transport_config);
  EXPECT_EQ(transport.status().code(), absl::StatusCode::kInternal);
  EXPECT_EQ(transport.status().message(), "Unknown transport type: unknown");
}

}  // namespace
}  // namespace ecclesia
