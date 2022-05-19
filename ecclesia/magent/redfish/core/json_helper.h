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

#ifndef ECCLESIA_MAGENT_REDFISH_CORE_JSON_HELPER_H_
#define ECCLESIA_MAGENT_REDFISH_CORE_JSON_HELPER_H_

#include <cassert>
#include <initializer_list>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "ecclesia/magent/redfish/core/redfish_keywords.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {

// Helper functions to manipulate Json objects

// Create and return an empty json object with the given name nested inside
// the input json object
inline nlohmann::json *GetJsonObject(nlohmann::json *json,
                                     const std::string &name) {
  assert(json && json->is_object());
  return &((*json)[name] = nlohmann::json::object());
}

// Create and return an empty json array with the given name nested inside
// the input json object
inline nlohmann::json *GetJsonArray(nlohmann::json *json,
                                    const std::string &name) {
  assert(json && json->is_object());
  return &((*json)[name] = nlohmann::json::array());
}

// Given an array instance for the "Members" array, append a collection member
inline void AppendCollectionMember(nlohmann::json *array,
                                   const std::string &uri) {
  if (!array->is_array()) return;
  nlohmann::json member{{kOdataId, uri}};
  array->push_back(member);
}

// Drills down a nested JSON object, using a serious of keys. If any of the
// drill-down steps fails, returns a status.
inline absl::StatusOr<nlohmann::json *> JsonDrillDown(
    nlohmann::json &data, const std::initializer_list<absl::string_view> keys) {
  nlohmann::json *node = &data;
  for (absl::string_view key : keys) {
    auto node_iter = node->find(key);
    if (node_iter == node->end()) {
      return absl::Status(absl::StatusCode::kNotFound,
                          absl::StrCat("Key “", key, "” not found"));
    } else {
      node = &(*node_iter);
    }
  }
  return node;
}

}  // namespace ecclesia
#endif  // ECCLESIA_MAGENT_REDFISH_CORE_JSON_HELPER_H_
