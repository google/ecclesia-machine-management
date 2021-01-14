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

#ifndef ECCLESIA_LIB_SMBIOS_PLATFORM_TRANSLATOR_H_
#define ECCLESIA_LIB_SMBIOS_PLATFORM_TRANSLATOR_H_

#include <string>

#include "absl/strings/string_view.h"

namespace ecclesia {

// The format of some of the strings in smbios structures can be platform
// specific. This helper class centralizes any translations that need to be
// performed on smbios fields.
class SmbiosFieldTranslator {
 public:
  SmbiosFieldTranslator() {}
  virtual ~SmbiosFieldTranslator() {}
  // Given a device_locator string from Memory Device (Type 17) structure,
  // return a Dimm Slot name preferred by the platform
  virtual std::string GetDimmSlotName(
      absl::string_view device_locator) const = 0;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_SMBIOS_PLATFORM_TRANSLATOR_H_
