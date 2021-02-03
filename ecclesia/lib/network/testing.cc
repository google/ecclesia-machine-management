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

#include "ecclesia/lib/network/testing.h"

#include <resolv.h>
#include <sys/socket.h>
#include <unistd.h>

#include "ecclesia/lib/cleanup/cleanup.h"
#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/lib/logging/posix.h"

namespace ecclesia {

int FindUnusedPortOrDie() {
  // Open the socket as an IPv6 TCP socket.
  int fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
  if (fd < 0) {
    PosixFatalLog() << "socket() failed: ";
  }
  auto fd_closer = LambdaCleanup([fd]() { close(fd); });

  // Set the socket as SO_REUSEADDR so that if we hand this port back to a
  // caller as an available port they can use it immediately.
  int one = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) == -1) {
    PosixFatalLog() << "setsockopt() failed";
  }

  // Use an IPv6 sockaddr structure.
  struct sockaddr_in6 addr = {};
  auto *addr_ptr = reinterpret_cast<struct sockaddr *>(&addr);
  socklen_t addr_len = sizeof(addr);

  // Bind to port 0. This should randomly allocate an available port.
  addr.sin6_family = AF_INET6;
  addr.sin6_port = 0;
  if (bind(fd, addr_ptr, addr_len) == -1) {
    PosixFatalLog() << "bind() failed for port 0";
  }

  // Get the port number that we bound to.
  if (getsockname(fd, addr_ptr, &addr_len) == -1) {
    PosixFatalLog() << "getsockname() failed on the port we bound to";
  }
  Check(addr_len <= sizeof(addr),
        "getsockname() returned a valid address length");
  int port = ntohs(addr.sin6_port);
  Check(port > 0, "bound port is a real, positive port number");
  return port;
}

}  // namespace ecclesia
