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

#include "absl/log/log.h"
#include "ecclesia/lib/file/path.h"

namespace ecclesia {

bool IsSafeUnixDomainSocketRoot(const std::string &root_path) {
  // The only place we consider it to be safe to put socket directories is
  // in /var/run. This directory should always exist and be always be writable
  // only by root.
  return root_path == "/var/run";
}

bool SetUpUnixDomainSocket(
    const std::string &socket_path, DomainSocketPermissions permissions,
    const DomainSocketOwners &owners,
    const std::function<bool(const std::string &)> &is_root_safe) {
  // Construct the directory and root paths from the socket path. We store these
  // in a string instead of a string_view because we need to be able to convert
  // them into NULL-terminated strings to pass them into C APIs.
  std::string socket_directory(GetDirname(socket_path));
  std::string socket_root(GetDirname(socket_directory));

  // Fail immediately if the socket root is not safe.
  if (!is_root_safe(socket_root)) {
    LOG(ERROR) << "cannot set up domain sockets in " << socket_root
               << " because it is not a safe and secure location";
    return false;
  }

  // Compute the expected permissions field mask from the permissions enum.
  mode_t expected_perms;
  switch (permissions) {
    case DomainSocketPermissions::kUserOnly:
      expected_perms = S_IRWXU;
      break;
    case DomainSocketPermissions::kUserAndGroup:
      expected_perms = S_IRWXU | S_IRGRP | S_IXGRP;
      break;
    default:
      LOG(ERROR) << "unrecognized permissions enum value "
                 << static_cast<int>(permissions) << " (as int)";
      return false;
  }

  // Add in setuid or setgid permissions as well if the function specifies a
  // UID or GID other than the default. This is necessary to ensure that domain
  // sockets created in this directory get the same ownership as the directory.
  if (owners.uid) {
    expected_perms |= S_ISUID;
  }
  if (owners.gid) {
    expected_perms |= S_ISGID;
  }

  // Create the socket directory. If it fails because a directory already exists
  // then that's okay as long as it has acceptable permissions (which we will
  // check afterwards). Any other failure is an error.
  if (mkdir(socket_directory.c_str(), expected_perms) != 0) {
    if (errno != EEXIST) {
      PLOG(ERROR) << "unable to create the socket directory "
                  << socket_directory;
      return false;
    }
  }

  // Now that the socket directory exists, we need to check that it has the
  // correct permissions and ownership.
  struct stat socket_dir_stat;
  if (lstat(socket_directory.c_str(), &socket_dir_stat) != 0) {
    PLOG(ERROR) << "unable to lstat() " << socket_directory;
    return false;
  }
  // First make sure it's actually a directory.
  if (!S_ISDIR(socket_dir_stat.st_mode)) {
    LOG(ERROR) << "socket directory " << socket_directory
               << "is not a directory";
    return false;
  }
  // Now, check if the directory has the correct permissions. If it does not
  // then try to change them.
  if ((socket_dir_stat.st_mode & ALLPERMS) != expected_perms) {
    if (chmod(socket_directory.c_str(),
              (socket_dir_stat.st_mode & ~ALLPERMS) | expected_perms) != 0) {
      PLOG(ERROR) << "socket directory " << socket_directory
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
      PLOG(ERROR) << "socket directory " << socket_directory
                  << " does not have the correct ownership and we cannot "
                     "change it; lchown() failed";
      return false;
    }
  }

  // At this point we know the socket directory is ready. There may already be
  // an existing socket file, so we want to remove that. Validation is complete
  // once the socket directory exists and the socket path does not.
  return CleanUpUnixDomainSocket(socket_path);
}

bool CleanUpUnixDomainSocket(const std::string &socket_path) {
  if (unlink(socket_path.c_str()) != 0) {
    // Since the goal of this function is to remove existing sockets, we can
    // ignore errors that indicate the socket doesn't exist. Other errors should
    // be logged and return false.
    if (errno != ENOENT) {
      PLOG(ERROR) << "Unable to remove the existing socket " << socket_path;
      return false;
    }
  } else {
    // If the unlink succeeded, log a message indicating success for better
    // debuggability concerning sockets that persist between invocations.
    LOG(INFO) << "Successfully removed the existing socket " << socket_path;
  }
  return true;
}

}  // namespace ecclesia
