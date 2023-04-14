/*
 * Copyright 2023 Google LLC
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

#ifndef ECCLESIA_LIB_REDFISH_DELLICIUS_TOOLS_TRANSPORT_CACHE_FLAGS_H_
#define ECCLESIA_LIB_REDFISH_DELLICIUS_TOOLS_TRANSPORT_CACHE_FLAGS_H_

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "re2/re2.h"

namespace ecclesia {

struct CachePolicy {
  enum class CachePolicytType { kNoCache, kTimeBasedCache };
  CachePolicytType type;
  std::optional<int64_t> cache_duration_seconds;
};

// AbslUnparseFlag converts from a CachePolicy to string.
inline std::string AbslUnparseFlag(const CachePolicy redfish_cache_policy) {
  if (redfish_cache_policy.type ==
          CachePolicy::CachePolicytType::kTimeBasedCache &&
      redfish_cache_policy.cache_duration_seconds.has_value()) {
    return absl::StrCat("time_based:",
                        *redfish_cache_policy.cache_duration_seconds);
  }
  return "no_cache";
}

// AbslParseFlag converts from a string to CachePolicy.
inline bool AbslParseFlag(absl::string_view text,
                          CachePolicy *redfish_cache_policy,
                          std::string *error) {
  static constexpr LazyRE2 kRedfishCachePolicyRegex = {
      R"(^(\w+)(?:\:(\d+))?$)"};
  if (text.empty()) {
    return true;
  }

  std::string cache_type, parsed_cache_duration;
  int64_t cache_duration_seconds;
  if (!RE2::FullMatch(text, *kRedfishCachePolicyRegex, &cache_type,
                      &parsed_cache_duration)) {
    *error += absl::StrFormat(
        "Failed to parse string \"%s\" in flag "
        "\"cache_policy\". "
        "Please make sure it's formatted as  "
        "<cache_type>:<cache_duration_seconds>.",
        std::string(text));
    return false;
  }
  if (cache_type == "time_based" &&
      absl::SimpleAtoi(parsed_cache_duration, &cache_duration_seconds)) {
    redfish_cache_policy->type = CachePolicy::CachePolicytType::kTimeBasedCache;
    redfish_cache_policy->cache_duration_seconds = cache_duration_seconds;
  } else {
    redfish_cache_policy->type = CachePolicy::CachePolicytType::kNoCache;
  }
  return true;
}

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_DELLICIUS_TOOLS_TRANSPORT_CACHE_H_
