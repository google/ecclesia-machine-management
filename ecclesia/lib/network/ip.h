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

// Generic utility functions for interacting with IPv4 and IPv6.

#ifndef ECCLESIA_LIB_NETWORK_IP_H_
#define ECCLESIA_LIB_NETWORK_IP_H_

namespace ecclesia {

// Functions for testing if IPv4 or IPv6 support is available locally. Note that
// these functions only test if local support is available, which just means
// that you can bind to localhost. It does not test if there's any actual
// network connectivity available over the protocol.
bool IsIpv4LocalhostAvailable();
bool IsIpv6LocalhostAvailable();

}

#endif  // ECCLESIA_LIB_NETWORK_IP_H_
