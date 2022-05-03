/*
 * Copyright 2022 Google LLC
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

#include "ecclesia/lib/network/ip.h"

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

namespace ecclesia {

// Both of these functions are implemented by seeing if we can get address info
// for the standard IPv4 and IPv6 local addresses. If we can get info then it
// should be possible to bind to ports on localhost using that address family.
bool IsIpv4LocalhostAvailable() {
  struct addrinfo *addr;
  if (getaddrinfo("127.0.0.1", nullptr, nullptr, &addr) == 0) {
    freeaddrinfo(addr);
    return true;
  }
  return false;
}
bool IsIpv6LocalhostAvailable() {
  struct addrinfo *addr;
  if (getaddrinfo("::1", nullptr, nullptr, &addr) == 0) {
    freeaddrinfo(addr);
    return true;
  }
  return false;
}

}  // namespace ecclesia
