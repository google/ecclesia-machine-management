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

#include "ecclesia/lib/redfish/query.h"

#include <iostream>
#include <vector>

#include "absl/strings/str_split.h"
#include "re2/re2.h"

namespace ecclesia {
namespace {
constexpr absl::string_view kRedPathDelimiter = "*";
}  // namespace

void GetRedPath(RedfishInterface* intf, absl::string_view redpath,
                Sysmodel::ResultCallback result_callback) {
  if (redpath.empty()) return;
  std::vector<absl::string_view> split_redpath = absl::StrSplit(redpath, '/');
  auto root = intf->GetRoot();
  RedfishVariant::IndexHelper current = root.AsIndexHelper();
  // To match partial RedPath stored in split_redpath.
  static constexpr LazyRE2 kRedPathRegex = {
      "([0-9a-zA-Z]+)(\\[([a-zA-Z0-9]+|\\*)\\])*"};
  for (const auto& elem : split_redpath) {
    if (elem.empty()) continue;
    std::string path, subpath, subpath_content;
    if (!RE2::FullMatch(elem, *kRedPathRegex, &path, &subpath,
                        &subpath_content))
      return;
    current[path];
    if (subpath_content.empty()) break;
    // featureset is not implemented.
    if (subpath_content == kRedPathDelimiter) current.Each();
  }

  current.Do([&](std::unique_ptr<RedfishObject>& object) {
    return result_callback(std::move(object));
  });
}
}  // namespace ecclesia