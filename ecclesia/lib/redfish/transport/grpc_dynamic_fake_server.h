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

#ifndef ECCLESIA_LIB_REDFISH_TRANSPORT_GRPC_DYNAMIC_FAKE_SERVER_H_
#define ECCLESIA_LIB_REDFISH_TRANSPORT_GRPC_DYNAMIC_FAKE_SERVER_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <queue>
#include <string>
#include <utility>

#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.grpc.pb.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.pb.h"
#include "ecclesia/lib/redfish/proto/redfish_v1_grpc_include.h"
#include "ecclesia/lib/time/clock.h"
#include "grpcpp/security/server_credentials.h"
#include "grpcpp/server.h"
#include "grpcpp/server_context.h"
#include "grpcpp/support/server_callback.h"
#include "grpcpp/support/status.h"

namespace ecclesia {

struct FakeServerStreamConfig {
  enum class Mode : uint8_t {
    kRandom,  // Send random generated events and never close the stream from
              // the server side.
    kPreConfigured,  // Send a fixed sequence of events and then close the
                     // stream.
  };

  Mode mode = Mode::kRandom;
  // The fixed event sequence the server will send. Valid if mode is
  // Mode::kPreConfigured
  std::queue<redfish::v1::Response> events;
};

class FakeServerWriteReactor
    : public grpc::ServerWriteReactor<::redfish::v1::Response> {
 public:
  FakeServerWriteReactor(FakeServerStreamConfig config);
  void OnWriteDone(bool ok) override;
  void OnDone() override;
  void OnCancel() override;
  void SetStreamConfig(FakeServerStreamConfig stream_config) {
    config_ = std::move(stream_config);
  }

  void MaybeStartWrite();

 private:
  FakeServerStreamConfig config_;
  redfish::v1::Response fake_event_;
};

// FakeRedfishV1Impl implements the Redfish service according to the callback
// that users set. It's useful in unit tests.
// The class is thread compatible.
class FakeRedfishV1Impl : public GrpcRedfishV1::CallbackService {
 public:
  using Callback = std::function<grpc::Status(grpc::CallbackServerContext *,
                                              const ::redfish::v1::Request *,
                                              redfish::v1::Response *)>;

  FakeRedfishV1Impl()
      : callback_(
            [](grpc::CallbackServerContext *, const ::redfish::v1::Request *,
               redfish::v1::Response *) { return grpc::Status::CANCELLED; }),
        reactor_(FakeServerStreamConfig()) {}

  grpc::ServerUnaryReactor *Get(grpc::CallbackServerContext *context,
                                const ::redfish::v1::Request *request,
                                redfish::v1::Response *response) override {
    return RequestUri(context, request, response);
  }

  grpc::ServerUnaryReactor *Post(grpc::CallbackServerContext *context,
                                 const ::redfish::v1::Request *request,
                                 redfish::v1::Response *response) override {
    return RequestUri(context, request, response);
  }

  grpc::ServerUnaryReactor *Patch(grpc::CallbackServerContext *context,
                                  const ::redfish::v1::Request *request,
                                  redfish::v1::Response *response) override {
    return RequestUri(context, request, response);
  }

  grpc::ServerUnaryReactor *Put(grpc::CallbackServerContext *context,
                                const ::redfish::v1::Request *request,
                                redfish::v1::Response *response) override {
    return RequestUri(context, request, response);
  }

  grpc::ServerUnaryReactor *Delete(grpc::CallbackServerContext *context,
                                   const ::redfish::v1::Request *request,
                                   redfish::v1::Response *response) override {
    return RequestUri(context, request, response);
  }

  grpc::ServerWriteReactor<::redfish::v1::Response> *Subscribe(
      ::grpc::CallbackServerContext *context,
      const ::redfish::v1::Request *request) override {
    return &reactor_;
  }

  void SetCallback(Callback callback) { callback_ = std::move(callback); }

  void SetStreamConfig(FakeServerStreamConfig stream_config) {
    reactor_.SetStreamConfig(std::move(stream_config));
  }

  void StartStreaming() { reactor_.MaybeStartWrite(); }

 private:
  ::grpc::ServerUnaryReactor *RequestUri(grpc::CallbackServerContext *context,
                                         const ::redfish::v1::Request *request,
                                         redfish::v1::Response *response) {
    grpc::ServerUnaryReactor *reactor = context->DefaultReactor();
    reactor->Finish(callback_(context, request, response));
    return reactor;
  }

  std::function<grpc::Status(grpc::CallbackServerContext *,
                             const ::redfish::v1::Request *,
                             redfish::v1::Response *)>
      callback_;
  FakeServerWriteReactor reactor_;
};

// GrpcDynamicFakeServer starts a local gRPC-Redfish server upon construction
// and destroys it upon destruction. The behaviour is configurable by setting
// the callback. The class is thread compatible.
class GrpcDynamicFakeServer {
 public:
  // Builds a server that uses an insecure channel at a random port.
  GrpcDynamicFakeServer();
  // Builds a server that uses an insecure channel at a given port.
  explicit GrpcDynamicFakeServer(int port);
  // Builds a server that uses given credentials at a random port.
  explicit GrpcDynamicFakeServer(
      std::shared_ptr<grpc::ServerCredentials> credentials);
  // Builds a server that uses given credentials at a given port.
  GrpcDynamicFakeServer(int port,
                        std::shared_ptr<grpc::ServerCredentials> credentials);
  ~GrpcDynamicFakeServer();

  std::string GetHostPort() const { return absl::StrCat("localhost:", port_); }
  int GetPort() const { return port_; }
  void SetCallback(std::function<grpc::Status(grpc::CallbackServerContext *,
                                              const ::redfish::v1::Request *,
                                              redfish::v1::Response *)>
                       callback);
  void SetStreamConfig(FakeServerStreamConfig stream_config);
  // Lets the server start to stream on incoming subscriptions.
  void StartStreaming();

 private:
  int port_;
  FakeRedfishV1Impl service_impl_;
  std::unique_ptr<grpc::Server> grpc_server_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_TRANSPORT_GRPC_DYNAMIC_FAKE_SERVER_H_
