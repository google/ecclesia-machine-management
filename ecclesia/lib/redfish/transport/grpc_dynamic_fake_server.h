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

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "absl/strings/str_cat.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.grpc.pb.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.pb.h"
#include "grpcpp/security/server_credentials.h"
#include "grpcpp/server.h"
#include "grpcpp/server_context.h"
#include "grpcpp/support/status.h"

namespace ecclesia {

// FakeRedfishV1Impl implements the Redfish service according to the callback
// that users set. It's useful in unit tests.
// The class is thread compatible.
class FakeRedfishV1Impl : public ::redfish::v1::RedfishV1::Service {
 public:
  FakeRedfishV1Impl()
      : callback_([](grpc::ServerContext*, const ::redfish::v1::Request*,
                     redfish::v1::Response*) {
          return grpc::Status::CANCELLED;
        }) {}

  grpc::Status Get(grpc::ServerContext* context,
                   const ::redfish::v1::Request* request,
                   redfish::v1::Response* response) override {
    return callback_(context, request, response);
  }

  grpc::Status Post(grpc::ServerContext* context,
                    const ::redfish::v1::Request* request,
                    redfish::v1::Response* response) override {
    return callback_(context, request, response);
  }

  grpc::Status Patch(grpc::ServerContext* context,
                     const ::redfish::v1::Request* request,
                     redfish::v1::Response* response) override {
    return callback_(context, request, response);
  }

  grpc::Status Put(grpc::ServerContext* context,
                   const ::redfish::v1::Request* request,
                   redfish::v1::Response* response) override {
    return callback_(context, request, response);
  }

  grpc::Status Delete(grpc::ServerContext* context,
                      const ::redfish::v1::Request* request,
                      redfish::v1::Response* response) override {
    return callback_(context, request, response);
  }

  void SetCallback(std::function<grpc::Status(grpc::ServerContext*,
                                              const ::redfish::v1::Request*,
                                              redfish::v1::Response*)>
                       callback) {
    callback_ = std::move(callback);
  }

 private:
  std::function<grpc::Status(grpc::ServerContext*,
                             const ::redfish::v1::Request*,
                             redfish::v1::Response*)>
      callback_;
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
  void SetCallback(std::function<grpc::Status(grpc::ServerContext*,
                                              const ::redfish::v1::Request*,
                                              redfish::v1::Response*)>
                       callback);

 private:
  int port_;
  FakeRedfishV1Impl service_impl_;
  std::unique_ptr<grpc::Server> grpc_server_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_TRANSPORT_GRPC_DYNAMIC_FAKE_SERVER_H_
