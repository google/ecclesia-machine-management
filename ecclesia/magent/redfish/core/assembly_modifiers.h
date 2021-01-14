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

#ifndef ECCLESIA_MAGENT_REDFISH_CORE_ASSEMBLY_MODIFIERS_H_
#define ECCLESIA_MAGENT_REDFISH_CORE_ASSEMBLY_MODIFIERS_H_

#include <string>

#include "ecclesia/lib/io/pci/location.h"
#include "ecclesia/magent/redfish/core/assembly.h"

namespace ecclesia {

// A helper function to create an AssemblyModifier that can be used to attach a
// PCIeFunction to an Assembly described by assembly_name and component_name if
// it exists.
Assembly::AssemblyModifier CreateModifierToAssociatePcieFunction(
    const PciLocation location, const std::string assembly_uri,
    const std::string assembly_name, const std::string component_name);

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_REDFISH_CORE_ASSEMBLY_MODIFIERS_H_
