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

#include "absl/strings/str_replace.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/magent/redfish/core/redfish_keywords.h"

namespace ecclesia {

Assembly::AssemblyModifier CreateModifierToAssociatePcieFunction(
    const PciLocation location, const std::string assembly_uri,
    const std::string assembly_name, const std::string component_name) {
  return [location(std::move(location)), assembly_uri(std::move(assembly_uri)),
          assembly_name(std::move(assembly_name)),
          component_name(std::move(component_name))](
             absl::flat_hash_map<std::string, Json::Value> &assemblies) {
    InfoLog() << "Adding PCIeFunction to " << assembly_uri;
    if (auto iter = assemblies.find(assembly_uri); iter != assemblies.end()) {
      for (auto &assembly : iter->second[kAssemblies]) {
        if (assembly.isMember(kName) &&
            assembly[kName].asString() == assembly_name) {
          if (!assembly.isMember(kOem) || !assembly[kOem].isMember(kGoogle) ||
              !assembly[kOem][kGoogle].isMember(kComponents) ||
              assembly[kOem][kGoogle][kComponents].empty()) {
            ErrorLog() << "Assembly " << assembly_name
                       << " does not have components";
            return;
          }
          auto &components = assembly[kOem][kGoogle][kComponents];
          for (auto &component : components) {
            if (component.isMember(kName) &&
                component[kName].asString() == component_name) {
              // Construct the PCIeFunction URI from the BDF and Redfish Schema
              const std::string pcie_device_str = absl::StrFormat(
                  "%04x:%02x:%02x", location.domain().value(),
                  location.bus().value(), location.device().value());
              const std::string pcie_function_uri = absl::StrReplaceAll(
                  kPCIeFunctionUriPattern,
                  {{"([\\w:]+)", pcie_device_str},
                   {"(\\d+)", absl::StrCat(location.function().value())}});

              Json::Value associated_with;
              associated_with[kOdataId] = pcie_function_uri;
              component[kAssociatedWith].append(associated_with);
              return;
            }
          }
          ErrorLog() << "Failed to find component " << component_name;
          return;
        }
      }
      ErrorLog() << "Failed to find assembly " << assembly_name;
      return;
    }
    ErrorLog() << "Failed to find Assembly at URI " << assembly_uri;
  };
}

}  // namespace ecclesia
