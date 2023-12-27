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

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "ecclesia/lib/network/testing.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.pb.h"
#include "grpcpp/security/server_credentials.h"
#include "grpcpp/server.h"
#include "grpcpp/server_builder.h"
#include "grpcpp/server_context.h"
#include "grpcpp/support/status.h"

namespace ecclesia {

FakeServerWriteReactor::FakeServerWriteReactor(FakeServerStreamConfig config)
    : config_(std::move(config)) {
  fake_event_.set_octet_stream("Random data");
  MaybeStartWrite();
}

void FakeServerWriteReactor::OnWriteDone(bool ok) {
  if (!ok) {
    LOG(WARNING) << "Write event failed!";
    Finish(grpc::Status(grpc::StatusCode::INTERNAL, "Unexpected error"));
    return;
  }
  switch (config_.mode) {
    case FakeServerStreamConfig::Mode::kPreConfigured: {
      config_.events.pop();
      break;
    }
    case FakeServerStreamConfig::Mode::kRandom: {
      break;
    }
  }
  MaybeStartWrite();
}

void FakeServerWriteReactor::OnDone() {}

// OnDone will still be called afterwards.
void FakeServerWriteReactor::OnCancel() {
  // not used now, but leave here for future usage.
}

void FakeServerWriteReactor::MaybeStartWrite() {
  switch (config_.mode) {
    case FakeServerStreamConfig::Mode::kPreConfigured: {
      if (config_.events.empty()) {
        Finish(grpc::Status::OK);
        return;
      }
      StartWrite(&config_.events.front());
      break;
    }
    case FakeServerStreamConfig::Mode::kRandom: {
      StartWrite(&fake_event_);
      break;
    }
  }
}

GrpcDynamicFakeServer::GrpcDynamicFakeServer()
    : GrpcDynamicFakeServer(grpc::InsecureServerCredentials()) {}

GrpcDynamicFakeServer::GrpcDynamicFakeServer(int port)
    : GrpcDynamicFakeServer(port, grpc::InsecureServerCredentials()) {}

GrpcDynamicFakeServer::GrpcDynamicFakeServer(
    std::shared_ptr<grpc::ServerCredentials> credentials)
    : GrpcDynamicFakeServer(FindUnusedPortOrDie(), std::move(credentials)) {}

GrpcDynamicFakeServer::GrpcDynamicFakeServer(
    int port, std::shared_ptr<grpc::ServerCredentials> credentials)
    : port_(port) {
  std::string server_address(absl::StrCat("localhost:", port_));
  grpc::ServerBuilder builder;
  builder.AddListeningPort(server_address, std::move(credentials));
  builder.RegisterService(&service_impl_);
  grpc_server_ = builder.BuildAndStart();
}

GrpcDynamicFakeServer::~GrpcDynamicFakeServer() { grpc_server_->Shutdown(); }
void GrpcDynamicFakeServer::SetCallback(FakeRedfishV1Impl::Callback callback) {
  service_impl_.SetCallback(std::move(callback));
}

void GrpcDynamicFakeServer::SetStreamConfig(FakeServerStreamConfig config) {
  service_impl_.SetStreamConfig(std::move(config));
}

void GrpcDynamicFakeServer::StartStreaming() { service_impl_.StartStreaming(); }

}  // namespace ecclesia
