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

#include <functional>
#include <optional>
#include <string>

#include "absl/strings/string_view.h"
#include "ecclesia/lib/file/path.h"
#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/lib/logging/posix.h"

namespace ecclesia {

bool IsSafeUnixDomainSocketRoot(const std::string &root_path) {
  // The only place we consider it to be safe to put socket directories is
  // in /var/run. This directory should always exist and be always be writable
  // only by root.
  return root_path == "/var/run";
}

bool SetUpUnixDomainSocket(
    const std::string &socket_path, const DomainSocketOwners &owners,
    const std::function<bool(const std::string &)> &is_root_safe) {
  // Construct the directory and root paths from the socket path. We store these
  // in a string instead of a string_view because we need to be able to convert
  // them into NUL-terminated strings to pass them into C APIs.
  std::string socket_directory(GetDirname(socket_path));
  std::string socket_root(GetDirname(socket_directory));

  // Fail immediately if the socket root is not safe.
  if (!is_root_safe(socket_root)) {
    ErrorLog() << "cannot set up domain sockets in " << socket_root
               << " because it is not a safe and secure location";
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
        PosixErrorLog() << "unable to lstat() " << socket_directory;
        return false;
      }
      if (!S_ISDIR(socket_dir_stat.st_mode)) {
        ErrorLog() << "socket directory " << socket_directory
                   << "is not a directory";
        return false;
      }
      if ((socket_dir_stat.st_mode & S_IRWXU) != S_IRWXU) {
        ErrorLog() << "socket directory " << socket_directory
                   << "does not have the correct permissions";
        return false;
      }
    } else {
      return false;
    }
  }

  // The socket directory exists, but it might not have the required owners.
  //
  // We could just unconditionally call chown with the desired owners but we
  // prefer to minimize extra writes at the expense of extra reads so we guard
  // the lookup with a stat check.
  struct stat socket_dir_stat;
  if (lstat(socket_directory.c_str(), &socket_dir_stat) != 0) {
    PosixErrorLog() << "unable to lstat() " << socket_directory;
    return false;
  }
  uid_t expected_uid = owners.uid ? *owners.uid : getuid();
  gid_t expected_gid = owners.gid ? *owners.gid : getgid();
  // If the directory isn't owned by the expected owners then change it.
  if (socket_dir_stat.st_uid != expected_uid ||
      socket_dir_stat.st_gid != expected_gid) {
    if (lchown(socket_directory.c_str(), expected_uid, expected_gid) != 0) {
      PosixErrorLog() << "unable to lchown() " << socket_directory;
      return false;
    }
  }

  // At this point we know the socket directory is ready. There may already be
  // an existing socket file, so we want to remove that. It's okay for the
  // remove to fail because the file already doesn't exist, but any other
  // failure is an error.
  if (unlink(socket_path.c_str()) != 0 && errno != ENOENT) {
    PosixErrorLog() << "unable to remove the existing socket " << socket_path;
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
  // It's okay for the remove to fail because the file already doesn't exist,
  // but any other failure is an error.
  return (unlink(socket_path.c_str()) == 0 || errno == ENOENT);
}

bool SetUnixDomainSocketOwnership(const std::string &socket_path,
                                  const DomainSocketOwners &owners) {
  // Check the existing ownership in order to avoid having to make a change if
  // we don't have to.
  struct stat socket_stat;
  if (lstat(socket_path.c_str(), &socket_stat) != 0) {
    PosixErrorLog() << "unable to lstat() " << socket_path;
    return false;
  }
  uid_t expected_uid = owners.uid ? *owners.uid : getuid();
  gid_t expected_gid = owners.gid ? *owners.gid : getgid();
  // If the existing ownership is not what we need, then try to change them.
  if (socket_stat.st_uid != expected_uid ||
      socket_stat.st_gid != expected_gid) {
    if (lchown(socket_path.c_str(), expected_uid, expected_gid) != 0) {
      PosixErrorLog() << "unable to lchown() " << socket_path;
      return false;
    }
  }
  // If we get here then either the socket already has the correct ownership or
  // we have changed it to the correct ownership.
  return true;
}

}  // namespace ecclesia
