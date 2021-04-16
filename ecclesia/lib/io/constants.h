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

#ifndef ECCLESIA_LIB_IO_CONST_H_
#define ECCLESIA_LIB_IO_CONST_H_

#include <cstdint>

namespace ecclesia {

// Constants for MSR numbers.
inline constexpr uint32_t kMsrIa32PpinCtl = 0x4E;
inline constexpr uint32_t kMsrIa32Ppin = 0x4F;
inline constexpr uint32_t kMsrIa32PlatformInfo = 0xCE;
inline constexpr uint32_t kMsrIa32PackageThermStatus = 0x1B1;

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_IO_CONST_H_
