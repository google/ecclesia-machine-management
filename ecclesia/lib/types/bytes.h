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

// This header provides constants for converting between different byte units.
#ifndef ECCLESIA_LIB_TYPES_BYTES_H_
#define ECCLESIA_LIB_TYPES_BYTES_H_

#include <cstdint>
#include <string>

namespace ecclesia {

inline constexpr uint64_t kBytesInKiB = 1 << 10;
inline constexpr uint64_t kBytesInMiB = 1 << 20;
inline constexpr uint64_t kBytesInGiB = 1 << 30;

inline constexpr uint64_t kBytesInKB = 1000;
inline constexpr uint64_t kBytesInMB = 1000 * kBytesInKB;
inline constexpr uint64_t kBytesInGB = 1000 * kBytesInMB;

inline uint64_t BytesToKiB(uint64_t val) { return val >> 10; }
inline uint64_t BytesToMiB(uint64_t val) { return val >> 20; }
inline uint64_t BytesToGiB(uint64_t val) { return val >> 30; }

inline uint64_t BytesToKB(uint64_t val) { return val / kBytesInKB; }
inline uint64_t BytesToMB(uint64_t val) { return val / kBytesInMB; }
inline uint64_t BytesToGB(uint64_t val) { return val / kBytesInGB; }

inline uint64_t KiBToBytes(uint64_t val) { return val << 10; }
inline uint64_t MiBToBytes(uint64_t val) { return val << 20; }
inline uint64_t GiBToBytes(uint64_t val) { return val << 30; }

inline uint64_t KBToBytes(uint64_t val) { return val * kBytesInKB; }
inline uint64_t MBToBytes(uint64_t val) { return val * kBytesInMB; }
inline uint64_t GBToBytes(uint64_t val) { return val * kBytesInGB; }

// Given a value in bytes, convert it to a human-readable string. This is a
// string matching the format "\d+[kMGT]" where the leading digits are the
// smallest valid whole number that works.
//
// The function also takes a multiplier argument, which will generally be either
// 1000 or 1024, which produces a powers-of-10 or powers-of-2 style output
// respectively.
std::string BytesToMetricPrefix(uint64_t bytes, int multiplier);
}  // namespace ecclesia

#endif  // ECCLESIA_LIB_TYPES_BYTES_H_
