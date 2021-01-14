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

// This library provides simple functions to parse certain packed text encodings
// which appear in various low-level data structures.
//
// Both BCD PLUS and 6-bit packed ASCII here are expected to be as described in
// the IMPI/Intel FRU information storage for IPMI standard. See revision 1.2
// at:
// https://www.intel.com/content/www/us/en/servers/ipmi/information-storage-definition-rev-1-2.html

#ifndef ECCLESIA_LIB_CODEC_DECODE_H_
#define ECCLESIA_LIB_CODEC_DECODE_H_

#include <string>

#include "absl/status/statusor.h"
#include "absl/types/span.h"

namespace ecclesia {

// Parse a given BCD Plus encoded byte string from the provided span of bytes.
absl::StatusOr<std::string> ParseBcdPlus(absl::Span<const unsigned char> bytes);

// Given a span of bytes, parses those bytes into a string, interpreting as six
// bit ascii as described in the standards mentioned above.
absl::StatusOr<std::string> ParseSixBitAscii(
    absl::Span<const unsigned char> bytes);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_CODEC_DECODE_H_
