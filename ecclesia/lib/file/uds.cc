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

#include "ecclesia/lib/file/uds.h"

#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#include <filesystem>
#include <functional>
#include <string>

namespace ecclesia {

bool IsSafeUnixDomainSocketRoot(const std::string &root_path) {
  // The only place we consider it to be safe to put socket directories is
  // in /var/run. This directory should always exist and be always be writable
  // only by root.
  return root_path == "/var/run";
}

bool SetUpUnixDomainSocket(
    const std::string &socket_path,
    const std::function<bool(const std::string &)> &is_root_safe) {
  // Construct the three paths we care about.
  std::filesystem::path socket(socket_path);
  std::filesystem::path socket_directory = socket.parent_path();
  std::filesystem::path socket_root = socket_directory.parent_path();

  // Fail immediately if the socket root is not safe.
  if (!is_root_safe(socket_root.string())) {
    return false;
  }

  // Create the socket directory. If it fails because a directory already
  // exists then that's okay as long as it has acceptable permissions. Any other
  // failure is an error.
  if (mkdir(socket_directory.c_str(), S_IRWXU) != 0) {
    if (errno == EEXIST) {
      // We're okay with the directory already existing, but if it does then we
      // need to make sure it's a directory with the right permissions. We use
      // lstat to make sure we don't follow a symlink to a directory.
      struct stat socket_dir_stat;
      if (lstat(socket_directory.c_str(), &socket_dir_stat) != 0) {
        return false;
      }
      if (!S_ISDIR(socket_dir_stat.st_mode)) {
        return false;
      }
      if ((socket_dir_stat.st_mode & S_IRWXU) != S_IRWXU) {
        return false;
      }
    } else {
      return false;
    }
  }

  // At this point we know the socket directory is ready. There may already be
  // an existing socket file, so we want to remove that. It's okay for the
  // remove to fail because the file already doesn't exist, but any other
  // failure is an error.
  if (unlink(socket.c_str()) != 0 && errno != ENOENT) {
    return false;
  }

  // At this point we know the socket directory is ready. There may already be
  // an existing socket file, so we want to remove that. It's okay for the
  // remove to fail because the file already doesn't exist, but any other
  // failure is an error.
  //
  // After this point, all validation has succeeded if the socket directory
  // exists and the socket path does not.
  return CleanUpUnixDomainSocket(socket_path);
}

bool CleanUpUnixDomainSocket(const std::string &socket_path) {
  std::filesystem::path socket(socket_path);
  // It's okay for the remove to fail because the file already doesn't exist,
  // but any other failure is an error.
  return (unlink(socket.c_str()) == 0 || errno == ENOENT);
}

}  // namespace ecclesia
