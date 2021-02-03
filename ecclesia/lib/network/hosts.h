/*
 * Copyright 2021 Google LLC
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

// Functions for interacting with /etc/hosts. In general you do not want to
// interact with this file directly but in some contexts where you need to avoid
// depending on external services it is necessary to manually interact with it.

#ifndef ECCLESIA_LIB_NETWORK_HOSTS_H_
#define ECCLESIA_LIB_NETWORK_HOSTS_H_

#include "absl/strings/string_view.h"

namespace ecclesia {

// Check if the given hostname has an entry in an /etc/hosts file. This only
// counts precise matches against the hostname, it does not do anything special
// like adding a domain to it.
//
// The two-argument version allows you to specify the path. The one-argument
// version will default it to /etc/hosts.
bool NameHasEtcHostsEntry(absl::string_view hostname, absl::string_view path);
inline bool NameHasEtcHostsEntry(absl::string_view hostname) {
  return NameHasEtcHostsEntry(hostname, "/etc/hosts");
}

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_NETWORK_HOSTS_H_
