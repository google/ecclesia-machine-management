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
#include <memory>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/types/optional.h"

namespace ecclesia {

RedfishQueryParamExpand::RedfishQueryParamExpand(
    RedfishQueryParamExpand::Params params)
    : type_(params.type), levels_(params.levels) {}

std::string RedfishQueryParamExpand::ToString() const {
  std::string expand_type;
  switch (type_) {
    case ExpandType::kBoth:
      expand_type = "*";
      break;
    case ExpandType::kNotLinks:
      expand_type = ".";
      break;
    case ExpandType::kLinks:
      expand_type = "~";
      break;
  }
  return absl::StrCat("$expand=", expand_type, "($levels=", levels_, ")");
}

absl::Status RedfishQueryParamExpand::ValidateRedfishSupport(
    const absl::optional<RedfishSupportedFeatures> &features) const {
  if (!features.has_value()) {
    return absl::InternalError("Expands are not supported.");
  }
  std::string expand_type;
  switch (type_) {
    case ExpandType::kBoth:
      if (!features->expand.expand_all) {
        return absl::InternalError("'expand_all' is not supported");
      }
      break;
    case ExpandType::kNotLinks:
      if (!features->expand.no_links) {
        return absl::InternalError("'no_links' is not supported");
      }
      break;
    case ExpandType::kLinks:
      if (!features->expand.links) {
        return absl::InternalError("'links' is not supported");
      }
      break;
  }
  if (levels_ > features->expand.max_levels) {
    return absl::InternalError(
        "number of levels exceed max levels set in redfish features");
  }
  return absl::OkStatus();
}
std::unique_ptr<RedfishObject> RedfishVariant::AsFreshObject() const {
  if (!ptr_) return nullptr;
  std::unique_ptr<RedfishObject> obj = ptr_->AsObject();
  if (!obj) return nullptr;
  return obj->EnsureFreshPayload().value_or(nullptr);
}

}  // namespace ecclesia
