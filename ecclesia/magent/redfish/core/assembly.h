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

#ifndef ECCLESIA_MAGENT_REDFISH_CORE_ASSEMBLY_H_
#define ECCLESIA_MAGENT_REDFISH_CORE_ASSEMBLY_H_

#include <functional>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "ecclesia/magent/redfish/core/resource.h"
#include "json/value.h"
#include "tensorflow_serving/util/net_http/server/public/httpserver_interface.h"
#include "tensorflow_serving/util/net_http/server/public/response_code_enum.h"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"

namespace ecclesia {

// This is a catch all assembly resource. Any URI for an assembly resource is
// handled by this class. The constructor takes in a path to a directory
// containing JSON files for any assembly we wish to provide via the redfish
// data model. This is not an ideal implementation, since the logic here just
// serves up the assemblies without checking for whether the corresponding
// component is present or not. However, a redfish compliant client that
// discovers assemblies by walking through the redfish tree will only discover
// valid assemblies.
class Assembly : public Resource {
 public:
  // The modifier can be used to change the static JSON data loaded from files.
  // For example, adding some dynamic data like part and serial numbers, or
  // modifying the assemblies based on some conditions like a plugin is detected
  // or not. Please note the modifier can make any change without restriction.
  // It's caller's responsibility to ensure the modifier makes the change as
  // expected.
  using AssemblyModifier =
      std::function<void(absl::flat_hash_map<std::string, Json::Value> &)>;

  // This constructor simply loads all the JSON files in the assemblies_dir
  // without making any change. The static content in the JSON files will be
  // responded if a request URL is matched.
  explicit Assembly(absl::string_view assemblies_dir)
      : Assembly(assemblies_dir, {}) {}

  // This constructor loads all the JSON files in the assemblies_dir and then
  // applies the given modifiers one by one to the loaded assemblies content.
  Assembly(absl::string_view assemblies_dir,
           std::vector<AssemblyModifier> assembly_modifiers);

  void RegisterRequestHandler(
      tensorflow::serving::net_http::HTTPServerInterface *server) override;

 private:
  void Get(tensorflow::serving::net_http::ServerRequestInterface *req,
           const ParamsType &) override {
    if (assemblies_.contains(req->uri_path())) {
      JSONResponseOK(assemblies_.at(req->uri_path()), req);
    } else {
      req->ReplyWithStatus(
          tensorflow::serving::net_http::HTTPStatusCode::NOT_FOUND);
    }
  }

  // Maintain a map of Assembly URI to the json response for the corresponding
  // assembly resource
  const absl::flat_hash_map<std::string, Json::Value> assemblies_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_REDFISH_CORE_ASSEMBLY_H_
