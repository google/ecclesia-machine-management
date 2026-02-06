/*
 * Copyright 2026 Google LLC
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


#include "ecclesia/lib/redfish/redpath/engine/resource_identifier/extractors/related_item.h"

#include <memory>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/property_definitions.h"
#include "ecclesia/lib/redfish/redpath/engine/resource_identifier/extractor.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {

absl::StatusOr<IdentifierExtractorIntf::ResourceIdentifier>
RelatedItemIdentifierExtractor::Extract(const RedfishObject& obj,
                                        const Deps& deps) const {
  if (deps.interface == nullptr) {
    return absl::InvalidArgumentError("Redfish interface is null");
  }
  nlohmann::json json = obj.GetContentAsJson();
  auto related_item_it = json.find(kRfPropertyRelatedItem);
  if (related_item_it == json.end() || !related_item_it->is_array() ||
      related_item_it->empty()) {
    return absl::NotFoundError("RelatedItem not found in query result data");
  }

  for (const auto& related_item : *related_item_it) {
    auto odata_id_it = related_item.find(PropertyOdataId::Name);
    if (odata_id_it == related_item.end() || !odata_id_it->is_string()) {
      continue;
    }
    std::string related_item_uri = odata_id_it->get<std::string>();
    // It's ok to use optional freshness here because we are only interested in
    // the identifier of the related item which is not expected to change once
    // created.
    GetParams params{.freshness = GetParams::Freshness::kOptional};
    if (deps.timeout_manager != nullptr) {
      params.timeout_manager = deps.timeout_manager;
    }
    RedfishVariant variant =
        deps.interface->CachedGetUri(related_item_uri, params);
    if (!variant.status().ok()) {
      continue;
    }
    std::unique_ptr<RedfishObject> related_item_obj = variant.AsObject();
    if (related_item_obj == nullptr) {
      return absl::FailedPreconditionError(absl::StrCat(
          "Related item object is null for URI: ", related_item_uri));
    }

    absl::StatusOr<IdentifierExtractorIntf::ResourceIdentifier>
        related_item_identifier =
            default_identifier_extractor_.Extract(*related_item_obj, deps);
    if (related_item_identifier.ok()) {
      return related_item_identifier;
    }
  }

  return absl::NotFoundError("Identifier not found in related item");
}
}  // namespace ecclesia
