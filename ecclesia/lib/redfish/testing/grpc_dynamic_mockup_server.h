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

#ifndef THIRD_PARTY_MILOTIC_CC_REDFISH_GRPC_DYNAMIC_MOCKUP_SERVER_H_
#define THIRD_PARTY_MILOTIC_CC_REDFISH_GRPC_DYNAMIC_MOCKUP_SERVER_H_

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.grpc.pb.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.pb.h"
#include "ecclesia/lib/redfish/test_mockup.h"
#include "grpcpp/security/server_credentials.h"
#include "grpcpp/server.h"
#include "grpcpp/server_context.h"
#include "grpcpp/support/status.h"

namespace ecclesia {

// GrpcDynamicMockupServer runs a proxy HTTP server in front of a Redfish
// Mockup. By default, the proxy HTTP server will pass through the HTTP
// responses from the mockup. However, the GrpcDynamicMockupServer can also be
// configured with custom handlers so that specific URIs return arbitrary
// results.
//
//  |--------------------------------------------------|
//  |                   gRPC Client                    |
//  |--------------------------------------------------|
//           | gRPC requests        ^ gRPC response
//           V                      |
//  |--------------------------------------------------|
//  |              GrpcDynamicMockupServer             |
//  |--------------------------------------------------|
//       |         ^                     ^  | local functor invocation
//       |  HTTP   |  HTTP               |  V
//       |         |               |--------------|
//       |         |               | URI handlers |
//       V         |               |--------------|
//  |---------------------|
//  | TestingMockupServer |
//  |---------------------|
class GrpcDynamicMockupServer {
 public:
  // In the BUILD file of your test implementation, ensure that you have
  // included the SAR binary as a data dependency.
  //
  // For example:
  // cc_test(
  //   ...
  //   data =
  //   ["//ecclesia/redfish_mockups/indus_hmb_cn:indus_hmb_cn_mockup.shar"],
  //   ...
  // )
  //
  // Then, provide the name of the mockup .shar file. Currently only mockups
  // defined in redfish_mockups are supported. For example:
  //   mockup_sar = "indus_hmb_cn_mockup.shar"
  GrpcDynamicMockupServer(absl::string_view mockup_shar, absl::string_view host,
                          int port);
  GrpcDynamicMockupServer(absl::string_view mockup_shar, absl::string_view host,
                          int port,
                          std::shared_ptr<grpc::ServerCredentials> credentials);

  GrpcDynamicMockupServer(absl::string_view mockup_shar,
                          absl::string_view uds_path);
  GrpcDynamicMockupServer(absl::string_view mockup_shar,
                          absl::string_view uds_path,
                          std::shared_ptr<grpc::ServerCredentials> credentials);

  ~GrpcDynamicMockupServer() = default;

  // Register a custom handler to respond to a given REST request for a URI.
  using HandlerFunc = std::function<grpc::Status(
      grpc::ServerContext* context, const ::redfish::v1::Request* request,
      redfish::v1::Response* response)>;
  void AddHttpGetHandler(absl::string_view uri, HandlerFunc handler);
  void AddHttpPatchHandler(absl::string_view uri, HandlerFunc handler);
  void AddHttpPostHandler(absl::string_view uri, HandlerFunc handler);

  // Clear all registered handlers.
  void ClearHandlers();

  void Wait() { server_->Wait(); }

 private:
  // The gRPC server
  std::unique_ptr<grpc::Server> server_;
  // The proxy HttpServer serves Redfish mockup data
  libredfish::TestingMockupServer mockup_server_;

  // Private implementation of the Redfish GRPC service, with helper methods
  // defined for overwriting the REST operations with custom handlers.
  // By default, if there are no custom handlers for an operation registered,
  // this implementation will forward the REST request to the Redfish mockup.
  class RedfishV1Impl final : public ::redfish::v1::RedfishV1::Service {
   public:
    explicit RedfishV1Impl(
        std::unique_ptr<libredfish::RedfishInterface> redfish_intf)
        : redfish_intf_(std::move(redfish_intf)) {}

    grpc::Status Get(grpc::ServerContext* context,
                     const ::redfish::v1::Request* request,
                     redfish::v1::Response* response) override;
    grpc::Status Post(grpc::ServerContext* context,
                      const ::redfish::v1::Request* request,
                      redfish::v1::Response* response) override;
    grpc::Status Patch(grpc::ServerContext* context,
                       const ::redfish::v1::Request* request,
                       redfish::v1::Response* response) override;
    grpc::Status Put(grpc::ServerContext* context,
                     const ::redfish::v1::Request* request,
                     redfish::v1::Response* response) override;
    grpc::Status Delete(grpc::ServerContext* context,
                        const ::redfish::v1::Request* request,
                        redfish::v1::Response* response) override;
    using HandlerFunc = GrpcDynamicMockupServer::HandlerFunc;
    void AddHttpGetHandler(absl::string_view uri, HandlerFunc handler)
        ABSL_LOCKS_EXCLUDED(patch_lock_);
    void AddHttpPatchHandler(absl::string_view uri, HandlerFunc handler)
        ABSL_LOCKS_EXCLUDED(patch_lock_);
    void AddHttpPostHandler(absl::string_view uri, HandlerFunc handler)
        ABSL_LOCKS_EXCLUDED(patch_lock_);

    // Clear all registered handlers.
    void ClearHandlers() ABSL_LOCKS_EXCLUDED(patch_lock_);

   private:
    // Interface to the mockup server, used by the proxy server
    std::unique_ptr<libredfish::RedfishInterface> redfish_intf_;

    // Store of all patches
    absl::Mutex patch_lock_;
    absl::flat_hash_map<std::string, HandlerFunc> rest_get_handlers_
        ABSL_GUARDED_BY(patch_lock_);
    absl::flat_hash_map<std::string, HandlerFunc> rest_patch_handlers_
        ABSL_GUARDED_BY(patch_lock_);
    absl::flat_hash_map<std::string, HandlerFunc> rest_post_handlers_
        ABSL_GUARDED_BY(patch_lock_);
  };

  std::unique_ptr<RedfishV1Impl> redfish_v1_impl_;
};
}  // namespace ecclesia
#endif  // THIRD_PARTY_MILOTIC_CC_REDFISH_GRPC_DYNAMIC_MOCKUP_SERVER_H_
