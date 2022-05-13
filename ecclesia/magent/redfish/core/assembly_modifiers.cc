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

#include "ecclesia/magent/redfish/core/assembly_modifiers.h"

#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/meta/type_traits.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "ecclesia/lib/io/pci/location.h"
#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/magent/redfish/core/assembly.h"
#include "ecclesia/magent/redfish/core/json_helper.h"
#include "ecclesia/magent/redfish/core/redfish_keywords.h"
#include "ecclesia/magent/redfish/core/uri_generation.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {

Assembly::AssemblyModifier CreateModifierToAssociatePcieFunction(
    const PciDbdfLocation location, const std::string assembly_uri,
    const std::string assembly_name, const std::string component_name) {
  return [location(std::move(location)), assembly_uri(std::move(assembly_uri)),
          assembly_name(std::move(assembly_name)),
          component_name(std::move(component_name))](
             absl::flat_hash_map<std::string, nlohmann::json> &assemblies) {
    InfoLog() << "Adding PCIeFunction to " << assembly_uri;
    if (auto iter = assemblies.find(assembly_uri); iter != assemblies.end()) {
      for (auto &assembly : iter->second[kAssemblies]) {
        if (auto name = assembly.find(kName);
            name->get<std::string>() == assembly_name) {
          if (auto node = JsonDrillDown(assembly, {kOem, kGoogle, kComponents});
              !node.ok() || (*node)->empty()) {
            return absl::NotFoundError(absl::StrFormat(
                "Assembly %s does not have components", assembly_name));
          }
          auto &components = assembly[kOem][kGoogle][kComponents];
          for (auto &component : components) {
            if (auto iter = component.find(kName);
                iter != component.end() &&
                iter->get<std::string>() == component_name) {
              nlohmann::json associated_with;
              associated_with[kOdataId] = GetPcieFunctionUri(location);
              component[kAssociatedWith].push_back(associated_with);
              return absl::OkStatus();
            }
          }
          return absl::NotFoundError(
              absl::StrFormat("Failed to find component %s", component_name));
        }
      }
      return absl::NotFoundError(
          absl::StrFormat("Failed to find assembly %s", assembly_name));
    }
    return absl::NotFoundError(
        absl::StrFormat("Failed to find Assembly at URI %s", assembly_uri));
  };
}

Assembly::AssemblyModifier CreateModifierToCreateComponent(
    const std::string assembly_uri, const std::string assembly_name,
    const std::string component_name) {
  return [assembly_uri(std::move(assembly_uri)),
          assembly_name(std::move(assembly_name)),
          component_name(std::move(component_name))](
             absl::flat_hash_map<std::string, nlohmann::json> &assemblies) {
    InfoLog() << "Adding component " << component_name
              << " to assembly: " << assembly_uri;
    auto iter = assemblies.find(assembly_uri);
    if (iter == assemblies.end() || !iter->second.contains(kAssemblies)) {
      return absl::NotFoundError(
          absl::StrFormat("Failed to find Assemblies at URI %s", assembly_uri));
    }
    for (auto &assembly : iter->second[kAssemblies]) {
      if (auto name = JsonDrillDown(assembly, {kName});
          name.ok() && (*name)->get<std::string>() == assembly_name) {
        auto inner_components =
            JsonDrillDown(assembly, {kOem, kGoogle, kComponents});
        if (!inner_components.ok()) {
          return absl::NotFoundError(absl::StrFormat(
              "Assembly %s does not have components", assembly_name));
        }
        auto *components = *inner_components;
        for (auto &component : *components) {
          if (auto iter = component.find(kName);
              iter != component.end() &&
              iter->get<std::string>() == component_name) {
            return absl::OkStatus();
          }
        }
        nlohmann::json component;
        component[kOdataId] =
            absl::StrCat(assembly[kOdataId].get<std::string>(), "/Components/",
                         components->size());
        component[kMemberId] = components->size();
        component[kName] = component_name;
        components->push_back(component);
        return absl::OkStatus();
      }
    }
    return absl::NotFoundError(
        absl::StrFormat("Failed to find assembly %s", assembly_name));
  };
}

}  // namespace ecclesia
