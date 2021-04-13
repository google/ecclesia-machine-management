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
inline constexpr uint32_t kMsrIa32FeatureControl = 0x3A;
inline constexpr uint32_t kMsrIa32PpinCtl = 0x4E;
inline constexpr uint32_t kMsrIa32Ppin = 0x4F;
inline constexpr uint32_t kMsrIa32PlatformInfo = 0xCE;
inline constexpr uint32_t kMsrIa32Mperf = 0xE7;
inline constexpr uint32_t kMsrIa32Aperf = 0xE8;
inline constexpr uint32_t kMsrIa32PerfSts = 0x198;
inline constexpr uint32_t kMsrIa32PerfCtl = 0x199;
inline constexpr uint32_t kMsrIa32ThermStatus = 0x19C;
inline constexpr uint32_t kMsrIa32MiscEnables = 0x1A0;
inline constexpr uint32_t kMsrIa32TurboRatioLimit = 0x1AD;
inline constexpr uint32_t kMsrIa32TurboRatioLimit1 = 0x1AE;
inline constexpr uint32_t kMsrIa32PackageThermStatus = 0x1B1;
inline constexpr uint32_t kMsrIa32VrMiscConfig = 0x603;
inline constexpr uint32_t kMsrIa32RaplPowerUint = 0x606;
inline constexpr uint32_t kMsrIa32PkgPowerInfo = 0x614;
inline constexpr uint32_t kMsrIa32PkgPowerLimit = 0x610;
inline constexpr uint32_t kMsrIa32EnergyStatus = 0x611;
inline constexpr uint32_t kMsrAmdPatchLevel = 0x8B;
inline constexpr uint32_t kMsrAmdHwcr = 0xC0010015;
inline constexpr uint32_t kMsrUncoreRatioLimit = 0x620;
inline constexpr uint32_t kMsrUncorePerfStatus = 0x621;

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_IO_CONST_H_
