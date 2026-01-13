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

#ifndef ECCLESIA_LIB_REDFISH_REDPATH_ENGINE_RESOURCE_IDENTIFIER_EXTRACTOR_H_
#define ECCLESIA_LIB_REDFISH_REDPATH_ENGINE_RESOURCE_IDENTIFIER_EXTRACTOR_H_

#include <optional>

#include "absl/status/statusor.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/redpath/engine/normalizer.h"
#include "ecclesia/lib/redfish/timing/query_timeout_manager.h"

namespace ecclesia {

// Interface for extracting stable identifiers from Redfish resources.
class IdentifierExtractorIntf {
 public:
  struct Deps {
    RedfishInterface* interface;
    RedpathNormalizer& normalizer;
    const RedpathNormalizerOptions& options;
    QueryTimeoutManager* timeout_manager = nullptr;
  };

  struct ResourceIdentifier {
    std::optional<Identifier> identifier;
    std::optional<CollectedProperty::SensorIdentifier> sensor_identifier;
  };

  virtual ~IdentifierExtractorIntf() = default;

  // Extracts identifier from a given Redfish object.
  virtual absl::StatusOr<ResourceIdentifier> Extract(
      const RedfishObject& obj, const Deps& deps) const = 0;
};

class DefaultIdentifierExtractor : public IdentifierExtractorIntf {
  // Default implementation of IdentifierExtractorIntf that extracts the
  // identifier from the Redfish object using the OdataType property.
 public:
  absl::StatusOr<ResourceIdentifier> Extract(const RedfishObject& obj,
                                             const Deps& deps) const override;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_REDPATH_ENGINE_RESOURCE_IDENTIFIER_EXTRACTOR_H_
