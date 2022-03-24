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

#include "ecclesia/lib/redfish/interface.h"

namespace ecclesia {

RedfishQueryParamExpand::RedfishQueryParamExpand(
    RedfishQueryParamExpand::Params params)
    : type_(params.type), levels_(params.levels) {}

std::string RedfishQueryParamExpand::ToString() const {
  std::string expand_type;
  switch (type_) {
    case ExpandType::kAll:
      expand_type = "*";
      break;
    case ExpandType::kNoResources:
      expand_type = ".";
      break;
    case ExpandType::kResourcesOnly:
      expand_type = "~";
      break;
  }
  return absl::StrCat("$expand=", expand_type, "($levels=", levels_, ")");
}

std::unique_ptr<RedfishObject> RedfishVariant::AsFreshObject() const {
  if (!ptr_) return nullptr;
  std::unique_ptr<RedfishObject> obj = ptr_->AsObject();
  if (!obj) return nullptr;
  return obj->EnsureFreshPayload();
}

}  // namespace ecclesia
