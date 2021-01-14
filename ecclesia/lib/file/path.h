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

// Various utility functions for manipulating filesystem paths. Assumes that
// you're working on a *nix system where the directory separator is "/".

#ifndef ECCLESIA_LIB_FILE_PATH_H_
#define ECCLESIA_LIB_FILE_PATH_H_

#include <string>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"

namespace ecclesia {

// Given a path return the basename or dirname of the path. The returned view
// will be a substring of the given path.
absl::string_view GetBasename(absl::string_view path);
absl::string_view GetDirname(absl::string_view path);

// Function that will take a sequence of paths and join them all together. This
// accepts both relative and absolute paths, but when an absolute path is passed
// in then it will "override" any arguments that came before it.
//
// We provide two overloads: one that takes a single span of string views, and
// one that takes a variadic set of string view arguments. The actual
// implemenation uses the span, but in user code being able to pass in a bunch
// of string views is very convenient in the API.
std::string JoinFilePaths(absl::Span<const absl::string_view> paths);

template <typename... Args>
std::string JoinFilePaths(absl::string_view base, Args... args) {
  // Stuff all of the arguments into a span and then call span version.
  absl::string_view paths_array[] = {base, args...};
  return JoinFilePaths(absl::MakeConstSpan(paths_array));
}

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_FILE_PATH_H_
