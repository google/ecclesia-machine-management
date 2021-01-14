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

// Magent version is passed down as a define from the build command.
// --copt=-DBUILD_VERSION=xx.yy.zz
// ex. blaze build :magent_indus_oss --copt=-DMAGENT_VERSION=6.50.1

#ifndef ECCLESIA_LIB_VERSION_VERSION_H_
#define ECCLESIA_LIB_VERSION_VERSION_H_

#include "absl/strings/string_view.h"

namespace ecclesia {

absl::string_view GetBuildVersion();

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_VERSION_VERSION_H_
