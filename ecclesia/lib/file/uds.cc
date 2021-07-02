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
#include <string>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
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

  // Create the socket directory. If it fails because a directory already exists
  // then that's okay as long as it has acceptable permissions (which we will
  // check afterwards). Any other failure is an error.
  if (mkdir(socket_directory.c_str(), S_IRWXU | S_IRWXG) != 0) {
    if (errno != EEXIST) {
      PosixErrorLog() << "unable to create the socket directory "
                      << socket_directory;
      return false;
    }
  }

  // Now that the socket directory exists, we need to check that it has the
  // correct permissions and ownership.
  struct stat socket_dir_stat;
  if (lstat(socket_directory.c_str(), &socket_dir_stat) != 0) {
    PosixErrorLog() << "unable to lstat() " << socket_directory;
    return false;
  }
  // First make sure it's actually a directory.
  if (!S_ISDIR(socket_dir_stat.st_mode)) {
    ErrorLog() << "socket directory " << socket_directory
               << "is not a directory";
    return false;
  }
  // Now, check if the directory has the correct permissions. If it does not
  // then try to change them.
  mode_t expected_perms = S_IRWXU | S_IRGRP | S_IXGRP;
  if ((socket_dir_stat.st_mode & ACCESSPERMS) != expected_perms) {
    if (chmod(socket_directory.c_str(),
              (socket_dir_stat.st_mode & ~ACCESSPERMS) | expected_perms) != 0) {
      PosixErrorLog() << "socket directory " << socket_directory
                      << "does not have the correct user permissions and "
                         "we cannot change them; chmod() failed";
      return false;
    }
  }
  // Finally, make sure that the directory has the required owners.
  uid_t expected_uid = owners.uid ? *owners.uid : getuid();
  gid_t expected_gid = owners.gid ? *owners.gid : getgid();
  if (socket_dir_stat.st_uid != expected_uid ||
      socket_dir_stat.st_gid != expected_gid) {
    if (lchown(socket_directory.c_str(), expected_uid, expected_gid) != 0) {
      PosixErrorLog() << "socket directory " << socket_directory
                      << " does not have the correct ownership and we cannot "
                         "change it; lchown() failed";
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
  // Check the existing permissions and ownership in order to avoid having to
  // make a change if we don't have to.
  struct stat socket_stat;
  if (lstat(socket_path.c_str(), &socket_stat) != 0) {
    PosixErrorLog() << "unable to lstat() " << socket_path;
    return false;
  }
  // First, check if the socket has the correct permissions. If it does not then
  // try to change them.
  mode_t expected_perms = S_IRWXU | S_IRWXG;
  if ((socket_stat.st_mode & ACCESSPERMS) != expected_perms) {
    if (chmod(socket_path.c_str(),
              (socket_stat.st_mode & ~ACCESSPERMS) | expected_perms) != 0) {
      PosixErrorLog() << "socket file " << socket_path
                      << "does not have the correct permissions and we cannot "
                         "change them; chmod() failed";
      return false;
    }
  }
  // Now, check if the socket has the specified ownership. Again, if it does not
  // then try to change it.
  uid_t expected_uid = owners.uid ? *owners.uid : getuid();
  gid_t expected_gid = owners.gid ? *owners.gid : getgid();
  if (socket_stat.st_uid != expected_uid ||
      socket_stat.st_gid != expected_gid) {
    if (lchown(socket_path.c_str(), expected_uid, expected_gid) != 0) {
      PosixErrorLog() << "socket file " << socket_path
                      << "does not have the correct ownership and we cannot "
                         "change it; lchown() failed";
      return false;
    }
  }
  // If we get here then either the socket had the correct permissions and
  // ownership already, or we were able to fix it so that it does.
  return true;
}

}  // namespace ecclesia
