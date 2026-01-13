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

#include "ecclesia/lib/redfish/redpath/engine/resource_identifier/extractor.h"

#include <optional>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/redpath/definitions/query_result/query_result.h"

namespace ecclesia {
absl::StatusOr<IdentifierExtractorIntf::ResourceIdentifier>
DefaultIdentifierExtractor::Extract(const RedfishObject& obj,
                                    const Deps& deps) const {
  DelliciusQuery::Subquery subquery;
  absl::StatusOr<QueryResultData> result =
      deps.normalizer.Normalize(obj, subquery, deps.options);
  if (!result.ok()) {
    std::string msg = "Failed to normalize object";
    if (std::optional<std::string> uri = obj.GetUriString(); uri.has_value()) {
      absl::StrAppend(&msg, ": ", *uri);
    }
    return absl::FailedPreconditionError(
        absl::StrCat(msg, " with status: ", result.status().message()));
  }

  auto it = result->fields().find(kIdentifierTag);
  if (it == result->fields().end() || !it->second.has_identifier()) {
    return absl::NotFoundError("Identifier not found in related item result");
  }

  return ResourceIdentifier{.identifier = it->second.identifier()};
}

}  // namespace ecclesia
