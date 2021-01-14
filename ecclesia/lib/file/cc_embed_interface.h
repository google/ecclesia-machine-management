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

// This library defines the standard data structured used by cc_embed to store
// arbitrary files directly in a C++ library.

#ifndef ECCLESIA_LIB_FILE_CC_EMBED_INTERFACE_H_
#define ECCLESIA_LIB_FILE_CC_EMBED_INTERFACE_H_

#include <array>
#include <cstddef>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/logging.h"

namespace ecclesia {

// The representation of a single data file. Files are represented as a single
// pair of strings: a name, with the name of the file, and data, the raw binary
// contents of the file.
//
// The name will be the base name of the file that the cc_embed binary read the
// underlying data from. This means that the name should always be a valid
// filename. It also means that name cannot be used to embed directory structure
// information.
//
// The data value is the raw byte contents of the file, without any encoding or
// compression. There is no constraint on the value of the stored bytes. In
// particular, one should not assume that data is a NUL-terminated string: if
// the embedded file is not text then it likely contains NUL characters.
struct EmbeddedFile {
  absl::string_view name;
  absl::string_view data;
};

// The cc_embed utility will embed multiple files into a single object by using
// an array of EmbeddedFile objects. The names of the individual files are
// guaranteed to be unique.
template <size_t N>
using EmbeddedFileArray = const std::array<EmbeddedFile, N>;

// Given an embedded file array, finds the data associated with the given name.
// There are two version of this function, one that returns nullopt if no file
// with the given name exists, and one that terminates with a fatal error.
//
// Note that these lookups do a simple linear search of the array. For common
// cases where the array contains only a single entry, or a very small number of
// entries, this is efficient. However, if your array contains a large number of
// entries you should load the references into a more complex data structure
// that supports more efficient lookups.
template <size_t N>
absl::optional<absl::string_view> GetEmbeddedFileWithName(
    absl::string_view name, const std::array<EmbeddedFile, N> &array) {
  for (const auto &entry : array) {
    if (entry.name == name) {
      return entry.data;
    }
  }
  return absl::nullopt;
}
template <size_t N>
absl::string_view GetEmbeddedFileWithNameOrDie(
    absl::string_view name, const std::array<EmbeddedFile, N> &array) {
  for (const auto &entry : array) {
    if (entry.name == name) {
      return entry.data;
    }
  }
  FatalLog() << "found no embedded file named: " << name;
}

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_FILE_CC_EMBED_INTERFACE_H_
