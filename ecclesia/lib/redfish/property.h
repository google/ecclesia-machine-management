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

#ifndef ECCLESIA_LIB_REDFISH_PROPERTY_H_
#define ECCLESIA_LIB_REDFISH_PROPERTY_H_

namespace libredfish {

// The base class for all property definitions. Subclasses must declare a type
// and a string Name.
template <typename PropertyDefinitionSubtypeT, typename PropertyType>
struct PropertyDefinition {
  using type = PropertyType;
};

// Macro for defining a Redfish property.
//
// This macro creates a PropertyDefinition subclass with the required static
// members to satisfy the CRTP expectations for new property definitions.
#define DEFINE_REDFISH_PROPERTY(classname, type, property_name) \
  struct classname : PropertyDefinition<classname, type> {      \
    static constexpr char Name[] = property_name;               \
  }

// Macro for defining a Redfish resource.
//
// This macro creates a ResourceDefinition subclass with the required static
// members to satisfy the static member expectations for resource definitions.
#define DEFINE_REDFISH_RESOURCE(classname, resource_name) \
  struct classname {                                      \
    static constexpr char Name[] = resource_name;         \
  }
}  // namespace libredfish

#endif  // ECCLESIA_LIB_REDFISH_PROPERTY_H_
