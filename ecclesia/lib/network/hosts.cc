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

#include "ecclesia/lib/network/hosts.h"

#include <cstddef>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"

namespace ecclesia {

bool NameHasEtcHostsEntry(absl::string_view hostname, absl::string_view path) {
  // Open up the hosts file. If we can't open it then fail immediately.
  std::ifstream etc_hosts((std::string(path)));
  if (!etc_hosts.good()) return false;

  // Process lines one at a time until we find a match for hostname.
  std::string line;
  while (std::getline(etc_hosts, line)) {
    // Lines can use # for comments so strip off anything after a #.
    std::pair<absl::string_view, absl::string_view> parts =
        absl::StrSplit(line, absl::MaxSplits('#', 1));
    absl::string_view true_line = parts.first;
    // Break the line up on whitespace. The first entry is an IP address and the
    // rest of the entries will be the names mapped onto it.
    std::vector<absl::string_view> names = absl::StrSplit(
        true_line, absl::ByAnyChar(" \t\r\n"), absl::SkipEmpty());
    for (size_t i = 1; i < names.size(); ++i) {
      if (names[i] == hostname) return true;
    }
  }

  // If we get here, then we had no entries that matched hostname.
  return false;
}

}  // namespace ecclesia
