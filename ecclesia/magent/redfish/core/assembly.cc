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

#include "ecclesia/magent/redfish/core/assembly.h"

#include <fstream>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/file/dir.h"
#include "ecclesia/lib/file/path.h"
#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/magent/redfish/core/redfish_keywords.h"
#include "ecclesia/magent/redfish/core/resource.h"
#include "single_include/nlohmann/json.hpp"
#include "tensorflow_serving/util/net_http/server/public/httpserver_interface.h"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"
#include "re2/re2.h"

namespace ecclesia {

namespace {

// Parse a text JSON file and return a json object corresponding to the root
nlohmann::json ReadJsonFile(const std::string &file_path) {
  std::string file_contents;
  std::string line;
  std::ifstream file(file_path);
  if (!file.is_open()) return nlohmann::json();
  while (getline(file, line)) {
    file_contents.append(line);
  }
  file.close();
  nlohmann::json value = nlohmann::json::parse(file_contents, nullptr, false);
  if (value.is_discarded()) {
    return nlohmann::json();
  }
  return value;
}

// Given a path to a directory containing JSON files that represent assembly
// resources, this function loads up the static data into a dictionary and then
// applies the given modifiers to the dictionary.
absl::flat_hash_map<std::string, nlohmann::json> GetAssemblies(
    absl::string_view dir_path,
    std::vector<Assembly::AssemblyModifier> assembly_modifiers) {
  // First, load static JSON files into the assemblies which is an URL to JSON
  // value mapping.
  absl::flat_hash_map<std::string, nlohmann::json> assemblies;
  WithEachFileInDirectory(dir_path, [&](absl::string_view filename) {
    std::string file_path = JoinFilePaths(dir_path, filename);
    // Assemblies. Instead of blindly loading all the *.json files, it's
    // desirable to detect the present assemblies in the system and then load
    // the corresponding JSON file.
    if (absl::EndsWith(file_path, ".json")) {
      nlohmann::json root = ReadJsonFile(file_path);
      if (auto id = root.find(kOdataId); id != root.end()) {
        assemblies[id->get<std::string>()] = root;
      }
    }
  }).IgnoreError();
  // Second, modify the static assemblies loaded from the files.
  for (auto &modifier : assembly_modifiers) {
    if (auto status = modifier(assemblies); !status.ok()) {
      ErrorLog() << "Modifier failed with message: " << status.message();
    }
  }
  return assemblies;
}

}  // namespace

Assembly::Assembly(absl::string_view assemblies_dir,
                   std::vector<AssemblyModifier> assembly_modifiers)
    : Resource(kAssemblyUriPattern),
      assemblies_(
          GetAssemblies(assemblies_dir, std::move(assembly_modifiers))) {}

void Assembly::RegisterRequestHandler(
    tensorflow::serving::net_http::HTTPServerInterface *server) {
  server->RegisterRequestDispatcher(
      [this](
          tensorflow::serving::net_http::ServerRequestInterface *http_request)
          -> tensorflow::serving::net_http::RequestHandler {
        RE2 regex(this->Uri());
        if (RE2::FullMatch(http_request->uri_path(), regex)) {
          return
              [this](
                  tensorflow::serving::net_http::ServerRequestInterface *req) {
                this->RequestHandler(req);
              };
        }
        return nullptr;
       
      },
      tensorflow::serving::net_http::RequestHandlerOptions());
}

}  // namespace ecclesia
