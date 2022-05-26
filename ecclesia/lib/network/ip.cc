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

#include "absl/cleanup/cleanup.h"

namespace ecclesia {
namespace {

// The underlying implementation of the IsIpvX functions. We implement the check
// getting all the address info for "localhost" and seeing if it supports the
// address family in question.
bool DoesLocalhostContainFamily(int addr_family) {
  struct addrinfo *addr;
  if (getaddrinfo("localhost", nullptr, nullptr, &addr) != 0) {
    // If we can't get localhost info at all, return false.
    return false;
  }
  // Now search through the linked list of info for anything with the matching
  // family. None of the other values matter.
  absl::Cleanup free_info_when_done = [addr]() { freeaddrinfo(addr); };
  while (addr) {
    if (addr->ai_family == addr_family) {
      return true;
    }
    addr = addr->ai_next;
  }
  return false;  // Getting here means address family not found.
}

}  // namespace

bool IsIpv4LocalhostAvailable() { return DoesLocalhostContainFamily(AF_INET); }
bool IsIpv6LocalhostAvailable() { return DoesLocalhostContainFamily(AF_INET6); }

}  // namespace ecclesia
