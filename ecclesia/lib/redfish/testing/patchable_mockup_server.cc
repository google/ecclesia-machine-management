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

#include "ecclesia/lib/redfish/testing/patchable_mockup_server.h"

#include <spawn.h>
#include <sys/wait.h>

#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "ecclesia/lib/file/path.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/file/uds.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/lib/logging/posix.h"
#include "ecclesia/lib/redfish/raw.h"
#include "ecclesia/magent/daemons/common.h"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"

namespace ecclesia {

void PatchableMockupServer::ClearPatches() {
  absl::MutexLock mu(&patch_lock_);
  uri_to_patches_.clear();
}

void PatchableMockupServer::PatchUri(std::string uri,
                                     absl::Span<const char> data) {
  absl::MutexLock mu(&patch_lock_);
  uri_to_patches_[std::move(uri)] = data;
}

absl::optional<absl::Span<const char>> PatchableMockupServer::GetPatch(
    absl::string_view uri) {
  absl::MutexLock mu(&patch_lock_);
  auto uri_to_patches_itr = uri_to_patches_.find(uri);
  if (uri_to_patches_itr != uri_to_patches_.end()) {
    return uri_to_patches_itr->second;
  }
  return absl::nullopt;
}

std::unique_ptr<libredfish::RedfishInterface>
PatchableMockupServer::RedfishClientInterface() {
  std::string endpoint =
      absl::StrCat("http://localhost:", proxy_server_->listen_port());
  auto intf = libredfish::NewRawInterface(
      endpoint, libredfish::RedfishInterface::kTrusted);
  ecclesia::Check(intf != nullptr, "can connect to the redfish proxy server");
  return intf;
}

PatchableMockupServer::PatchableMockupServer(absl::string_view mockup_shar,
                                             absl::string_view mockup_uds_path)
    : proxy_server_(ecclesia::CreateServer(0)),
      mockup_server_(mockup_shar, mockup_uds_path),
      redfish_intf_(mockup_server_.RedfishClientInterface()) {
  auto handler =
      [this](::tensorflow::serving::net_http::ServerRequestInterface *req) {
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        // Handler checks if we have any registered patches for the requested
        // URI. If yes, then return the patch. If not, then pass through the
        // response from the Redfish mockup.
        auto maybe_patch = this->GetPatch(req->uri_path());
        if (maybe_patch.has_value()) {
          req->WriteResponseBytes(maybe_patch->data(), maybe_patch->length());
        } else {
          req->WriteResponseString(
              redfish_intf_->GetUri(req->uri_path()).DebugString());
        }
        req->Reply();
      };
  proxy_server_->RegisterRequestDispatcher(
      [handler = std::move(handler)](
          ::tensorflow::serving::net_http::ServerRequestInterface *req)
          -> ::tensorflow::serving::net_http::RequestHandler {
        return handler;
      },
      ::tensorflow::serving::net_http::RequestHandlerOptions());
  ecclesia::Check(proxy_server_->StartAcceptingRequests(),
                  "can start the proxy server");
}

PatchableMockupServer::~PatchableMockupServer() {
  if (proxy_server_) {
    proxy_server_->Terminate();
    // As this server is used for testing, just arbitrarily choose 10 seconds
    // to be the upper limit for termination.
    proxy_server_->WaitForTerminationWithTimeout(absl::Seconds(10));
  }
}

}  // namespace ecclesia
