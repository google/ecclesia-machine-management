/*
 * Copyright 2022 Google LLC
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

#ifndef ECCLESIA_LIB_REDFISH_STATUS_H_
#define ECCLESIA_LIB_REDFISH_STATUS_H_

#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"

namespace ecclesia {

// Helper functions standardizing status messages for frequently-encountered
// errors.
inline std::string UnableToGetAsFreshObjectMessage(
    std::optional<absl::string_view> uri) {
  if (!uri.has_value()) {
    return absl::StrCat("Unable to get fresh object for unspecified URI.");
  }
  return absl::StrCat("Unable to get '", *uri, "' as fresh object.");
}
inline std::string UnableToGetAsObjectMessage(absl::string_view property_name) {
  return absl::StrCat("Unable to get '", property_name, "' as object.");
}
inline std::string UnableToGetPropertyMessage(absl::string_view property_name) {
  return absl::StrCat("Unable to get '", property_name, "' property.");
}
template <typename PropertyDefinitionT>
inline std::string UnableToGetPropertyMessage() {
  return UnableToGetPropertyMessage(PropertyDefinitionT::Name);
}

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_STATUS_H_
