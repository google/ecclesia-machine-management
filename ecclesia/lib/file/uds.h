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

// Utilities for interacting with domain sockets in the filesystem.
//
// When working with paths for Unix domain sockets, we think of the path as
// having three components:
//   - the "socket", the filename of the actual socket
//   - the "socket directory", the name of the directory where the socket file
//     will be created
//   - the "socket root", the path of the directory that contains the socket
//     directory
// The full path is constructed from these three components by doing the
// substitution $SOCKET_ROOT/$SOCKET_DIR/$SOCKET.
//
// In general the expectation is that the socket root is a system directory
// presumed to always exist and whose permissions are valid. The socket and
// socket directory may have to be created on the fly, which is what these
// utilities help with.

#ifndef ECCLESIA_LIB_FILE_UDS_H_
#define ECCLESIA_LIB_FILE_UDS_H_

#include <sys/types.h>

#include <functional>
#include <string>

#include "absl/types/optional.h"

namespace ecclesia {

// Enumeration specifying the permissions that the socket should allow.
enum class DomainSocketPermissions {
  // The domain socket will only have user permissions (0700).
  kUserOnly,
  // The domain socket will have user and group permissions (0750).
  kUserAndGroup,
};

// Structure for (optionally) specifying UID and GID information in requests.
// This is used by functions that allow maniupulating the socket or directory
// ownership. A value being unspecified is interpreted as "don't change it".
struct DomainSocketOwners {
  absl::optional<uid_t> uid;
  absl::optional<gid_t> gid;
};

// Given a path to a socket root, return a bool indicating if this path is
// considered to be a safe one for creating socket directories.
bool IsSafeUnixDomainSocketRoot(const std::string &root_path);

// Given a path to a domain socket:
//   - verify that the socket root is a safe directory
//   - if the socket directory exists verify that it has safe permissions, or
//     create it with safe permissions if it does not exist
//   - remove the socket file if it already exists
// It will return true only if all of these steps succeed and the given path is
// now ready to use for a socket.
//
// Verifying the socket root safety is done using the given is_root_safe
// function. In general this should just be IsSafeUnixDomainSocketRoot but it
// can be useful to replace it in testing.
bool SetUpUnixDomainSocket(
    const std::string &socket_path, DomainSocketPermissions permissions,
    const DomainSocketOwners &owners,
    const std::function<bool(const std::string &)> &is_root_safe);

// Given a path to a domain socket, delete it.
// Assumes that the socket is no longer in use. This function should only
// be called after SetUpUnixDomainSocket has been called on the same socket
// prior in the process and that all services serving on the socket are shut
// down.
// Returns true if the socket was deleted successfully, false otherwise.
bool CleanUpUnixDomainSocket(const std::string &socket_path);

// Given a path to a domain socket, set the socket owners. This will also set
// the permissions to u+a,g+a as the socket ownership isn't much use without
// read and write permission for the user and group.
//
// Note that this socket must be in active use for this to be meaningful. You
// can't pre-create a file for the socket and change the permissions because you
// can't bind() over top of an existing file.
//
// A consquence of this is that you should never rely on this to lock down a
// socket with stricter ownership because you can't guarantee that no
// connections will be requested before the permission is applied.
bool SetUnixDomainSocketOwnership(const std::string &socket_path,
                                  const DomainSocketOwners &owners);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_FILE_UDS_H_
