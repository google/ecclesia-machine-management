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

#include "ecclesia/lib/file/dir.h"

#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#include <stack>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/file/path.h"

namespace ecclesia {

absl::Status MakeDirectories(absl::string_view dirname) {
  std::stack<std::string> missing_dirs;

  // Keep walking up the tree until we hit the root.
  while (dirname != "/" && dirname != "") {
    std::string path(dirname);
    if (access(path.c_str(), F_OK) == 0) {
      break;
    } else {
      missing_dirs.push(std::move(path));
      dirname = GetDirname(dirname);
    }
  }

  // Try and create all of the components that are missing.
  while (!missing_dirs.empty()) {
    std::string path = std::move(missing_dirs.top());
    missing_dirs.pop();
    if (mkdir(path.c_str(), 0700) < 0) {
      return absl::UnknownError(absl::StrFormat(
          "unable to create directory %s, mkdir returned errno=%d", path,
          errno));
    }
  }

  // If we get here then every directory was created.
  return absl::OkStatus();
}

}  // namespace ecclesia
