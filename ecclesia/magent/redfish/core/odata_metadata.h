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

#ifndef ECCLESIA_MAGENT_REDFISH_CORE_METADATA_H_
#define ECCLESIA_MAGENT_REDFISH_CORE_METADATA_H_

#include <memory>
#include <string>

#include "absl/memory/memory.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "ecclesia/lib/apifs/apifs.h"
#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/magent/redfish/core/redfish_keywords.h"
#include "tensorflow_serving/util/net_http/server/public/httpserver_interface.h"
#include "tensorflow_serving/util/net_http/server/public/response_code_enum.h"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"

namespace ecclesia {

// (DSP0266, Section 8.4.1): "The OData metadata describes top-level service
// resources and resource types."
class ODataMetadata {
 public:
  // Initialize the csdl metadata at construction
  explicit ODataMetadata(absl::string_view odata_metadata_path) {
    ApifsFile apifs_file((std::string(odata_metadata_path)));
    absl::StatusOr<std::string> maybe_contents = apifs_file.Read();
    if (!maybe_contents.ok()) {
      ecclesia::ErrorLog() << "Failed to read OData metadata file.";
    } else {
      absl::StrAppend(&metadata_text_, *maybe_contents);
    }
  }

  // Register the ODataMetadata URI (redfish/v1/$metadata)
  void RegisterRequestHandler(
      tensorflow::serving::net_http::HTTPServerInterface *server) {
    tensorflow::serving::net_http::RequestHandlerOptions handler_options;
    server->RegisterRequestHandler(
        kODataMetadataUri,
        [this](tensorflow::serving::net_http::ServerRequestInterface *req) {
          return this->RequestHandler(req);
        },
        handler_options);
  }

  // Getter for metadata text
  const absl::string_view GetMetadata() const { return metadata_text_; }

 private:
  // Handle a request for the OData Metadata by replying with the XML contents
  // contained within metadata_text_
  void RequestHandler(
      tensorflow::serving::net_http::ServerRequestInterface *req) {
    if (req->http_method() == "GET") {
      tensorflow::serving::net_http::SetContentType(req,
                                                    "text/xml; charset=UTF-8");
      req->WriteResponseString(metadata_text_);
      req->ReplyWithStatus(tensorflow::serving::net_http::HTTPStatusCode::OK);
    } else {
      req->ReplyWithStatus(
          tensorflow::serving::net_http::HTTPStatusCode::METHOD_NA);
    }
  }

  std::string metadata_text_;
};

// Factory function to create a OData Metadata instance with request handler.
inline std::unique_ptr<ODataMetadata> CreateMetadata(
    tensorflow::serving::net_http::HTTPServerInterface *server,
    absl::string_view odata_metadata_path) {
  auto metadata = absl::make_unique<ODataMetadata>(odata_metadata_path);
  metadata->RegisterRequestHandler(server);
  return metadata;
}

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_REDFISH_CORE_METADATA_H_
