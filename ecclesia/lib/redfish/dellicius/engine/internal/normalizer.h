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

#ifndef ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_INTERNAL_NORMALIZER_H_
#define ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_INTERNAL_NORMALIZER_H_

#include <memory>
#include <utility>

#include "absl/status/statusor.h"
#include "ecclesia/lib/redfish/dellicius/engine/internal/interface.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/topology.h"

namespace ecclesia {

// Populates the Subquery output using property requirements in the subquery.
class DefaultNormalizer final : public Normalizer {
 public:
  // with fallback to default CSDL bundle.

  absl::StatusOr<SubqueryDataSet> Normalize(
      const RedfishVariant &var,
      const DelliciusQuery::Subquery &subquery) const override;
};

// Decorates provided concrete Normalizer to add devpath to subquery output.
class NormalizerDevpathDecorator final : public Normalizer {
 public:
  NormalizerDevpathDecorator(
      std::unique_ptr<Normalizer> default_normalizer, RedfishInterface *service)
          : default_normalizer_(std::move(default_normalizer)),
            topology_(CreateTopologyFromRedfish(service)) {}

  absl::StatusOr<SubqueryDataSet> Normalize(
      const RedfishVariant &var,
      const DelliciusQuery::Subquery &subquery) const override;
 private:
  std::unique_ptr<Normalizer> default_normalizer_;
  NodeTopology topology_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_DELLICIUS_ENGINE_INTERNAL_NORMALIZER_H_
