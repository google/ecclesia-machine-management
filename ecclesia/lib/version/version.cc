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

#include "ecclesia/lib/version/version.h"

#include "absl/strings/string_view.h"

namespace ecclesia {
// macro for stringifying BUILD_VERSION
#define STRINGIFYX(x) #x
#define ECCLESIA_BUILD_VERSION(x) STRINGIFYX(x)

absl::string_view GetBuildVersion() {
#ifdef BUILD_VERSION
  static constexpr absl::string_view kBuildVersion =
      ECCLESIA_BUILD_VERSION(BUILD_VERSION);
#else
  static constexpr absl::string_view kBuildVersion = "(unknown)";
#endif

  return kBuildVersion;
}

#undef STRINGIFYX
#undef BUILD_VERSION
}  // namespace ecclesia
