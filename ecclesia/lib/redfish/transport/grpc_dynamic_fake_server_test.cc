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

#include "ecclesia/lib/redfish/transport/grpc_dynamic_fake_server.h"

#include <memory>
#include <queue>
#include <string>
#include <thread>  // NOLINT: we must use std thread to make upstream codes work
#include <utility>

#include "google/protobuf/struct.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.grpc.pb.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.pb.h"
#include "ecclesia/lib/redfish/transport/cache.h"
#include "ecclesia/lib/redfish/transport/grpc.h"
#include "ecclesia/lib/redfish/transport/http_redfish_intf.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/redfish/utils.h"
#include "ecclesia/lib/testing/proto.h"
#include "ecclesia/lib/testing/status.h"
#include "grpcpp/channel.h"
#include "grpcpp/client_context.h"
#include "grpcpp/create_channel.h"
#include "grpcpp/security/credentials.h"
#include "grpcpp/server_context.h"
#include "grpcpp/support/status.h"
#include "grpcpp/support/time.h"  // IWYU pragma: keep
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {
namespace {

using ::google::protobuf::Struct;
using ::google::protobuf::Value;
using ::redfish::v1::Request;
using ::redfish::v1::Response;

TEST(FakeServerTest, InsecureChannelDefault) {
  GrpcDynamicFakeServer fake_server;
  std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel(
      fake_server.GetHostPort(), grpc::InsecureChannelCredentials());
  std::unique_ptr<GrpcRedfishV1::Stub> stub = GrpcRedfishV1::NewStub(channel);
  grpc::ClientContext context;
  context.set_deadline(absl::ToChronoTime(absl::Now() + absl::Seconds(1)));
  Request request;
  EXPECT_EQ(stub->Get(&context, request, nullptr).error_code(),
            grpc::StatusCode::CANCELLED);
}

TEST(FakeServerTest, InsecureChannelOk) {
  GrpcDynamicFakeServer fake_server;
  std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel(
      fake_server.GetHostPort(), grpc::InsecureChannelCredentials());
  std::unique_ptr<GrpcRedfishV1::Stub> stub = GrpcRedfishV1::NewStub(channel);
  Value value;
  value.set_string_value("world");
  Struct expected_response;
  expected_response.mutable_fields()->insert({"hello", value});
  std::string expected_url = "/fake";
  fake_server.SetCallback([expected_response, expected_url](
                              grpc::CallbackServerContext *context,
                              const Request *request, Response *response) {
    EXPECT_EQ(request->url(), expected_url);
    *response->mutable_json() = expected_response;
    return grpc::Status::OK;
  });
  grpc::ClientContext context;
  context.set_deadline(absl::ToChronoTime(absl::Now() + absl::Seconds(1)));
  Response response;
  Request request;
  request.set_url(expected_url);
  EXPECT_TRUE(stub->Get(&context, request, &response).ok());
  EXPECT_THAT(response.json(), EqualsProto(expected_response));
}

TEST(FakeServerTest, InsecureChannelNotOk) {
  GrpcDynamicFakeServer fake_server;
  std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel(
      fake_server.GetHostPort(), grpc::InsecureChannelCredentials());
  std::unique_ptr<GrpcRedfishV1::Stub> stub = GrpcRedfishV1::NewStub(channel);

  fake_server.SetCallback(
      [](grpc::CallbackServerContext *context, const Request *request,
         Response *response) { return grpc::Status::CANCELLED; });
  Request request;
  {
    SCOPED_TRACE("Get returns CANCELLED status");
    grpc::ClientContext context;
    context.set_deadline(absl::ToChronoTime(absl::Now() + absl::Seconds(1)));
    EXPECT_EQ(stub->Get(&context, request, nullptr).error_code(),
              grpc::StatusCode::CANCELLED);
  }
  {
    SCOPED_TRACE("Post returns CANCELLED status");
    grpc::ClientContext context;
    context.set_deadline(absl::ToChronoTime(absl::Now() + absl::Seconds(1)));
    EXPECT_EQ(stub->Post(&context, request, nullptr).error_code(),
              grpc::StatusCode::CANCELLED);
  }
  {
    SCOPED_TRACE("Delete returns CANCELLED status");
    grpc::ClientContext context;
    context.set_deadline(absl::ToChronoTime(absl::Now() + absl::Seconds(1)));
    EXPECT_EQ(stub->Delete(&context, request, nullptr).error_code(),
              grpc::StatusCode::CANCELLED);
  }
  {
    SCOPED_TRACE("Patch returns CANCELLED status");
    grpc::ClientContext context;
    context.set_deadline(absl::ToChronoTime(absl::Now() + absl::Seconds(1)));
    EXPECT_EQ(stub->Patch(&context, request, nullptr).error_code(),
              grpc::StatusCode::CANCELLED);
  }
  {
    SCOPED_TRACE("Put returns CANCELLED status");
    grpc::ClientContext context;
    context.set_deadline(absl::ToChronoTime(absl::Now() + absl::Seconds(1)));
    EXPECT_EQ(stub->Put(&context, request, nullptr).error_code(),
              grpc::StatusCode::CANCELLED);
  }
}

Response MakeBytesResponseFromStr(absl::string_view str) {
  Response response;
  response.set_octet_stream(std::string(str));
  return response;
}

Response MakeJsonResponseFromStr(absl::string_view str) {
  Response response;
  response.set_json_str(std::string(str));
  return response;
}

TEST(FakeServerTest, InsecureChannelStreamPredefinedEventsOk) {
  GrpcDynamicFakeServer fake_server;
  std::queue<Response> predefined_events;
  predefined_events.push(MakeBytesResponseFromStr("hello"));
  predefined_events.push(MakeJsonResponseFromStr(R"({"value": "world"})"));

  absl::Notification server_stream_ended;
  FakeServerStreamConfig stream_config = {
      .mode = FakeServerStreamConfig::Mode::kPreConfigured,
      .events = predefined_events,
  };
  fake_server.SetStreamConfig(stream_config);
  fake_server.StartStreaming();

  GrpcTransportParams transport_params;

  absl::StatusOr<std::unique_ptr<RedfishTransport>> transport =
      CreateGrpcRedfishTransport(fake_server.GetHostPort(), transport_params,
                                 grpc::InsecureChannelCredentials());
  ASSERT_THAT(transport, ecclesia::IsOk());

  std::unique_ptr<RedfishCachedGetterInterface> cache =
      std::make_unique<NullCache>(transport.value().get());

  std::unique_ptr<RedfishInterface> redfish_client =
      NewHttpInterface(std::move(transport.value()), std::move(cache),
                       RedfishInterface::TrustedEndpoint::kTrusted);
  int event_id = 0;
  absl::AnyInvocable<void(const RedfishVariant &) const> on_event =
      [&event_id](const RedfishVariant &result) {
        if (event_id == 0) {
          ASSERT_TRUE(result.AsRaw());
          EXPECT_EQ(result.AsRaw(), GetBytesFromString("hello"));
          event_id++;
        } else if (event_id == 1) {
          EXPECT_EQ(
              result.AsFreshObject()->GetContentAsJson(),
              nlohmann::json::parse(R"({"value": "world"})", nullptr, false));
          event_id++;
        }
        EXPECT_LE(event_id, 3);
      };

  absl::AnyInvocable<void(const absl::Status &status) const> on_stop =
      [&server_stream_ended](const absl::Status &status) {
        EXPECT_THAT(status, ecclesia::IsOk());
        server_stream_ended.Notify();
      };

  absl::StatusOr<std::unique_ptr<RedfishEventStream>> stream =
      redfish_client->Subscribe("whatever", on_event, on_stop);
  ASSERT_THAT(stream, ecclesia::IsOk());
  stream.value()->StartStreaming();

  server_stream_ended.WaitForNotification();
}

TEST(FakeServerTest, InsecureChannelStreamRandomEventsOkUntilCancelled) {
  GrpcDynamicFakeServer fake_server;

  absl::Notification server_stream_ended;
  FakeServerStreamConfig stream_config = {
      .mode = FakeServerStreamConfig::Mode::kRandom,
  };
  fake_server.SetStreamConfig(stream_config);
  fake_server.StartStreaming();

  GrpcTransportParams transport_params;

  absl::Notification notification;
  absl::StatusOr<std::unique_ptr<RedfishTransport>> transport =
      CreateGrpcRedfishTransport(fake_server.GetHostPort(), transport_params,
                                 grpc::InsecureChannelCredentials());
  ASSERT_THAT(transport, ecclesia::IsOk());

  absl::AnyInvocable<void(const absl::Status &) const> on_stop =
      [&server_stream_ended](const absl::Status &status) {
        server_stream_ended.Notify();
        EXPECT_THAT(status, testing::Not(ecclesia::IsOk()));
      };

  // Notify the clients when 10 events are sent
  RedfishTransport::EventCallback on_event =
      [received_events = 0,
       &notification](const RedfishTransport::Result &result) mutable {
        EXPECT_EQ(std::get<RedfishTransport::bytes>(result.body),
                  GetBytesFromString("Random data"));
        received_events++;
        if (received_events == 10) {
          notification.Notify();
        }
      };

  absl::StatusOr<std::unique_ptr<RedfishEventStream>> stream =
      (*transport)->Subscribe("whatever", std::move(on_event), on_stop);

  ASSERT_THAT(stream, ecclesia::IsOk());
  (*stream)->StartStreaming();
  std::thread counting_worker([&stream, &notification]() {
    notification.WaitForNotification();
    (*stream)->CancelStreaming();
  });

  counting_worker.join();
  // Server should end with not okay, too
  server_stream_ended.WaitForNotification();
}

}  // namespace
}  // namespace ecclesia
