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

#include "ecclesia/lib/file/path.h"

#include <cstddef>
#include <string>

#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"

namespace ecclesia {

absl::string_view GetBasename(absl::string_view path) {
  // First remove any trailing slashes in the path. If we have a path of the
  // form /dir1/dir2/dir3/ we consider the basename to be "dir3" and not an
  // empty string.
  while (path.size() > 1 && path.back() == '/') {
    path.remove_suffix(1);
  }
  // Remove everything before the last /.
  auto last_slash = path.find_last_of('/');
  if (last_slash != path.npos) {
    path.remove_prefix(last_slash + 1);
  }
  return path;
}

absl::string_view GetDirname(absl::string_view path) {
  // First remove any trailing slashes in the path. If we have a path of the
  // form /dir1/dir2/dir3/ we consider the dirname to be "/dir1/dir2" and
  // "/dir1/dir2/dir3".
  while (path.size() > 1 && path.back() == '/') {
    path.remove_suffix(1);
  }
  // Remove everything after the last /. If there isn't a slash then the dir is
  // the empty string.
  auto last_slash = path.find_last_of('/');
  if (last_slash != path.npos) {
    path.remove_suffix(path.size() - last_slash - 1);
  } else {
    path.remove_suffix(path.size());
  }
  // Remove any trailing slashes.
  while (path.size() > 1 && path.back() == '/') {
    path.remove_suffix(1);
  }
  return path;
}

std::string JoinFilePaths(absl::Span<const absl::string_view> paths) {
  // Find the last absolute path in the span (if there is one). We can then
  // ignore all the paths that come before it.
  for (size_t i = paths.size() - 1; i > 0; --i) {
    if (!paths[i].empty() && paths[i][0] == '/') {
      paths.remove_prefix(i);
      break;
    }
  }

  // Strip off any leading empty paths in the span. They don't impact the
  // final result and leading off with them complicates the joining process.
  while (!paths.empty()) {
    if (paths.front().empty()) {
      paths.remove_prefix(1);
    } else {
      break;
    }
  }

  // Combine all of the paths using a simple strjoin. Note that this may end
  // up inserting sequences of consecutive slashes in the path, but this will
  // be removed with a followup pass.
  std::string full_path = absl::StrJoin(paths, "/");
  if (full_path.empty()) return full_path;

  // Go through the the path and turn any consecutive slashes into a single
  // one.
  bool last_was_slash = (full_path[0] == '/');
  size_t final_size = 1;
  for (size_t i = 1; i < full_path.size(); ++i) {
    char next_char = full_path[i];
    if ((next_char != '/') || (next_char == '/' && !last_was_slash)) {
      full_path[final_size++] = next_char;
    }
    last_was_slash = (next_char == '/');
  }
  // If we ended with a trailing slash and aren't "/" then trim that as well.
  if (last_was_slash && final_size > 1) final_size -= 1;
  // Crop the string down to size and return it.
  full_path.resize(final_size);
  return full_path;
}

}  // namespace ecclesia
