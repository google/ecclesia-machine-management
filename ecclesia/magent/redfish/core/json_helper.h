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
#include <string>

#include "ecclesia/magent/redfish/core/redfish_keywords.h"
#include "json/value.h"

namespace ecclesia {

// Helper functions to manipulate Json objects

// Create and return an empty json object with the given name nested inside
// the input json object
inline Json::Value *GetJsonObject(Json::Value *json, const std::string &name) {
  assert(json && json->isObject());
  return &((*json)[name] = Json::Value(Json::objectValue));
}

// Create and return an empty json array with the given name nested inside
// the input json object
inline Json::Value *GetJsonArray(Json::Value *json, const std::string &name) {
  assert(json && json->isObject());
  return &((*json)[name] = Json::Value(Json::arrayValue));
}

// Given an array instance for the "Members" array, append a collection member
inline void AppendCollectionMember(Json::Value *array, const std::string &uri) {
  if (!array->isArray()) return;
  Json::Value member;
  member[kOdataId] = uri;
  array->append(member);
}

}  // namespace ecclesia
#endif  // ECCLESIA_MAGENT_REDFISH_CORE_JSON_HELPER_H_
