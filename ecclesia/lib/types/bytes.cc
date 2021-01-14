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

#include "absl/strings/str_cat.h"
#include "ecclesia/lib/types/bytes.h"

namespace ecclesia {
namespace {

// The suffix values ("metric prefixes") to append to data size strings,
// in order from smallest to largest. We use array syntax here and not string
// syntax because we don't want the array to include a trailing NUL.
constexpr char kSuffixValues[] = {'k', 'M', 'G', 'T'};
}  // namespace

std::string BytesToMetricPrefix(uint64_t bytes, int multiplier) {
  std::string suffix;
  // Keep picking a larger suffix until the multiplier no longer divides evenly.
  for (char next_suffix : kSuffixValues) {
    if (bytes == 0 || bytes % multiplier != 0) break;
    suffix = next_suffix;
    bytes /= multiplier;
  }
  return absl::StrCat(bytes, suffix);
}
}  // namespace ecclesia
